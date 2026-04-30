#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/PartHandler.h>
#include <Poco/Net/MessageHeader.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Array.h>
#include <Poco/StreamCopier.h>
#include <Poco/Process.h>
#include <Poco/Pipe.h>
#include <Poco/PipeStream.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/Thread.h>

#include <sqlite3.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <regex>
#include <cstdlib>

using namespace Poco::Net;
using namespace Poco::Util;
using namespace Poco::JSON;

// ===========================================================================
// Configuration
// ===========================================================================
static const unsigned short PORT      = 8080;
static const std::string    DB_PATH   = "mediacenter.db";
static const std::string    HLS_DIR   = "hls_cache";
static const std::string    THUMB_DIR = "thumbnails";
static const std::string    UPLOAD_DIR = "uploads";

// ===========================================================================
// Database manager
// ===========================================================================
class Database {
public:
    Database() : db_(nullptr) {}
    ~Database() { if (db_) sqlite3_close(db_); }

    bool open(const std::string& path) {
        int rc = sqlite3_open(path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::cerr << "[db] Failed to open: " << sqlite3_errmsg(db_) << "\n";
            return false;
        }
        sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        return createTables();
    }

    sqlite3* handle() { return db_; }
    std::mutex& mutex() { return mtx_; }

private:
    bool createTables() {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS media (
                id              INTEGER PRIMARY KEY AUTOINCREMENT,
                title           TEXT    NOT NULL,
                file_path       TEXT    NOT NULL UNIQUE,
                media_type      TEXT    NOT NULL DEFAULT 'video',
                duration_secs   REAL,
                file_size       INTEGER,
                width           INTEGER,
                height          INTEGER,
                video_codec     TEXT,
                audio_codec     TEXT,
                thumbnail_path  TEXT,
                added_at        TEXT    DEFAULT (datetime('now'))
            );
        )";
        char* err = nullptr;
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::cerr << "[db] Table creation failed: " << err << "\n";
            sqlite3_free(err);
            return false;
        }
        return true;
    }

    sqlite3* db_;
    std::mutex mtx_;
};

static Database g_db;

// ===========================================================================
// Path helpers
// ===========================================================================

// If "uploads/foo.mp4" exists, return "uploads/foo (2).mp4" — and so on.
static std::string makeUniquePath(const std::string& dir,
                                  const std::string& filename) {
    Poco::Path p(filename);
    std::string base = p.getBaseName();
    std::string ext  = p.getExtension();
    std::string suffix = ext.empty() ? "" : "." + ext;

    std::string candidate = dir + "/" + filename;
    int counter = 2;
    while (Poco::File(candidate).exists()) {
        candidate = dir + "/" + base + " (" + std::to_string(counter) + ")"
                    + suffix;
        ++counter;
    }
    return candidate;
}

// ===========================================================================
// Multipart upload — streams the "file" part straight to disk.
// No size cap.
// ===========================================================================
class UploadPartHandler : public PartHandler {
public:
    std::string savedPath;
    std::string originalFilename;
    Poco::UInt64 bytesWritten = 0;

    void handlePart(const MessageHeader& header,
                    std::istream& stream) override {
        if (!header.has("Content-Disposition")) return;

        std::string disp;
        NameValueCollection params;
        MessageHeader::splitParameters(header["Content-Disposition"],
                                       disp, params);

        // Only handle the form field named "file".
        if (params.get("name", "") != "file") return;

        // Strip any directory components from the client-supplied filename
        // — protects against ".." or absolute paths in the upload metadata.
        std::string clientName = params.get("filename", "upload.bin");
        originalFilename = Poco::Path(clientName).getFileName();
        if (originalFilename.empty()) originalFilename = "upload.bin";

        savedPath = makeUniquePath(UPLOAD_DIR, originalFilename);

        std::ofstream out(savedPath, std::ios::binary);
        if (!out) {
            std::cerr << "[upload] Failed to open " << savedPath << " for write\n";
            savedPath.clear();
            return;
        }

        char buf[64 * 1024];
        while (stream.read(buf, sizeof(buf)) || stream.gcount()) {
            std::streamsize n = stream.gcount();
            out.write(buf, n);
            bytesWritten += static_cast<Poco::UInt64>(n);
        }
        out.close();
    }
};

