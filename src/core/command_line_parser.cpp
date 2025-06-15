#include "command_line_parser.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace work_assistant {

CommandLineParser::CommandLineParser() 
    : m_program_name("work_assistant")
    , m_program_description("Work Study Assistant - Intelligent productivity monitoring")
    , m_program_version("1.0.0") {
}

void CommandLineParser::AddOption(const Option& option) {
    m_options.push_back(option);
}

void CommandLineParser::AddOption(const std::string& short_name, const std::string& long_name, 
                                 const std::string& description, bool has_value, bool required) {
    Option option;
    option.short_name = short_name;
    option.long_name = long_name;
    option.description = description;
    option.has_value = has_value;
    option.required = required;
    
    AddOption(option);
}

bool CommandLineParser::Parse(int argc, char* argv[]) {
    m_values.clear();
    m_positional_args.clear();
    m_last_error.clear();
    
    if (argc > 0) {
        m_program_name = argv[0];
        
        // Extract just the program name (remove path)
        size_t last_slash = m_program_name.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            m_program_name = m_program_name.substr(last_slash + 1);
        }
    }
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg.empty()) {
            continue;
        }
        
        // Check for long option (--option)
        if (arg.substr(0, 2) == "--") {
            std::string option_name = arg.substr(2);
            std::string value;
            
            // Check for --option=value format
            size_t equals_pos = option_name.find('=');
            if (equals_pos != std::string::npos) {
                value = option_name.substr(equals_pos + 1);
                option_name = option_name.substr(0, equals_pos);
            }
            
            Option* option = FindOption(option_name);
            if (!option) {
                m_last_error = "Unknown option: --" + option_name;
                return false;
            }
            
            if (option->has_value) {
                if (value.empty()) {
                    // Value should be in next argument
                    if (i + 1 >= argc) {
                        m_last_error = "Option --" + option_name + " requires a value";
                        return false;
                    }
                    value = argv[++i];
                }
                
                if (!ValidateOptionValue(*option, value)) {
                    return false;
                }
                
                m_values[option_name] = value;
            } else {
                if (!value.empty()) {
                    m_last_error = "Option --" + option_name + " does not take a value";
                    return false;
                }
                m_values[option_name] = "true";
            }
        }
        // Check for short option (-o)
        else if (arg[0] == '-' && arg.length() > 1 && arg[1] != '-') {
            std::string options = arg.substr(1);
            
            for (size_t j = 0; j < options.length(); ++j) {
                std::string option_name(1, options[j]);
                Option* option = FindOption(option_name);
                
                if (!option) {
                    m_last_error = "Unknown option: -" + option_name;
                    return false;
                }
                
                if (option->has_value) {
                    std::string value;
                    
                    // Value can be in same argument (-ovalue) or next argument
                    if (j + 1 < options.length()) {
                        value = options.substr(j + 1);
                        j = options.length(); // Consume rest of string
                    } else {
                        if (i + 1 >= argc) {
                            m_last_error = "Option -" + option_name + " requires a value";
                            return false;
                        }
                        value = argv[++i];
                    }
                    
                    if (!ValidateOptionValue(*option, value)) {
                        return false;
                    }
                    
                    m_values[option->long_name.empty() ? option->short_name : option->long_name] = value;
                } else {
                    m_values[option->long_name.empty() ? option->short_name : option->long_name] = "true";
                }
            }
        }
        // Positional argument
        else {
            m_positional_args.push_back(arg);
        }
    }
    
    // Validate required options
    if (!ValidateRequiredOptions()) {
        return false;
    }
    
    return true;
}

bool CommandLineParser::HasOption(const std::string& name) const {
    return m_values.find(name) != m_values.end();
}

std::string CommandLineParser::GetValue(const std::string& name, const std::string& default_value) const {
    auto it = m_values.find(name);
    if (it != m_values.end()) {
        return it->second;
    }
    
    // Check if option has a default value
    const Option* option = FindOption(name);
    if (option && !option->default_value.empty()) {
        return option->default_value;
    }
    
    return default_value;
}

int CommandLineParser::GetIntValue(const std::string& name, int default_value) const {
    std::string value = GetValue(name);
    if (value.empty()) {
        return default_value;
    }
    
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        return default_value;
    }
}

bool CommandLineParser::GetBoolValue(const std::string& name, bool default_value) const {
    if (!HasOption(name)) {
        return default_value;
    }
    
    std::string value = GetValue(name);
    if (value == "true" || value == "1") {
        return true;
    } else if (value == "false" || value == "0") {
        return false;
    }
    
    // For boolean flags, presence means true
    return true;
}

