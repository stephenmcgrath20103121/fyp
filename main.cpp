#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>
#include <Poco/Process.h>
#include <Poco/Pipe.h>
#include <Poco/PipeStream.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/Thread.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <mutex>
#include <vector>

using namespace Poco::Net;
using namespace Poco::Util;
using namespace Poco::JSON;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
static const std::string VIDEO_SOURCE   = "sampleVideo.mp4";   // hardcoded input
static const std::string HLS_OUTPUT_DIR = "hls_output";        // segment output dir
static const std::string HLS_PLAYLIST   = "playlist.m3u8";
static const unsigned short PORT        = 8080;

// ---------------------------------------------------------------------------
// In-memory "database"
// ---------------------------------------------------------------------------
struct ItemStore {
    std::map<int, std::string> items = { {1, "sampleVideo.mp4"}, {2, "sampleMusic.mp3"} };
    int nextId = 3;
    std::mutex mtx;
};

static ItemStore g_store;

// ---------------------------------------------------------------------------
// Helper — send a JSON response
// ---------------------------------------------------------------------------
static void sendJSON(HTTPServerResponse& resp, int status, const std::string& body) {
    resp.setStatus(static_cast<HTTPResponse::HTTPStatus>(status));
    resp.setContentType("application/json");
    resp.sendBuffer(body.data(), body.size());
}

