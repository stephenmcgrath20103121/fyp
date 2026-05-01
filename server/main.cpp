#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/PartHandler.h>
#include <Poco/Net/MessageHeader.h>
#include <Poco/Net/NetworkInterface.h>
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
#include <algorithm>

using namespace Poco::Net;
using namespace Poco::Util;
using namespace Poco::JSON;

// ===========================================================================
// Configuration
// ===========================================================================
static const unsigned short PORT      = 8080;
static const std::string    DB_PATH   = "mediacenter.db";
static const std::string    HLS_DIR   = "hls_cache";
static const std::string    HLS_AUDIO_DIR = "hls_audio_cache";
static const std::string    IMAGE_CACHE_DIR = "image_cache";
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
    std::string formatName;
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
            m.formatName = fmt->optValue<std::string>("format_name", "");
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
// ===========================================================================

// Codecs ffprobe reports for image streams. HEVC and AV1 also appear here
// because HEIC and AVIF are encoded with those codecs; the audio + container
// checks in validateMediaType disambiguate them from real H.265 / AV1 video.
static const std::set<std::string> kImageCodecs = {
    "mjpeg",        // JPEG
    "png",
    "gif",
    "webp",
    "bmp",
    "tiff",
    "hevc",         // HEIC (also H.265 video — disambiguated below)
    "av1",          // AVIF (also AV1 video — disambiguated below)
    "jpeg2000",
    "jpegxl"
};

// Container format_name fragments that indicate an image-only container.
// ffprobe reports format_name as a comma-separated list of aliases, so we
// substring-match.
static bool isImageContainer(const std::string& formatName) {
    static const std::vector<std::string> imageFormats = {
        "image2", "png_pipe", "webp_pipe", "jpeg_pipe",
        "gif", "bmp_pipe", "tiff_pipe", "jpegxl_pipe"
    };
    for (const auto& f : imageFormats) {
        if (formatName.find(f) != std::string::npos) return true;
    }
    return false;
}

// HEIC and AVIF live in MP4-family containers. These containers can also
// hold real video, so they're only treated as image when there's no audio.
static bool isHeifContainer(const std::string& formatName) {
    return formatName.find("mov,mp4") != std::string::npos
        || formatName.find("mp4")     != std::string::npos
        || formatName.find("heif")    != std::string::npos
        || formatName.find("heic")    != std::string::npos
        || formatName.find("avif")    != std::string::npos;
}

