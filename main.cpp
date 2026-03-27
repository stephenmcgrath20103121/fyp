#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Logger.h"

#include <iostream>
#include <ostream>
#include <string>

using namespace Poco::Net;
using namespace Poco::Util;

// -------------------------------------------------------------------
// Request Handler — processes every incoming HTTP request
// -------------------------------------------------------------------
class MyRequestHandler : public HTTPRequestHandler {
public:
    void handleRequest(HTTPServerRequest& req, HTTPServerResponse& res) {
        //Poco::Logger& logger = Poco::Logger::get("HTTPServer");
        Application& app = Application::instance();
        app.logger().information("Request from %s: %s %s",
            req.clientAddress().toString(),
            req.getMethod(),
            req.getURI());

        res.setChunkedTransferEncoding(true);
        res.setContentType("text/html");
        res.setStatus(HTTPResponse::HTTP_OK);

        std::ostream& out = res.send();
        out << "<html><head><title>POCO HTTP Server</title></head>"
            << "<body>"
            << "<h1>Hello World</h1>"
            << "<p><b>Method:</b> " << req.getMethod() << "</p>"
            << "<p><b>URI:</b> "    << req.getURI()    << "</p>"
            << "<p><b>Client:</b> " << req.clientAddress().toString() << "</p>"
            << "</body></html>";
    }
};

// -------------------------------------------------------------------
// Request Handler Factory — creates a new handler per request
// -------------------------------------------------------------------
class MyRequestHandlerFactory : public HTTPRequestHandlerFactory {
public:
    MyRequestHandlerFactory()
    {
    }
    
    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest&) {
        return new MyRequestHandler();
    }
};

// -------------------------------------------------------------------
// Server Application — entry point
// -------------------------------------------------------------------
class HTTPServerApp : public ServerApplication {
protected:
    int main(const std::vector<std::string>&) override {
        // Configure server parameters
        HTTPServerParams* params = new HTTPServerParams;
        params->setMaxQueued(100);
        params->setMaxThreads(16);

        const Poco::UInt16 port = 8080;

        // Bind to all interfaces on the chosen port
        ServerSocket svs(port);
        HTTPServer srv(new MyRequestHandlerFactory(), svs, params);

        srv.start();
        logger().information("HTTP server listening on port %hu", port);
        logger().information("Press Ctrl+C to stop.");

        // Block until a shutdown signal is received
        waitForTerminationRequest();

        logger().information("Shutting down...");
        srv.stop();

        return Application::EXIT_OK;
    }
};

// -------------------------------------------------------------------
// Main
// -------------------------------------------------------------------
POCO_SERVER_MAIN(HTTPServerApp)