void CommandLineParser::PrintHelp() const {
    PrintUsage();
    
    std::cout << "\nDescription:\n";
    std::cout << "  " << m_program_description << "\n\n";
    
    if (!m_options.empty()) {
        std::cout << "Options:\n";
        
        // Calculate maximum width for alignment
        size_t max_width = 0;
        for (const auto& option : m_options) {
            std::string option_str;
            if (!option.short_name.empty()) {
                option_str += "-" + option.short_name;
                if (!option.long_name.empty()) {
                    option_str += ", ";
                }
            }
            if (!option.long_name.empty()) {
                option_str += "--" + option.long_name;
            }
            if (option.has_value) {
                option_str += " VALUE";
            }
            max_width = std::max(max_width, option_str.length());
        }
        
        for (const auto& option : m_options) {
            std::string option_str;
            if (!option.short_name.empty()) {
                option_str += "-" + option.short_name;
                if (!option.long_name.empty()) {
                    option_str += ", ";
                }
            }
            if (!option.long_name.empty()) {
                option_str += "--" + option.long_name;
            }
            if (option.has_value) {
                option_str += " VALUE";
            }
            
            std::cout << "  " << std::left << std::setw(max_width + 2) << option_str;
            std::cout << option.description;
            
            if (option.required) {
                std::cout << " (required)";
            }
            if (!option.default_value.empty()) {
                std::cout << " (default: " << option.default_value << ")";
            }
            
            std::cout << "\n";
        }
    }
    
    std::cout << "\nExamples:\n";
    std::cout << "  " << m_program_name << " --help\n";
    std::cout << "  " << m_program_name << " --config /path/to/config.conf\n";
    std::cout << "  " << m_program_name << " --daemon --web-port 8080\n";
    std::cout << "  " << m_program_name << " --no-gui --ocr-mode fast\n";
    std::cout << "\n";
}

void CommandLineParser::PrintUsage() const {
    std::cout << "Usage: " << m_program_name << " [OPTIONS]";
    
    bool has_required = false;
    for (const auto& option : m_options) {
        if (option.required) {
            has_required = true;
            break;
        }
    }
    
    if (has_required) {
        std::cout << " REQUIRED_OPTIONS";
    }
    
    std::cout << "\n";
}

void CommandLineParser::PrintVersion() const {
    std::cout << m_program_name << " version " << m_program_version << "\n";
}

CommandLineParser::Option* CommandLineParser::FindOption(const std::string& name) {
    for (auto& option : m_options) {
        if (option.short_name == name || option.long_name == name) {
            return &option;
        }
    }
    return nullptr;
}

const CommandLineParser::Option* CommandLineParser::FindOption(const std::string& name) const {
    for (const auto& option : m_options) {
        if (option.short_name == name || option.long_name == name) {
            return &option;
        }
    }
    return nullptr;
}

bool CommandLineParser::ValidateRequiredOptions() {
    for (const auto& option : m_options) {
        if (option.required) {
            std::string key = option.long_name.empty() ? option.short_name : option.long_name;
            if (m_values.find(key) == m_values.end()) {
                m_last_error = "Required option missing: " + GetOptionName(option);
                return false;
            }
        }
    }
    return true;
}

bool CommandLineParser::ValidateOptionValue(const Option& option, const std::string& value) {
    if (option.validator && !option.validator(value)) {
        m_last_error = "Invalid value for option " + GetOptionName(option) + ": " + value;
        return false;
    }
    return true;
}

std::string CommandLineParser::GetOptionName(const Option& option) const {
    if (!option.long_name.empty()) {
        return "--" + option.long_name;
    } else if (!option.short_name.empty()) {
        return "-" + option.short_name;
    }
    return "unknown";
}

// WorkAssistantCommandLine implementation
CommandLineParser WorkAssistantCommandLine::CreateParser() {
    CommandLineParser parser;
    SetupStandardOptions(parser);
    return parser;
}

void WorkAssistantCommandLine::SetupStandardOptions(CommandLineParser& parser) {
    parser.SetProgramName("work_study_assistant");
    parser.SetProgramDescription("Work Study Assistant - Intelligent productivity monitoring and analysis tool");
    parser.SetProgramVersion("1.0.0");
    
    // Help and version
    parser.AddOption("h", HELP, "Show this help message");
    parser.AddOption("", VERSION, "Show version information");
    
    // Configuration
    parser.AddOption("c", CONFIG, "Configuration file path", true);
    parser.AddOption("", DATA_DIR, "Data directory path", true);
    
    // Logging
    parser.AddOption("l", LOG_LEVEL, "Log level (debug, info, warn, error)", true);
    parser.AddOption("v", VERBOSE, "Enable verbose output");
    parser.AddOption("q", QUIET, "Quiet mode (minimal output)");
    
    // Execution mode
    parser.AddOption("d", DAEMON, "Run as daemon/service");
    parser.AddOption("", NO_GUI, "Run without graphical interface");
    parser.AddOption("", TEST_MODE, "Run in test mode");
    
    // Server configuration
    parser.AddOption("p", WEB_PORT, "Web server port", true);
    
    // OCR and AI configuration
    parser.AddOption("", OCR_MODE, "OCR mode (fast, accurate, multimodal, auto)", true);
    parser.AddOption("m", AI_MODEL, "AI model file path", true);
    
    // Add validators for specific options
    auto port_validator = [](const std::string& value) {
        try {
            int port = std::stoi(value);
            return port >= 1 && port <= 65535;
        } catch (...) {
            return false;
        }
    };
    
    auto log_level_validator = [](const std::string& value) {
        return value == "debug" || value == "info" || value == "warn" || value == "error";
    };
    
    auto ocr_mode_validator = [](const std::string& value) {
        return value == "fast" || value == "accurate" || value == "multimodal" || value == "auto";
    };
    
    // Set validators (would need to modify the AddOption calls above in a real implementation)
}

} // namespace work_assistant