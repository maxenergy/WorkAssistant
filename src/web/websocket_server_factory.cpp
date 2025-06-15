#include "websocket_server.h"

namespace work_assistant {

// External function declarations
#ifdef DROGON_ENABLED
std::unique_ptr<IWebSocketServer> CreateDrogonWebServer();
#endif

// Factory method to create appropriate WebSocket server
std::unique_ptr<IWebSocketServer> CreateWebSocketServer() {
#ifdef DROGON_ENABLED
    // Prefer Drogon if available
    return CreateDrogonWebServer();
#else
    // Fallback to simple implementation
    return std::make_unique<SimpleWebSocketServer>();
#endif
}

} // namespace work_assistant