// ===========================================================================
// ffprobe — extract metadata as JSON, parse into fields
// ===========================================================================
struct MediaMeta {
    double      duration   = 0.0;
    int64_t     fileSize   = 0;
    int         width      = 0;
    int         height     = 0;
    std::string videoCodec;
    std::string audioCodec;
    bool        probeOk    = false;
};

static MediaMeta probeFile(const std::string& path) {
    MediaMeta m;

    try { m.fileSize = Poco::File(path).getSize(); } catch (...) {}

    std::vector<std::string> args = {
        "-v", "quiet",
        "-print_format", "json",
        "-show_format",
        "-show_streams",
        path
    };

    Poco::Pipe outPipe;
    auto ph = Poco::Process::launch("ffprobe", args, nullptr, &outPipe, nullptr);
    Poco::PipeInputStream istr(outPipe);
    std::string jsonStr;
    Poco::StreamCopier::copyToString(istr, jsonStr);
    int rc = Poco::Process::wait(ph);
    if (rc != 0) {
        std::cerr << "[ffprobe] Non-zero exit for " << path << "\n";
        return m;
    }

    try {
        Parser parser;
        auto root = parser.parse(jsonStr).extract<Object::Ptr>();

        if (root->has("format")) {
            auto fmt = root->getObject("format");
            if (fmt->has("duration"))
                m.duration = std::stod(fmt->getValue<std::string>("duration"));
        }

        if (root->has("streams")) {
            auto streams = root->getArray("streams");
            for (unsigned i = 0; i < streams->size(); ++i) {
                auto s = streams->getObject(i);
                std::string type = s->getValue<std::string>("codec_type");
                if (type == "video" && m.videoCodec.empty()) {
                    m.videoCodec = s->optValue<std::string>("codec_name", "");
                    m.width      = s->optValue<int>("width", 0);
                    m.height     = s->optValue<int>("height", 0);
                }
                if (type == "audio" && m.audioCodec.empty()) {
                    m.audioCodec = s->optValue<std::string>("codec_name", "");
                }
            }
        }
        m.probeOk = true;
    } catch (const std::exception& e) {
        std::cerr << "[ffprobe] JSON parse error: " << e.what() << "\n";
    }

    return m;
}

// ===========================================================================
// Type validation — does this file match the requested media_type?
// Extend the audio/image branches as those types are added.
// ===========================================================================
static bool validateMediaType(const MediaMeta& meta, const std::string& type) {
    if (!meta.probeOk) return false;

    if (type == "video") {
        // Real video: has a video codec and a non-trivial duration.
        // (Still images often present as a video stream with duration ~0.)
        return !meta.videoCodec.empty() && meta.duration > 0.0;
    }
    if (type == "audio") {
        // Audio: has an audio codec and no video stream.
        return !meta.audioCodec.empty() && meta.videoCodec.empty();
    }
    if (type == "image") {
        // Image: ffprobe sees a single video stream, ~0 duration.
        return !meta.videoCodec.empty() && meta.duration == 0.0;
    }
    return false;
}

// ===========================================================================
// Thumbnail generation
// ===========================================================================
static std::string generateThumbnail(int mediaId, const std::string& filePath,
                                     double durationSecs) {
    Poco::File dir(THUMB_DIR);
    if (!dir.exists()) dir.createDirectories();

    std::string outPath = THUMB_DIR + "/" + std::to_string(mediaId) + ".jpg";
    double seekTo = (durationSecs > 1.0) ? durationSecs * 0.1 : 0.0;

    std::vector<std::string> args = {
        "-y", "-ss", std::to_string(seekTo),
        "-i", filePath,
        "-frames:v", "1",
        "-vf", "scale=320:-1",
        outPath
    };

    Poco::Pipe outPipe;
    auto ph = Poco::Process::launch("ffmpeg", args, nullptr, &outPipe, &outPipe);
    Poco::Process::wait(ph);

    if (Poco::File(outPath).exists()) return outPath;
    return "";
}