static std::string itemsToJSON() {
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (auto& [id, name] : g_store.items) {
        if (!first) oss << ",";
        oss << "{\"id\":" << id << ",\"name\":\"" << name << "\"}";
        first = false;
    }
    oss << "]";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Helper — serve a static file with the given MIME type
// ---------------------------------------------------------------------------
static void serveFile(HTTPServerResponse& resp, const std::string& path,
                      const std::string& mimeType) {
    Poco::File f(path);
    if (!f.exists()) {
        sendJSON(resp, 404, R"({"error":"file not found"})");
        return;
    }

    resp.setStatus(HTTPResponse::HTTP_OK);
    resp.setContentType(mimeType);

    // Allow any origin so browser-based players also work
    resp.set("Access-Control-Allow-Origin", "*");

    // Disable caching so VLC always gets the latest playlist
    resp.set("Cache-Control", "no-cache, no-store");

    std::ifstream ifs(path, std::ios::binary);
    Poco::StreamCopier::copyStream(ifs, resp.send());
}

// ---------------------------------------------------------------------------
// Launch ffmpeg to produce HLS segments
// ---------------------------------------------------------------------------
static Poco::ProcessHandle launchFFmpeg() {
    // Ensure output directory exists
    Poco::File dir(HLS_OUTPUT_DIR);
    if (!dir.exists()) dir.createDirectories();

    std::string segmentPattern = HLS_OUTPUT_DIR + "/segment%04d.ts";
    std::string playlistPath   = HLS_OUTPUT_DIR + "/" + HLS_PLAYLIST;

    // Build ffmpeg argument list
    std::vector<std::string> args;
    args.push_back("-y");                         // overwrite without asking
    args.push_back("-i");
    args.push_back(VIDEO_SOURCE);
    // Video codec
    args.push_back("-c:v");
    args.push_back("libx264");
    args.push_back("-preset");
    args.push_back("veryfast");
    // Audio codec
    args.push_back("-c:a");
    args.push_back("aac");
    args.push_back("-b:a");
    args.push_back("128k");
    // HLS muxer options
    args.push_back("-f");
    args.push_back("hls");
    args.push_back("-hls_time");
    args.push_back("4");                          // 4-second segments
    args.push_back("-hls_list_size");
    args.push_back("0");                          // keep all segments in playlist
    args.push_back("-hls_base_url");
    args.push_back("/stream/");    
    args.push_back("-hls_segment_filename");
    args.push_back(segmentPattern);
    args.push_back(playlistPath);

    std::cout << "[ffmpeg] Launching transcode of " << VIDEO_SOURCE << " ...\n";

    Poco::Pipe outPipe;
    return Poco::Process::launch("ffmpeg", args, nullptr, &outPipe, &outPipe);
}

// ---------------------------------------------------------------------------
// Wait for the playlist file to appear on disk (with timeout)
// ---------------------------------------------------------------------------
static bool waitForPlaylist(int timeoutSeconds = 30) {
    std::string path = HLS_OUTPUT_DIR + "/" + HLS_PLAYLIST;
    for (int i = 0; i < timeoutSeconds * 10; ++i) {
        if (Poco::File(path).exists()) {
            std::cout << "[ffmpeg] Playlist is available.\n";
            return true;
        }
        Poco::Thread::sleep(100); // 100 ms
    }
    std::cerr << "[ffmpeg] Timed out waiting for playlist.\n";
    return false;
}

// ---------------------------------------------------------------------------
// Request handler — dispatches by method + path
// ---------------------------------------------------------------------------
class RestHandler : public HTTPRequestHandler {
public:
    void handleRequest(HTTPServerRequest& req, HTTPServerResponse& resp) override {
        const std::string& method = req.getMethod();
        const std::string& uri    = req.getURI();

        std::cout << method << " " << uri << "\n";

        // ---------------------------------------------------------------
        // HLS streaming endpoints
        // ---------------------------------------------------------------

        // GET /stream/playlist.m3u8
        if (method == "GET" && uri == "/stream/" + HLS_PLAYLIST) {
            serveFile(resp, HLS_OUTPUT_DIR + "/" + HLS_PLAYLIST,
                      "application/vnd.apple.mpegurl");
            return;
        }

        // GET /stream/<segment>.ts
        if (method == "GET" && uri.rfind("/stream/", 0) == 0) {
            std::string filename = uri.substr(8); // strip "/stream/"
            // Basic safety: reject paths with ".." or "/"
            if (filename.find("..") != std::string::npos ||
                filename.find('/')  != std::string::npos) {
                sendJSON(resp, 403, R"({"error":"forbidden"})");
                return;
            }
            serveFile(resp, HLS_OUTPUT_DIR + "/" + filename, "video/MP2T");
            return;
        }

        // ---------------------------------------------------------------
        // Original REST API endpoints (unchanged)
        // ---------------------------------------------------------------

        // GET /items
        if (method == "GET" && uri == "/items") {
            std::lock_guard<std::mutex> lock(g_store.mtx);
            sendJSON(resp, 200, itemsToJSON());
            return;
        }

        // GET /items/{id}
        if (method == "GET" && uri.rfind("/items/", 0) == 0) {
            int id = std::stoi(uri.substr(7));
            std::lock_guard<std::mutex> lock(g_store.mtx);
            auto it = g_store.items.find(id);
            if (it == g_store.items.end()) {
                sendJSON(resp, 404, R"({"error":"not found"})");
            } else {
                std::ostringstream oss;
                oss << "{\"id\":" << id << ",\"name\":\"" << it->second << "\"}";
                sendJSON(resp, 200, oss.str());
            }
            return;
        }

        // POST /items   body: {"name":"..."}
        if (method == "POST" && uri == "/items") {
            std::istream& body = req.stream();
            std::string raw;
            Poco::StreamCopier::copyToString(body, raw);
            try {
                Parser parser;
                auto result = parser.parse(raw);
                auto obj    = result.extract<Object::Ptr>();
                std::string name = obj->getValue<std::string>("name");
                std::lock_guard<std::mutex> lock(g_store.mtx);
                int id = g_store.nextId++;
                g_store.items[id] = name;
                std::ostringstream oss;
                oss << "{\"id\":" << id << ",\"name\":\"" << name << "\"}";
                sendJSON(resp, 201, oss.str());
            } catch (...) {
                sendJSON(resp, 400, R"({"error":"invalid JSON or missing 'name'"})");
            }
            return;
        }

        // PUT /items/{id}   body: {"name":"..."}
        if (method == "PUT" && uri.rfind("/items/", 0) == 0) {
            int id = std::stoi(uri.substr(7));
            std::istream& body = req.stream();
            std::string raw;
            Poco::StreamCopier::copyToString(body, raw);
            try {
                Parser parser;
                auto result = parser.parse(raw);
                auto obj    = result.extract<Object::Ptr>();
                std::string name = obj->getValue<std::string>("name");
                std::lock_guard<std::mutex> lock(g_store.mtx);
                auto it = g_store.items.find(id);
                if (it == g_store.items.end()) {
                    sendJSON(resp, 404, R"({"error":"not found"})");
                } else {
                    it->second = name;
                    std::ostringstream oss;
                    oss << "{\"id\":" << id << ",\"name\":\"" << name << "\"}";
                    sendJSON(resp, 200, oss.str());
                }
            } catch (...) {
                sendJSON(resp, 400, R"({"error":"invalid JSON or missing 'name'"})");
            }
            return;
        }

        // DELETE /items/{id}
        if (method == "DELETE" && uri.rfind("/items/", 0) == 0) {
            int id = std::stoi(uri.substr(7));
            std::lock_guard<std::mutex> lock(g_store.mtx);
            if (g_store.items.erase(id) == 0) {
                sendJSON(resp, 404, R"({"error":"not found"})");
            } else {
                sendJSON(resp, 200, R"({"result":"deleted"})");
            }
            return;
        }

        // 404 fallback
        sendJSON(resp, 404, R"({"error":"endpoint not found"})");
    }
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
class RestHandlerFactory : public HTTPRequestHandlerFactory {
public:
    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest&) override {
        return new RestHandler();
    }
};

// ---------------------------------------------------------------------------
// Application entry point
// ---------------------------------------------------------------------------
class RestServerApp : public ServerApplication {
protected:
    int main(const std::vector<std::string>&) override {

        // 1. Kick off ffmpeg in background
        Poco::ProcessHandle ph = launchFFmpeg();

        // 2. Wait until the .m3u8 appears before accepting players
        if (!waitForPlaylist(30)) {
            std::cerr << "Aborting — ffmpeg did not produce a playlist.\n";
            return Application::EXIT_SOFTWARE;
        }

        // 3. Start HTTP server
        ServerSocket socket(PORT);
        HTTPServerParams* params = new HTTPServerParams();
        params->setMaxQueued(100);
        params->setMaxThreads(16);

        HTTPServer server(new RestHandlerFactory(), socket, params);
        server.start();

        std::cout << "\n=== Server listening on http://localhost:" << PORT << " ===\n"
                  << "Stream URL (open in VLC):\n"
                  << "  http://localhost:" << PORT << "/stream/playlist.m3u8\n\n"
                  << "REST endpoints (unchanged):\n"
                  << "  GET    /items\n"
                  << "  GET    /items/{id}\n"
                  << "  POST   /items        body: {\"name\":\"...\"}\n"
                  << "  PUT    /items/{id}   body: {\"name\":\"...\"}\n"
                  << "  DELETE /items/{id}\n\n"
                  << "Press Ctrl+C to stop.\n";

        waitForTerminationRequest();

        server.stop();

        // Clean up ffmpeg if still running
        try { Poco::Process::kill(ph); } catch (...) {}

        return Application::EXIT_OK;
    }
};

POCO_SERVER_MAIN(RestServerApp)