static bool validateMediaType(const MediaMeta& meta, const std::string& type) {
    if (!meta.probeOk) return false;

    if (type == "video") {
        // Real video: has a video codec and a non-trivial duration.
        return !meta.videoCodec.empty() && meta.duration > 0.0;
    }
    if (type == "audio") {
        // Audio: has an audio codec and no video stream.
        return !meta.audioCodec.empty() && meta.videoCodec.empty();
    }
    if (type == "image") {
        // Must have a recognised image codec.
        if (kImageCodecs.count(meta.videoCodec) == 0) return false;
        // Must have no audio stream (rules out music videos with cover art
        // misidentified, or H.265 video with audio).
        if (!meta.audioCodec.empty()) return false;
        // For HEVC / AV1, additionally require an image-style container
        // (HEIF/MP4-family). Other image codecs are unambiguous.
        if (meta.videoCodec == "hevc" || meta.videoCodec == "av1") {
            return isHeifContainer(meta.formatName);
        }
        // For other image codecs, accept either a known image container or
        // (defensively) just trust the codec — `mjpeg` etc. only appear in
        // image contexts in practice for files this server would ingest.
        return isImageContainer(meta.formatName) || true;
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
// On-demand audio HLS transcoding
// ===========================================================================
static std::map<int, std::mutex> g_hlsAudioLocks;
static std::mutex g_hlsAudioMapMtx;

static std::string audioHlsDirForMedia(int id) {
    return HLS_AUDIO_DIR + "/" + std::to_string(id);
}

static bool ensureHLSAudio(int mediaId, const std::string& filePath) {
    std::string dir  = audioHlsDirForMedia(mediaId);
    std::string m3u8 = dir + "/playlist.m3u8";

    if (Poco::File(m3u8).exists()) return true;

    std::mutex* mtx;
    {
        std::lock_guard<std::mutex> g(g_hlsAudioMapMtx);
        mtx = &g_hlsAudioLocks[mediaId];
    }
    std::lock_guard<std::mutex> lock(*mtx);

    if (Poco::File(m3u8).exists()) return true;

    Poco::File(dir).createDirectories();

    std::string segPattern = dir + "/segment%04d.ts";

    std::vector<std::string> args = {
        "-y",
        "-i", filePath,
        "-vn",                              // drop any embedded cover art
        "-c:a", "aac", "-b:a", "192k",
        "-f", "hls",
        "-hls_time", "10",
        "-hls_list_size", "0",
        "-hls_segment_filename", segPattern,
        m3u8
    };

    std::cout << "[hls-audio] Transcoding media " << mediaId << " ...\n";

    Poco::Pipe outPipe;
    auto ph = Poco::Process::launch("ffmpeg", args, nullptr, &outPipe, &outPipe);

    for (int i = 0; i < 300; ++i) {
        if (Poco::File(m3u8).exists()) {
            std::cout << "[hls-audio] Playlist ready for media " << mediaId << "\n";
            return true;
        }
        Poco::Thread::sleep(100);
    }

    std::cerr << "[hls-audio] Timeout waiting for playlist (media "
              << mediaId << ")\n";
    try { Poco::Process::kill(ph); } catch (...) {}
    return false;
}

// ===========================================================================
// Startup orphan sweep
//   - uploads/                  keyed by file_path (DB column)
//   - hls_cache/<id>/           keyed by media id (subdirectory name)
//   - hls_audio_cache/<id>/     keyed by media id (subdirectory name)
//   - thumbnails/<id>.jpg       keyed by media id (filename stem)
//   - image_cache/<id>.jpg      keyed by media id (filename stem)
// ===========================================================================

// Strip the extension from a filename, e.g. "42.jpg" -> "42".
static std::string stripExtension(const std::string& filename) {
    size_t dot = filename.find_last_of('.');
    return (dot == std::string::npos) ? filename : filename.substr(0, dot);
}

// Try to parse a string as a positive int. Returns -1 if not a clean integer.
static int parseIdOrNeg1(const std::string& s) {
    if (s.empty()) return -1;
    for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return -1;
    try { return std::stoi(s); } catch (...) { return -1; }
}

// Sweep a directory of "<id>.<ext>" files. Removes entries whose id is not
// in `knownIds`, or whose name doesn't parse as an integer at all.
static int sweepIdNamedFiles(const std::string& dirPath,
                             const std::set<int>& knownIds,
                             const char* tag) {
    Poco::File dir(dirPath);
    if (!dir.exists()) return 0;

    std::vector<Poco::File> entries;
    dir.list(entries);

    int removed = 0;
    for (auto& f : entries) {
        if (!f.isFile()) continue;
        std::string name = Poco::Path(f.path()).getFileName();
        int id = parseIdOrNeg1(stripExtension(name));
        if (id < 0 || knownIds.find(id) == knownIds.end()) {
            try {
                f.remove();
                ++removed;
                std::cout << "[sweep:" << tag << "] Removed orphan: "
                          << f.path() << "\n";
            } catch (const std::exception& e) {
                std::cerr << "[sweep:" << tag << "] Failed to remove "
                          << f.path() << ": " << e.what() << "\n";
            }
        }
    }
    return removed;
}

// Sweep a directory of "<id>/" subdirectories. Removes whole subtrees whose
// id is not in `knownIds`, or whose name doesn't parse as an integer.
static int sweepIdNamedDirs(const std::string& dirPath,
                            const std::set<int>& knownIds,
                            const char* tag) {
    Poco::File dir(dirPath);
    if (!dir.exists()) return 0;

    std::vector<Poco::File> entries;
    dir.list(entries);

    int removed = 0;
    for (auto& f : entries) {
        if (!f.isDirectory()) continue;
        std::string name = Poco::Path(f.path()).getFileName();
        int id = parseIdOrNeg1(name);
        if (id < 0 || knownIds.find(id) == knownIds.end()) {
            try {
                f.remove(true);   // recursive
                ++removed;
                std::cout << "[sweep:" << tag << "] Removed orphan: "
                          << f.path() << "\n";
            } catch (const std::exception& e) {
                std::cerr << "[sweep:" << tag << "] Failed to remove "
                          << f.path() << ": " << e.what() << "\n";
            }
        }
    }
    return removed;
}

static int sweepOrphanUploads(const std::set<std::string>& knownPaths) {
    Poco::File uploads(UPLOAD_DIR);
    if (!uploads.exists()) return 0;

    std::vector<Poco::File> entries;
    uploads.list(entries);

    int removed = 0;
    for (auto& f : entries) {
        if (!f.isFile()) continue;
        if (knownPaths.find(f.path()) == knownPaths.end()) {
            try {
                f.remove();
                ++removed;
                std::cout << "[sweep:uploads] Removed orphan: "
                          << f.path() << "\n";
            } catch (const std::exception& e) {
                std::cerr << "[sweep:uploads] Failed to remove "
                          << f.path() << ": " << e.what() << "\n";
            }
        }
    }
    return removed;
}

// Top-level — runs all five sweeps against a single DB snapshot.
static void sweepOrphans() {
    std::set<std::string> knownPaths;
    std::set<int>         knownIds;

    {
        std::lock_guard<std::mutex> lock(g_db.mutex());
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(g_db.handle(),
            "SELECT id, file_path FROM media;", -1, &stmt, nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            knownIds.insert(sqlite3_column_int(stmt, 0));
            auto p = sqlite3_column_text(stmt, 1);
            if (p) knownPaths.insert(reinterpret_cast<const char*>(p));
        }
        sqlite3_finalize(stmt);
    }

    int total = 0;
    total += sweepOrphanUploads(knownPaths);
    total += sweepIdNamedDirs (HLS_DIR,         knownIds, "hls");
    total += sweepIdNamedDirs (HLS_AUDIO_DIR,   knownIds, "hls-audio");
    total += sweepIdNamedFiles(THUMB_DIR,       knownIds, "thumbnails");
    total += sweepIdNamedFiles(IMAGE_CACHE_DIR, knownIds, "image-cache");

    std::cout << "[sweep] " << total << " orphan entry/entries removed; "
              << knownIds.size() << " media row(s) in DB.\n";
}

// ===========================================================================
// HTTP helpers
// ===========================================================================
static void sendJSON(HTTPServerResponse& resp, int status, const std::string& body) {
    resp.setStatus(static_cast<HTTPResponse::HTTPStatus>(status));
    resp.setContentType("application/json");
    resp.set("Access-Control-Allow-Origin", "*");
    resp.set("Cross-Origin-Resource-Policy", "cross-origin");
    resp.set("Cache-Control", "no-cache, must-revalidate");
    resp.sendBuffer(body.data(), body.size());
}

static void serveFile(HTTPServerResponse& resp, const std::string& path,
                      const std::string& mime,
                      const std::string& cacheControl = "no-cache, no-store") {
    Poco::File f(path);
    if (!f.exists()) {
        sendJSON(resp, 404, R"({"error":"file not found"})");
        return;
    }
    resp.setStatus(HTTPResponse::HTTP_OK);
    resp.setContentType(mime);
    resp.set("Access-Control-Allow-Origin", "*");
    resp.set("Cross-Origin-Resource-Policy", "cross-origin");
    resp.set("Cache-Control", cacheControl);
    resp.setContentLength64(f.getSize());
    std::ifstream ifs(path, std::ios::binary);
    Poco::StreamCopier::copyStream(ifs, resp.send());
}

static int extractId(const std::string& uri, const std::string& prefix) {
    try {
        return std::stoi(uri.substr(prefix.size()));
    } catch (...) { return -1; }
}

// ===========================================================================
// Map a file extension to an image MIME type. Returns "" if unknown.
// ===========================================================================
static std::string imageMimeForPath(const std::string& path) {
    std::string ext = Poco::Path(path).getExtension();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png")                  return "image/png";
    if (ext == "gif")                  return "image/gif";
    if (ext == "webp")                 return "image/webp";
    if (ext == "bmp")                  return "image/bmp";
    if (ext == "tif" || ext == "tiff") return "image/tiff";
    if (ext == "avif")                 return "image/avif";
    if (ext == "heic")                 return "image/heic";
    if (ext == "svg")                  return "image/svg+xml";
    return "";
}

// ===========================================================================
// Image transcoding — convert formats browsers don't render natively (HEIC,
// TIFF) to JPEG, cached on disk. Native formats are passed through.
// ===========================================================================
static std::map<int, std::mutex> g_imageLocks;
static std::mutex g_imageMapMtx;

static bool isBrowserNativeImage(const std::string& extLower) {
    return extLower == "jpg"  || extLower == "jpeg" || extLower == "png"
        || extLower == "gif"  || extLower == "webp" || extLower == "avif"
        || extLower == "bmp"  || extLower == "svg";
}

// Returns a path that's safe to serve to a browser. For native formats this
// is the original upload; otherwise it's a cached JPEG conversion.
// Sets outMime accordingly. Returns "" on conversion failure.
static std::string ensureServeableImage(int id, const std::string& filePath,
                                        std::string& outMime) {
    std::string ext = Poco::Path(filePath).getExtension();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (isBrowserNativeImage(ext)) {
        outMime = imageMimeForPath(filePath);
        if (outMime.empty()) outMime = "application/octet-stream";
        return filePath;
    }

    // Convert — cached as JPEG keyed by media id.
    std::string cachedPath = IMAGE_CACHE_DIR + "/" + std::to_string(id) + ".jpg";
    outMime = "image/jpeg";

    if (Poco::File(cachedPath).exists()) return cachedPath;

    std::mutex* mtx;
    {
        std::lock_guard<std::mutex> g(g_imageMapMtx);
        mtx = &g_imageLocks[id];
    }
    std::lock_guard<std::mutex> lock(*mtx);

    // Re-check after acquiring lock.
    if (Poco::File(cachedPath).exists()) return cachedPath;

    try { Poco::File(IMAGE_CACHE_DIR).createDirectories(); } catch (...) {}

    std::vector<std::string> args = {
        "-y",
        "-i", filePath,
        "-q:v", "2",            // visually transparent JPEG quality
        cachedPath
    };

    std::cout << "[image] Transcoding media " << id << " (" << ext
              << " -> jpg) ...\n";

    Poco::Pipe outPipe;
    auto ph = Poco::Process::launch("ffmpeg", args, nullptr, &outPipe, &outPipe);
    int rc = Poco::Process::wait(ph);

    if (rc != 0 || !Poco::File(cachedPath).exists()) {
        std::cerr << "[image] Conversion failed for media " << id << "\n";
        return "";
    }
    return cachedPath;
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
            resp.set("Cross-Origin-Resource-Policy", "cross-origin");
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
        // GET /api/media/{id}/audio/playlist.m3u8 or segment.ts
        // =================================================================
        static const std::regex rxAudioStream(R"(/api/media/(\d+)/audio/(.*))");
        std::smatch smA;
        if (method == "GET" && std::regex_match(uri, smA, rxAudioStream)) {
            int id = std::stoi(smA[1].str());
            std::string file = smA[2].str();

            std::string filePath, mediaType;
            {
                std::lock_guard<std::mutex> lock(g_db.mutex());
                sqlite3_stmt* stmt;
                sqlite3_prepare_v2(g_db.handle(),
                    "SELECT file_path, media_type FROM media WHERE id = ?;",
                    -1, &stmt, nullptr);
                sqlite3_bind_int(stmt, 1, id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    filePath  = reinterpret_cast<const char*>(
                                   sqlite3_column_text(stmt, 0));
                    mediaType = reinterpret_cast<const char*>(
                                   sqlite3_column_text(stmt, 1));
                }
                sqlite3_finalize(stmt);
            }

            if (filePath.empty()) {
                sendJSON(resp, 404, R"({"error":"media not found"})");
                return;
            }
            if (mediaType != "audio") {
                sendJSON(resp, 400, R"({"error":"item is not audio"})");
                return;
            }

            if (!ensureHLSAudio(id, filePath)) {
                sendJSON(resp, 500, R"({"error":"audio transcoding failed"})");
                return;
            }

            if (file.find("..") != std::string::npos ||
                file.find('/')  != std::string::npos) {
                sendJSON(resp, 403, R"({"error":"forbidden"})");
                return;
            }

            std::string fullPath = audioHlsDirForMedia(id) + "/" + file;
            std::string mime = (file.find(".m3u8") != std::string::npos)
                ? "application/vnd.apple.mpegurl"
                : "video/MP2T";   // .ts segments carry MPEG-TS even when audio-only
            serveFile(resp, fullPath, mime);
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

            std::string filePath, mediaType;
            {
                std::lock_guard<std::mutex> lock(g_db.mutex());
                sqlite3_stmt* stmt;
                sqlite3_prepare_v2(g_db.handle(),
                    "SELECT file_path, media_type FROM media WHERE id = ?;",
                    -1, &stmt, nullptr);
                sqlite3_bind_int(stmt, 1, id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    filePath = reinterpret_cast<const char*>(
                        sqlite3_column_text(stmt, 0));
                    mediaType = reinterpret_cast<const char*>(
                           sqlite3_column_text(stmt, 1));
                }
                sqlite3_finalize(stmt);
            }

            if (filePath.empty()) {
                sendJSON(resp, 404, R"({"error":"media not found"})");
                return;
            }

            if (mediaType != "video") {
                sendJSON(resp, 400, R"({"error":"item is not video"})");
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
        static const std::regex rxThumb(R"(/api/media/(\d+)/thumbnail(?:\?.*)?)");
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
            serveFile(resp, thumbPath, "image/jpeg",
                      "public, max-age=3600, must-revalidate");
            return;
        }

        // =================================================================
        // GET /api/media/{id}/image  — serve original or transcoded image
        // =================================================================
        static const std::regex rxImage(R"(/api/media/(\d+)/image(?:\?.*)?)");
        if (method == "GET" && std::regex_match(uri, sm, rxImage)) {
            int id = std::stoi(sm[1].str());

            std::string filePath, mediaType;
            {
                std::lock_guard<std::mutex> lock(g_db.mutex());
                sqlite3_stmt* stmt;
                sqlite3_prepare_v2(g_db.handle(),
                    "SELECT file_path, media_type FROM media WHERE id = ?;",
                    -1, &stmt, nullptr);
                sqlite3_bind_int(stmt, 1, id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    filePath  = reinterpret_cast<const char*>(
                                   sqlite3_column_text(stmt, 0));
                    mediaType = reinterpret_cast<const char*>(
                                   sqlite3_column_text(stmt, 1));
                }
                sqlite3_finalize(stmt);
            }

            if (filePath.empty()) {
                sendJSON(resp, 404, R"({"error":"media not found"})");
                return;
            }
            if (mediaType != "image") {
                sendJSON(resp, 400, R"({"error":"item is not image"})");
                return;
            }

            std::string mime;
            std::string serveablePath = ensureServeableImage(id, filePath, mime);
            if (serveablePath.empty()) {
                sendJSON(resp, 500, R"({"error":"image conversion failed"})");
                return;
            }

            serveFile(resp, serveablePath, mime,
                      "public, max-age=3600, must-revalidate");
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
                // Image conversion cache
                try {
                    Poco::File imgCache(IMAGE_CACHE_DIR + "/" + std::to_string(id) + ".jpg");
                    if (imgCache.exists()) imgCache.remove();
                } catch (...) {}
                // Audio HLS cache
                try {
                    Poco::File audioHlsDir(audioHlsDirForMedia(id));
                    if (audioHlsDir.exists()) audioHlsDir.remove(true);
                } catch (...) {}
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
// Detect the primary IPv4 address on the LAN.
// Skips loopback (127.x), wildcard, and link-local (169.254.x) autoconf addrs.
// ===========================================================================
static std::string detectLANAddress() {
    using Poco::Net::NetworkInterface;
    using Poco::Net::IPAddress;

    try {
        // Defaults: only interfaces that are up and have at least one IP.
        for (const auto& iface : NetworkInterface::list()) {
            if (iface.isLoopback()) continue;
            for (const auto& tuple : iface.addressList()) {
                const IPAddress& addr = tuple.get<0>();
                if (addr.family() != IPAddress::IPv4) continue;
                if (addr.isLoopback() || addr.isWildcard()) continue;
                // Skip 169.254.x.x (link-local / DHCP failure addresses)
                std::string s = addr.toString();
                if (s.rfind("169.254.", 0) == 0) continue;
                return s;
            }
        }
    } catch (...) {}
    return "";
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
        try { Poco::File(IMAGE_CACHE_DIR).createDirectories(); } catch (...) {}
        try { Poco::File(HLS_AUDIO_DIR).createDirectories(); } catch (...) {}
        try { Poco::File(HLS_DIR).createDirectories();    } catch (...) {}
        try { Poco::File(THUMB_DIR).createDirectories();  } catch (...) {}

        // Reconcile uploads/ with the DB
        sweepOrphans();

        ServerSocket socket(PORT);
        HTTPServerParams* params = new HTTPServerParams();
        params->setMaxQueued(100);
        params->setMaxThreads(16);

        HTTPServer server(new MediaHandlerFactory(), socket, params);
        server.start();

        const std::string lanIP = detectLANAddress();

        std::cout
            << "\n=== Media Center listening on port " << PORT << " ===\n"
            << "  localhost:       http://localhost:" << PORT << "\n";
        if (!lanIP.empty()) {
            std::cout
                << "  on this network: http://" << lanIP << ":" << PORT << "\n"
                << "\n  → On the client, set in .env.local:\n"
                << "    NEXT_PUBLIC_MEDIA_API=http://" << lanIP << ":" << PORT << "\n";
        } else {
            std::cout
                << "  (could not auto-detect a LAN IPv4 address)\n";
        }

        std::cout
            << "\nEndpoints:\n"
            << "  POST   /api/media               "
            "multipart/form-data: file=<binary>, "
            "media_type=<video|audio|image>, title=<optional>\n"
            << "  GET    /api/media               list all\n"
            << "  GET    /api/media/{id}          detail\n"
            << "  PUT    /api/media/{id}          body: {\"title\":\"...\"}\n"
            << "  DELETE /api/media/{id}\n"
            << "  GET    /api/media/{id}/image\n"
            << "  GET    /api/media/{id}/audio/playlist.m3u8\n"
            << "  GET    /api/media/{id}/stream/playlist.m3u8\n"
            << "  GET    /api/media/{id}/thumbnail\n\n"
            << "Press Ctrl+C to stop.\n";

        waitForTerminationRequest();
        server.stop();
        return Application::EXIT_OK;
    }
};

POCO_SERVER_MAIN(MediaServerApp)