// ===========================================================================
// On-demand HLS transcoding
// ===========================================================================
static std::map<int, std::mutex> g_hlsLocks;
static std::mutex g_hlsMapMtx;

static std::string hlsDirForMedia(int id) {
    return HLS_DIR + "/" + std::to_string(id);
}

static bool ensureHLS(int mediaId, const std::string& filePath) {
    std::string dir  = hlsDirForMedia(mediaId);
    std::string m3u8 = dir + "/playlist.m3u8";

    if (Poco::File(m3u8).exists()) return true;

    std::mutex* mtx;
    {
        std::lock_guard<std::mutex> g(g_hlsMapMtx);
        mtx = &g_hlsLocks[mediaId];
    }
    std::lock_guard<std::mutex> lock(*mtx);

    if (Poco::File(m3u8).exists()) return true;

    Poco::File(dir).createDirectories();

    std::string segPattern = dir + "/segment%04d.ts";

    std::vector<std::string> args = {
        "-y",
        "-i", filePath,
        "-c:v", "libx264", "-preset", "veryfast",
        "-c:a", "aac", "-b:a", "128k",
        "-f", "hls",
        "-hls_time", "4",
        "-hls_list_size", "0",
        "-hls_segment_filename", segPattern,
        m3u8
    };

    std::cout << "[hls] Transcoding media " << mediaId << " ...\n";

    Poco::Pipe outPipe;
    auto ph = Poco::Process::launch("ffmpeg", args, nullptr, &outPipe, &outPipe);

    for (int i = 0; i < 300; ++i) {
        if (Poco::File(m3u8).exists()) {
            std::cout << "[hls] Playlist ready for media " << mediaId << "\n";
            return true;
        }
        Poco::Thread::sleep(100);
    }

    std::cerr << "[hls] Timeout waiting for playlist (media " << mediaId << ")\n";
    try { Poco::Process::kill(ph); } catch (...) {}
    return false;
}

// ===========================================================================
// Startup orphan sweep — removes upload files not referenced by the DB.
// ===========================================================================
static void sweepOrphanUploads() {
    Poco::File uploads(UPLOAD_DIR);
    if (!uploads.exists()) return;

    std::set<std::string> known;
    {
        std::lock_guard<std::mutex> lock(g_db.mutex());
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(g_db.handle(),
            "SELECT file_path FROM media;", -1, &stmt, nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto p = sqlite3_column_text(stmt, 0);
            if (p) known.insert(reinterpret_cast<const char*>(p));
        }
        sqlite3_finalize(stmt);
    }

    std::vector<Poco::File> entries;
    uploads.list(entries);

    int removed = 0;
    for (auto& f : entries) {
        if (!f.isFile()) continue;
        if (known.find(f.path()) == known.end()) {
            try {
                f.remove();
                ++removed;
                std::cout << "[sweep] Removed orphan: " << f.path() << "\n";
            } catch (const std::exception& e) {
                std::cerr << "[sweep] Failed to remove " << f.path()
                          << ": " << e.what() << "\n";
            }
        }
    }
    std::cout << "[sweep] " << removed << " orphan file(s) removed; "
              << known.size() << " tracked in DB.\n";
}

// ===========================================================================
// HTTP helpers
// ===========================================================================
static void sendJSON(HTTPServerResponse& resp, int status, const std::string& body) {
    resp.setStatus(static_cast<HTTPResponse::HTTPStatus>(status));
    resp.setContentType("application/json");
    resp.set("Access-Control-Allow-Origin", "*");
    resp.sendBuffer(body.data(), body.size());
}

static void serveFile(HTTPServerResponse& resp, const std::string& path,
                      const std::string& mime) {
    Poco::File f(path);
    if (!f.exists()) {
        sendJSON(resp, 404, R"({"error":"file not found"})");
        return;
    }
    resp.setStatus(HTTPResponse::HTTP_OK);
    resp.setContentType(mime);
    resp.set("Access-Control-Allow-Origin", "*");
    resp.set("Cache-Control", "no-cache, no-store");
    resp.setContentLength64(f.getSize());
    std::ifstream ifs(path, std::ios::binary);
    Poco::StreamCopier::copyStream(ifs, resp.send());
}

