#include <iostream>
#include <csignal>
#include "application.h"

using namespace work_assistant;

// Global application instance for signal handling
Application* g_app = nullptr;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_app) {
        g_app->Shutdown();
    }
    exit(0);
}

int main() {
    std::cout << "Work Study Assistant - Starting..." << std::endl;
    
    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Create and initialize application
    Application app;
    g_app = &app;
    
    if (!app.Initialize()) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }
    
    // Run application
    app.Run();
    
    return 0;
}