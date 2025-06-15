#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace work_assistant {

// Command line argument parsing and handling
class CommandLineParser {
public:
    struct Option {
        std::string short_name;     // e.g., "h"
        std::string long_name;      // e.g., "help"
        std::string description;    // Help text
        bool has_value = false;     // Whether option takes a value
        bool required = false;      // Whether option is required
        std::string default_value;  // Default value if not provided
        std::function<bool(const std::string&)> validator; // Optional validator
    };
    
    CommandLineParser();
    ~CommandLineParser() = default;
    
    // Add command line options
    void AddOption(const Option& option);
    void AddOption(const std::string& short_name, const std::string& long_name, 
                   const std::string& description, bool has_value = false, bool required = false);
    
    // Parse command line arguments
    bool Parse(int argc, char* argv[]);
    
    // Get parsed values
    bool HasOption(const std::string& name) const;
    std::string GetValue(const std::string& name, const std::string& default_value = "") const;
    int GetIntValue(const std::string& name, int default_value = 0) const;
    bool GetBoolValue(const std::string& name, bool default_value = false) const;
    
    // Get positional arguments (non-option arguments)
    std::vector<std::string> GetPositionalArgs() const { return m_positional_args; }
    
    // Help and usage
    void PrintHelp() const;
    void PrintUsage() const;
    void PrintVersion() const;
    
    // Error handling
    std::string GetLastError() const { return m_last_error; }
    bool HasErrors() const { return !m_last_error.empty(); }
    
    // Program information
    void SetProgramName(const std::string& name) { m_program_name = name; }
    void SetProgramDescription(const std::string& description) { m_program_description = description; }
    void SetProgramVersion(const std::string& version) { m_program_version = version; }

private:
    std::vector<Option> m_options;
    std::unordered_map<std::string, std::string> m_values;
    std::vector<std::string> m_positional_args;
    
    std::string m_program_name;
    std::string m_program_description;
    std::string m_program_version;
    std::string m_last_error;
    
    // Helper methods
    Option* FindOption(const std::string& name);
    const Option* FindOption(const std::string& name) const;
    bool ValidateRequiredOptions();
    bool ValidateOptionValue(const Option& option, const std::string& value);
    std::string GetOptionName(const Option& option) const;
};

// Application-specific command line configuration
class WorkAssistantCommandLine {
public:
    static CommandLineParser CreateParser();
    static void SetupStandardOptions(CommandLineParser& parser);
    
    // Standard option names
    static constexpr const char* HELP = "help";
    static constexpr const char* VERSION = "version";
    static constexpr const char* CONFIG = "config";
    static constexpr const char* DATA_DIR = "data-dir";
    static constexpr const char* LOG_LEVEL = "log-level";
    static constexpr const char* DAEMON = "daemon";
    static constexpr const char* NO_GUI = "no-gui";
    static constexpr const char* WEB_PORT = "web-port";
    static constexpr const char* OCR_MODE = "ocr-mode";
    static constexpr const char* AI_MODEL = "ai-model";
    static constexpr const char* VERBOSE = "verbose";
    static constexpr const char* QUIET = "quiet";
    static constexpr const char* TEST_MODE = "test-mode";
};

} // namespace work_assistant