static int extractId(const std::string& uri, const std::string& prefix) {
    try {
        return std::stoi(uri.substr(prefix.size()));
    } catch (...) { return -1; }
}

static std::string mediaRowToJSON(sqlite3_stmt* stmt) {
    Object obj;
    obj.set("id",            sqlite3_column_int(stmt, 0));
    obj.set("title",         std::string(reinterpret_cast<const char*>(
                                 sqlite3_column_text(stmt, 1))));
    obj.set("file_path",     std::string(reinterpret_cast<const char*>(
                                 sqlite3_column_text(stmt, 2))));
    obj.set("media_type",    std::string(reinterpret_cast<const char*>(
                                 sqlite3_column_text(stmt, 3))));
    obj.set("duration_secs", sqlite3_column_double(stmt, 4));
    obj.set("file_size",     static_cast<int64_t>(sqlite3_column_int64(stmt, 5)));
    obj.set("width",         sqlite3_column_int(stmt, 6));
    obj.set("height",        sqlite3_column_int(stmt, 7));

    auto colText = [&](int c) -> std::string {
        auto p = sqlite3_column_text(stmt, c);
        return p ? std::string(reinterpret_cast<const char*>(p)) : "";
    };
    obj.set("video_codec",    colText(8));
    obj.set("audio_codec",    colText(9));
    obj.set("thumbnail_path", colText(10));
    obj.set("added_at",       colText(11));

    std::ostringstream oss;
    obj.stringify(oss);
    return oss.str();
}

static const char* SELECT_ALL = "SELECT * FROM media ORDER BY added_at DESC;";
static const char* SELECT_ONE = "SELECT * FROM media WHERE id = ?;";

