#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

class SimpleWebServer {
public:
    SimpleWebServer(int port = 8080) : m_port(port), m_running(false) {}
    
    bool Start() {
        m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_server_fd == 0) {
            std::cerr << "Socket creation failed" << std::endl;
            return false;
        }
        
        int opt = 1;
        if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
            std::cerr << "setsockopt failed" << std::endl;
            return false;
        }
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(m_port);
        
        if (bind(m_server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            std::cerr << "Bind failed on port " << m_port << std::endl;
            return false;
        }
        
        if (listen(m_server_fd, 3) < 0) {
            std::cerr << "Listen failed" << std::endl;
            return false;
        }
        
        m_running = true;
        std::cout << "Web server started on http://localhost:" << m_port << std::endl;
        
        // Start server thread
        m_server_thread = std::thread([this]() { this->ServerLoop(); });
        
        return true;
    }
    
    void Stop() {
        m_running = false;
        if (m_server_fd > 0) {
            close(m_server_fd);
        }
        if (m_server_thread.joinable()) {
            m_server_thread.join();
        }
    }
    
private:
    void ServerLoop() {
        while (m_running) {
            struct sockaddr_in address;
            int addrlen = sizeof(address);
            
            int new_socket = accept(m_server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket < 0) {
                if (m_running) {
                    std::cerr << "Accept failed" << std::endl;
                }
                continue;
            }
            
            HandleRequest(new_socket);
            close(new_socket);
        }
    }
    
    void HandleRequest(int socket) {
        char buffer[1024] = {0};
        read(socket, buffer, 1024);
        
        std::string request(buffer);
        std::string path = ExtractPath(request);
        
        std::string response;
        if (path == "/" || path == "/index.html") {
            response = ServeFile("web/static/index.html", "text/html");
        } else if (path == "/styles.css") {
            response = ServeFile("web/static/styles.css", "text/css");
        } else if (path == "/app.js") {
            response = ServeFile("web/static/app.js", "application/javascript");
        } else if (path.substr(0, 4) == "/api") {
            response = HandleAPIRequest(path, request);
        } else {
            response = Create404Response();
        }
        
        send(socket, response.c_str(), response.length(), 0);
    }
    
    std::string ExtractPath(const std::string& request) {
        size_t start = request.find(' ') + 1;
        size_t end = request.find(' ', start);
        if (start != std::string::npos && end != std::string::npos) {
            return request.substr(start, end - start);
        }
        return "/";
    }
    
    std::string ServeFile(const std::string& filepath, const std::string& content_type) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return Create404Response();
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        std::stringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: " << content_type << "\r\n";
        response << "Content-Length: " << content.length() << "\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "\r\n";
        response << content;
        
        return response.str();
    }
    
    std::string HandleAPIRequest(const std::string& path, const std::string& request) {
        std::string json_response;
        
        if (path == "/api/status") {
            json_response = R"({
                "status": "ok",
                "ocr_engine": "Mock PaddleOCR + MiniCPM-V",
                "ai_engine": "Mock LLaMA.cpp",
                "storage_status": "Ready",
                "web_server": "SimpleWebServer"
            })";
        } else if (path == "/api/ocr/stats") {
            json_response = R"({
                "total_processed": 125,
                "successful_extractions": 118,
                "average_processing_time_ms": 150,
                "average_confidence": 0.92
            })";
        } else if (path == "/api/activities/recent") {
            json_response = R"([
                {
                    "timestamp": "2025-06-15T12:30:00Z",
                    "window_title": "VS Code - main.cpp",
                    "content_summary": "Code editing session",
                    "work_category": "FOCUSED_WORK"
                },
                {
                    "timestamp": "2025-06-15T12:25:00Z",
                    "window_title": "Terminal - Work Assistant",
                    "content_summary": "Build and test application",
                    "work_category": "FOCUSED_WORK"
                }
            ])";
        } else {
            json_response = R"({"error": "API endpoint not found"})";
        }
        
        std::stringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << json_response.length() << "\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "\r\n";
        response << json_response;
        
        return response.str();
    }
    
    std::string Create404Response() {
        std::string content = "<html><body><h1>404 Not Found</h1></body></html>";
        std::stringstream response;
        response << "HTTP/1.1 404 Not Found\r\n";
        response << "Content-Type: text/html\r\n";
        response << "Content-Length: " << content.length() << "\r\n";
        response << "\r\n";
        response << content;
        return response.str();
    }
    
private:
    int m_port;
    int m_server_fd;
    bool m_running;
    std::thread m_server_thread;
};

int main() {
    SimpleWebServer server(8080);
    
    if (!server.Start()) {
        std::cerr << "Failed to start web server" << std::endl;
        return 1;
    }
    
    std::cout << "Press Enter to stop the server..." << std::endl;
    std::cin.get();
    
    server.Stop();
    std::cout << "Server stopped" << std::endl;
    
    return 0;
}