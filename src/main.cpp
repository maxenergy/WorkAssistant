#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include "application.h"
#include "command_line_parser.h"
#include "config_manager.h"
#include "directory_manager.h"
#include "daemon.h"

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

int main(int argc, char* argv[]) {
    // Parse command line arguments
    CommandLineParser parser = WorkAssistantCommandLine::CreateParser();
    
    if (!parser.Parse(argc, argv)) {
        std::cerr << "Error: " << parser.GetLastError() << std::endl;
        parser.PrintUsage();
        return 1;
    }
    
    // Handle help and version
    if (parser.HasOption(WorkAssistantCommandLine::HELP)) {
        parser.PrintHelp();
        return 0;
    }
    
    if (parser.HasOption(WorkAssistantCommandLine::VERSION)) {
        parser.PrintVersion();
        return 0;
    }
    
    // Set up logging level
    std::string log_level = parser.GetValue(WorkAssistantCommandLine::LOG_LEVEL, "info");
    bool verbose = parser.HasOption(WorkAssistantCommandLine::VERBOSE);
    bool quiet = parser.HasOption(WorkAssistantCommandLine::QUIET);
    
    if (!quiet) {
        std::cout << "Work Study Assistant v1.0.0 - Starting..." << std::endl;
        if (verbose) {
            std::cout << "Verbose mode enabled" << std::endl;
        }
    }
    
    // Initialize directory structure
    std::string data_dir = parser.GetValue(WorkAssistantCommandLine::DATA_DIR);
    if (!data_dir.empty()) {
        if (!DirectoryManager::InitializeDirectories(data_dir)) {
            std::cerr << "Failed to initialize directories in: " << data_dir << std::endl;
            return 1;
        }
    } else {
        if (!DirectoryManager::InitializeDirectories()) {
            std::cerr << "Failed to initialize default directories" << std::endl;
            return 1;
        }
    }
    
    // Initialize configuration
    ConfigManager config;
    if (!config.Initialize()) {
        std::cerr << "Failed to initialize configuration" << std::endl;
        return 1;
    }
    
    // Load custom config file if specified
    std::string config_file = parser.GetValue(WorkAssistantCommandLine::CONFIG);
    if (!config_file.empty()) {
        if (!config.LoadConfig(config_file)) {
            std::cerr << "Failed to load config file: " << config_file << std::endl;
            return 1;
        }
    }
    
    // Override config with command line options
    if (parser.HasOption(WorkAssistantCommandLine::WEB_PORT)) {
        int port = parser.GetIntValue(WorkAssistantCommandLine::WEB_PORT, 8080);
        config.SetInt(DefaultConfig::WEB_SECTION, DefaultConfig::WEB_PORT, port);
    }
    
    if (parser.HasOption(WorkAssistantCommandLine::OCR_MODE)) {
        std::string ocr_mode = parser.GetValue(WorkAssistantCommandLine::OCR_MODE);
        int mode_value = 3; // AUTO
        if (ocr_mode == "fast") mode_value = 0;
        else if (ocr_mode == "accurate") mode_value = 1;
        else if (ocr_mode == "multimodal") mode_value = 2;
        config.SetInt(DefaultConfig::OCR_SECTION, DefaultConfig::OCR_DEFAULT_MODE, mode_value);
    }
    
    if (parser.HasOption(WorkAssistantCommandLine::AI_MODEL)) {
        std::string ai_model = parser.GetValue(WorkAssistantCommandLine::AI_MODEL);
        config.SetString(DefaultConfig::AI_SECTION, DefaultConfig::AI_MODEL_PATH, ai_model);
    }
    
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
    
    // Check for test mode
    if (parser.HasOption(WorkAssistantCommandLine::TEST_MODE)) {
        if (!quiet) {
            std::cout << "Running in test mode - will exit after initialization" << std::endl;
        }
        app.Shutdown();
        return 0;
    }
    
    // Check for daemon mode
    bool daemon_mode = parser.HasOption(WorkAssistantCommandLine::DAEMON);
    bool no_gui = parser.HasOption(WorkAssistantCommandLine::NO_GUI);
    
    if (daemon_mode) {
        if (!quiet) {
            std::cout << "Starting in daemon mode..." << std::endl;
        }
        
        // Create daemon service
        auto daemon = DaemonServiceFactory::Create();
        
        // Set up daemon functions
        daemon->SetMainFunction([&app]() {
            app.Run();
        });
        
        daemon->SetShutdownFunction([&app]() {
            app.Shutdown();
        });
        
        // Start daemon
        if (!daemon->StartDaemon()) {
            std::cerr << "Failed to start daemon" << std::endl;
            return 1;
        }
        
        // Keep main process alive
        while (daemon->IsDaemonRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        return 0;
    }
    
    if (no_gui) {
        if (!quiet) {
            std::cout << "Running without GUI" << std::endl;
        }
    }
    
    // Run application normally
    if (!quiet) {
        std::cout << "Application initialized successfully. ";
        if (config.GetBool(DefaultConfig::WEB_SECTION, DefaultConfig::WEB_ENABLED, true)) {
            int port = config.GetInt(DefaultConfig::WEB_SECTION, DefaultConfig::WEB_PORT, 8080);
            std::cout << "Web interface available at http://localhost:" << port;
        }
        std::cout << std::endl;
    }
    
    app.Run();
    
    return 0;
}