// ===========================================================================
// Request handler
// ===========================================================================
class MediaHandler : public HTTPRequestHandler {
public:
    void handleRequest(HTTPServerRequest& req, HTTPServerResponse& resp) override {
        const std::string& method = req.getMethod();
        const std::string& uri    = req.getURI();

        std::cout << method << " " << uri << "\n";

        // CORS preflight
        if (method == "OPTIONS") {
            resp.setStatus(HTTPResponse::HTTP_NO_CONTENT);
            resp.set("Access-Control-Allow-Origin", "*");
            resp.set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp.set("Access-Control-Allow-Headers", "Content-Type");
            resp.send();
            return;
        }

        // =================================================================
        // POST /api/media  — multipart upload
        //   form fields: file (required), media_type (default "video"),
        //                title (default = original filename basename)
        // =================================================================
        if (method == "POST" && uri == "/api/media") {
            UploadPartHandler partHandler;

            try {
                HTMLForm form(req, req.stream(), partHandler);

                if (partHandler.savedPath.empty()) {
                    sendJSON(resp, 400,
                        R"({"error":"missing 'file' part"})");
                    return;
                }

                std::string filePath  = partHandler.savedPath;
                std::string mediaType = form.get("media_type", "video");
                std::string title     = form.get("title",
                    Poco::Path(partHandler.originalFilename).getBaseName());

                // ----- Probe & validate -----
                MediaMeta meta = probeFile(filePath);
                if (!validateMediaType(meta, mediaType)) {
                    try { Poco::File(filePath).remove(); } catch (...) {}
                    std::string msg =
                        R"({"error":"file is not a valid )" + mediaType + R"("})";
                    sendJSON(resp, 400, msg);
                    return;
                }

                // ----- Insert into DB -----
                int newId = -1;
                {
                    std::lock_guard<std::mutex> lock(g_db.mutex());
                    const char* sql = R"(
                        INSERT INTO media
                          (title, file_path, media_type, duration_secs, file_size,
                           width, height, video_codec, audio_codec)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
                    )";
                    sqlite3_stmt* stmt;
                    if (sqlite3_prepare_v2(g_db.handle(), sql, -1, &stmt, nullptr)
                            != SQLITE_OK) {
                        try { Poco::File(filePath).remove(); } catch (...) {}
                        sendJSON(resp, 500, R"({"error":"db prepare failed"})");
                        return;
                    }
                    sqlite3_bind_text(stmt, 1, title.c_str(),     -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 2, filePath.c_str(),  -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 3, mediaType.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_double(stmt, 4, meta.duration);
                    sqlite3_bind_int64 (stmt, 5, meta.fileSize);
                    sqlite3_bind_int   (stmt, 6, meta.width);
                    sqlite3_bind_int   (stmt, 7, meta.height);
                    sqlite3_bind_text  (stmt, 8, meta.videoCodec.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text  (stmt, 9, meta.audioCodec.c_str(), -1, SQLITE_TRANSIENT);

                    if (sqlite3_step(stmt) != SQLITE_DONE) {
                        std::string err = sqlite3_errmsg(g_db.handle());
                        sqlite3_finalize(stmt);
                        try { Poco::File(filePath).remove(); } catch (...) {}
                        sendJSON(resp, 500,
                            std::string(R"({"error":"db insert failed","detail":")")
                            + err + R"("})");
                        return;
                    }
                    newId = static_cast<int>(
                        sqlite3_last_insert_rowid(g_db.handle()));
                    sqlite3_finalize(stmt);
                }

                // ----- Thumbnail -----
                std::string thumbPath =
                    generateThumbnail(newId, filePath, meta.duration);
                if (!thumbPath.empty()) {
                    std::lock_guard<std::mutex> lock(g_db.mutex());
                    const char* upd =
                        "UPDATE media SET thumbnail_path = ? WHERE id = ?;";
                    sqlite3_stmt* u;
                    sqlite3_prepare_v2(g_db.handle(), upd, -1, &u, nullptr);
                    sqlite3_bind_text(u, 1, thumbPath.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int (u, 2, newId);
                    sqlite3_step(u);
                    sqlite3_finalize(u);
                }

                // ----- Return new row -----
                {
                    std::lock_guard<std::mutex> lock(g_db.mutex());
                    sqlite3_stmt* sel;
                    sqlite3_prepare_v2(g_db.handle(), SELECT_ONE, -1, &sel, nullptr);
                    sqlite3_bind_int(sel, 1, newId);
                    if (sqlite3_step(sel) == SQLITE_ROW) {
                        sendJSON(resp, 201, mediaRowToJSON(sel));
                    } else {
                        sendJSON(resp, 201,
                            R"({"id":)" + std::to_string(newId) + "}");
                    }
                    sqlite3_finalize(sel);
                }
            } catch (const std::exception& e) {
                if (!partHandler.savedPath.empty()) {
                    try { Poco::File(partHandler.savedPath).remove(); } catch (...) {}
                }
                std::cerr << "[upload] Exception: " << e.what() << "\n";
                sendJSON(resp, 400,
                    R"({"error":"failed to process upload"})");
            }
            return;
        }

        // =================================================================
        // GET /api/media  — list all
        // =================================================================
        if (method == "GET" && uri == "/api/media") {
            std::lock_guard<std::mutex> lock(g_db.mutex());
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(g_db.handle(), SELECT_ALL, -1, &stmt, nullptr);

            std::ostringstream oss;
            oss << "[";
            bool first = true;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) oss << ",";
                oss << mediaRowToJSON(stmt);
                first = false;
            }
            oss << "]";
            sqlite3_finalize(stmt);
            sendJSON(resp, 200, oss.str());
            return;
        }

        // =================================================================
        // GET /api/media/{id}/stream/playlist.m3u8 or segment.ts
        // =================================================================
        static const std::regex rxStream(R"(/api/media/(\d+)/stream/(.*))");
        std::smatch sm;
        if (method == "GET" && std::regex_match(uri, sm, rxStream)) {
            int id = std::stoi(sm[1].str());
            std::string file = sm[2].str();

            std::string filePath;
            {
                std::lock_guard<std::mutex> lock(g_db.mutex());
                sqlite3_stmt* stmt;
                sqlite3_prepare_v2(g_db.handle(),
                    "SELECT file_path FROM media WHERE id = ?;",
                    -1, &stmt, nullptr);
                sqlite3_bind_int(stmt, 1, id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    filePath = reinterpret_cast<const char*>(
                        sqlite3_column_text(stmt, 0));
                }
                sqlite3_finalize(stmt);
            }

            if (filePath.empty()) {
                sendJSON(resp, 404, R"({"error":"media not found"})");
                return;
            }

            if (!ensureHLS(id, filePath)) {
                sendJSON(resp, 500, R"({"error":"transcoding failed"})");
                return;
            }

            if (file.find("..") != std::string::npos ||
                file.find('/') != std::string::npos) {
                sendJSON(resp, 403, R"({"error":"forbidden"})");
                return;
            }

            std::string fullPath = hlsDirForMedia(id) + "/" + file;
            std::string mime = (file.find(".m3u8") != std::string::npos)
                ? "application/vnd.apple.mpegurl"
                : "video/MP2T";
            serveFile(resp, fullPath, mime);
            return;
        }

        // =================================================================
        // GET /api/media/{id}/thumbnail
        // =================================================================
        static const std::regex rxThumb(R"(/api/media/(\d+)/thumbnail)");
        if (method == "GET" && std::regex_match(uri, sm, rxThumb)) {
            int id = std::stoi(sm[1].str());
            std::string thumbPath;
            {
                std::lock_guard<std::mutex> lock(g_db.mutex());
                sqlite3_stmt* stmt;
                sqlite3_prepare_v2(g_db.handle(),
                    "SELECT thumbnail_path FROM media WHERE id = ?;",
                    -1, &stmt, nullptr);
                sqlite3_bind_int(stmt, 1, id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    auto p = sqlite3_column_text(stmt, 0);
                    if (p) thumbPath = reinterpret_cast<const char*>(p);
                }
                sqlite3_finalize(stmt);
            }
            if (thumbPath.empty() || !Poco::File(thumbPath).exists()) {
                sendJSON(resp, 404, R"({"error":"thumbnail not available"})");
                return;
            }
            serveFile(resp, thumbPath, "image/jpeg");
            return;
        }

        // =================================================================
        // GET /api/media/{id}
        // =================================================================
        if (method == "GET" && uri.rfind("/api/media/", 0) == 0) {
            int id = extractId(uri, "/api/media/");
            if (id < 0) {
                sendJSON(resp, 400, R"({"error":"invalid id"})");
                return;
            }
            std::lock_guard<std::mutex> lock(g_db.mutex());
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(g_db.handle(), SELECT_ONE, -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                sendJSON(resp, 200, mediaRowToJSON(stmt));
            } else {
                sendJSON(resp, 404, R"({"error":"not found"})");
            }
            sqlite3_finalize(stmt);
            return;
        }

        // =================================================================
        // PUT /api/media/{id}  — update title
        // =================================================================
        if (method == "PUT" && uri.rfind("/api/media/", 0) == 0) {
            int id = extractId(uri, "/api/media/");
            if (id < 0) {
                sendJSON(resp, 400, R"({"error":"invalid id"})");
                return;
            }
            std::string raw;
            Poco::StreamCopier::copyToString(req.stream(), raw);

            std::string newTitle;
            try {
                Parser p;
                auto obj = p.parse(raw).extract<Object::Ptr>();
                newTitle = obj->getValue<std::string>("title");
            } catch (...) {
                sendJSON(resp, 400,
                    R"({"error":"invalid JSON or missing 'title'"})");
                return;
            }

            std::lock_guard<std::mutex> lock(g_db.mutex());
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(g_db.handle(),
                "UPDATE media SET title = ? WHERE id = ?;",
                -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, newTitle.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, id);
            sqlite3_step(stmt);
            int changes = sqlite3_changes(g_db.handle());
            sqlite3_finalize(stmt);

            if (changes == 0) {
                sendJSON(resp, 404, R"({"error":"not found"})");
                return;
            }

            sqlite3_prepare_v2(g_db.handle(), SELECT_ONE, -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, id);
            if (sqlite3_step(stmt) == SQLITE_ROW)
                sendJSON(resp, 200, mediaRowToJSON(stmt));
            sqlite3_finalize(stmt);
            return;
        }

        // =================================================================
        // DELETE /api/media/{id}  — also removes the upload file
        // =================================================================
        if (method == "DELETE" && uri.rfind("/api/media/", 0) == 0) {
            int id = extractId(uri, "/api/media/");
            if (id < 0) {
                sendJSON(resp, 400, R"({"error":"invalid id"})");
                return;
            }

            std::string filePath;

            std::lock_guard<std::mutex> lock(g_db.mutex());

            // Read file_path before deleting the row.
            {
                sqlite3_stmt* sel;
                sqlite3_prepare_v2(g_db.handle(),
                    "SELECT file_path FROM media WHERE id = ?;",
                    -1, &sel, nullptr);
                sqlite3_bind_int(sel, 1, id);
                if (sqlite3_step(sel) == SQLITE_ROW) {
                    auto p = sqlite3_column_text(sel, 0);
                    if (p) filePath = reinterpret_cast<const char*>(p);
                }
                sqlite3_finalize(sel);
            }

            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(g_db.handle(),
                "DELETE FROM media WHERE id = ?;", -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, id);
            sqlite3_step(stmt);
            int changes = sqlite3_changes(g_db.handle());
            sqlite3_finalize(stmt);

            if (changes == 0) {
                sendJSON(resp, 404, R"({"error":"not found"})");
            } else {
                // Remove uploaded file
                if (!filePath.empty()) {
                    try {
                        Poco::File f(filePath);
                        if (f.exists()) f.remove();
                    } catch (...) {}
                }
                // HLS cache
                try {
                    Poco::File hlsDir(hlsDirForMedia(id));
                    if (hlsDir.exists()) hlsDir.remove(true);
                } catch (...) {}
                // Thumbnail
                try {
                    Poco::File thumb(THUMB_DIR + "/" + std::to_string(id) + ".jpg");
                    if (thumb.exists()) thumb.remove();
                } catch (...) {}

                sendJSON(resp, 200, R"({"result":"deleted"})");
            }
            return;
        }

        // 404 fallback
        sendJSON(resp, 404, R"({"error":"endpoint not found"})");
    }
};

// ===========================================================================
// Factory
// ===========================================================================
class MediaHandlerFactory : public HTTPRequestHandlerFactory {
public:
    HTTPRequestHandler* createRequestHandler(
            const HTTPServerRequest&) override {
        return new MediaHandler();
    }
};

// ===========================================================================
// Application
// ===========================================================================
class MediaServerApp : public ServerApplication {
protected:
    int main(const std::vector<std::string>&) override {

        if (!g_db.open(DB_PATH)) {
            std::cerr << "Failed to open database. Aborting.\n";
            return Application::EXIT_SOFTWARE;
        }
        std::cout << "[db] Opened " << DB_PATH << "\n";

        // Make sure runtime directories exist
        try { Poco::File(UPLOAD_DIR).createDirectories(); } catch (...) {}
        try { Poco::File(HLS_DIR).createDirectories();    } catch (...) {}
        try { Poco::File(THUMB_DIR).createDirectories();  } catch (...) {}

        // Reconcile uploads/ with the DB
        sweepOrphanUploads();

        ServerSocket socket(PORT);
        HTTPServerParams* params = new HTTPServerParams();
        params->setMaxQueued(100);
        params->setMaxThreads(16);

        HTTPServer server(new MediaHandlerFactory(), socket, params);
        server.start();

        std::cout
            << "\n=== Media Center listening on http://localhost:"
            << PORT << " ===\n\n"
            << "Endpoints:\n"
            << "  POST   /api/media               "
               "multipart/form-data: file=<binary>, "
               "media_type=<video|audio|image>, title=<optional>\n"
            << "  GET    /api/media               list all\n"
            << "  GET    /api/media/{id}          detail\n"
            << "  PUT    /api/media/{id}          body: {\"title\":\"...\"}\n"
            << "  DELETE /api/media/{id}\n"
            << "  GET    /api/media/{id}/stream/playlist.m3u8\n"
            << "  GET    /api/media/{id}/thumbnail\n\n"
            << "Press Ctrl+C to stop.\n";

        waitForTerminationRequest();
        server.stop();
        return Application::EXIT_OK;
    }
};

POCO_SERVER_MAIN(MediaServerApp)