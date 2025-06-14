#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <future>
#include <unordered_map>
#include <cstdint>
#include <functional>

namespace work_assistant {

// Forward declarations to avoid circular dependencies
struct CaptureFrame;
struct OCRDocument;
struct ContentAnalysis;
struct WindowInfo;
struct WindowEvent;

// AI-related forward declarations
enum class ContentType;
enum class WorkCategory;
enum class ActivityPriority;

// Additional forward declarations

// Data record types
enum class RecordType {
    WINDOW_EVENT = 1,
    SCREEN_CAPTURE = 2,
    OCR_RESULT = 3,
    AI_ANALYSIS = 4,
    USER_ACTION = 5,
    SYSTEM_INFO = 6
};

// Storage security levels
enum class SecurityLevel {
    NONE = 0,           // No encryption (development only)
    BASIC = 1,          // Simple encryption
    STANDARD = 2,       // AES-256 encryption
    HIGH_SECURITY = 3   // AES-256 + additional layers
};

// Data record structure
struct DataRecord {
    uint64_t id = 0;
    RecordType type = RecordType::WINDOW_EVENT;
    std::chrono::system_clock::time_point timestamp;
    std::string session_id;
    std::unordered_map<std::string, std::string> metadata;
    std::vector<uint8_t> data;
    std::string checksum;
    
    DataRecord() : timestamp(std::chrono::system_clock::now()) {}
    
    // Helper methods
    void SetStringData(const std::string& str);
    std::string GetStringData() const;
    void SetJsonData(const std::string& json);
    std::string GetJsonData() const;
    size_t GetDataSize() const { return data.size(); }
    bool IsValid() const;
};

// Window activity record
struct WindowActivityRecord {
    uint64_t id = 0;
    std::chrono::system_clock::time_point timestamp;
    std::string window_title;
    std::string application_name;
    uint32_t process_id = 0;
    int x = 0, y = 0, width = 0, height = 0;
    std::string event_type;
    std::chrono::milliseconds duration{0};
    
    // Convert to/from DataRecord
    DataRecord ToDataRecord() const;
    static WindowActivityRecord FromDataRecord(const DataRecord& record);
};

// Content analysis record
struct ContentAnalysisRecord {
    uint64_t id = 0;
    std::chrono::system_clock::time_point timestamp;
    std::string session_id;
    
    // Source information
    std::string window_title;
    std::string application_name;
    
    // OCR results
    std::string extracted_text;
    std::vector<std::string> keywords;
    float ocr_confidence = 0.0f;
    
    // AI analysis (stored as integers to avoid enum dependencies)
    int content_type = 0; // ContentType enum value
    int work_category = 0; // WorkCategory enum value
    int priority = 2; // ActivityPriority enum value (MEDIUM = 2)
    bool is_productive = false;
    bool is_focused_work = false;
    float ai_confidence = 0.0f;
    int distraction_level = 0;
    
    // Performance metrics
    std::chrono::milliseconds processing_time{0};
    
    // Convert to/from DataRecord
    DataRecord ToDataRecord() const;
    static ContentAnalysisRecord FromDataRecord(const DataRecord& record);
};

// Storage configuration
struct StorageConfig {
    std::string storage_path = "data/";
    std::string database_name = "work_assistant.db";
    SecurityLevel security_level = SecurityLevel::STANDARD;
    std::string master_password;
    
    // Data retention settings
    std::chrono::hours data_retention_hours{24 * 30}; // 30 days default
    bool auto_cleanup = true;
    size_t max_database_size_mb = 1024; // 1GB default
    
    // Performance settings
    bool enable_compression = true;
    bool enable_indexing = true;
    size_t write_buffer_size_mb = 64;
    int backup_interval_hours = 24;
    
    // Security settings
    bool require_password = true;
    int key_derivation_iterations = 100000;
    bool enable_data_integrity_checks = true;
    
    StorageConfig() = default;
    bool IsValid() const;
};

// Storage statistics
struct StorageStatistics {
    size_t total_records = 0;
    size_t window_events = 0;
    size_t screen_captures = 0;
    size_t ocr_results = 0;
    size_t ai_analyses = 0;
    
    size_t database_size_bytes = 0;
    size_t compressed_size_bytes = 0;
    float compression_ratio = 0.0f;
    
    std::chrono::system_clock::time_point oldest_record;
    std::chrono::system_clock::time_point newest_record;
    
    // Performance metrics
    double avg_write_time_ms = 0.0;
    double avg_read_time_ms = 0.0;
    size_t total_writes = 0;
    size_t total_reads = 0;
    
    StorageStatistics() = default;
};

// Query parameters for data retrieval
struct QueryParams {
    // Time range
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // Filters
    std::vector<RecordType> record_types;
    std::vector<std::string> applications;
    std::vector<int> content_types; // ContentType enum values as integers
    std::vector<int> work_categories; // WorkCategory enum values as integers
    std::string search_text;
    
    // Result options
    size_t limit = 1000;
    size_t offset = 0;
    bool include_data = true;
    bool order_descending = true;
    
    QueryParams() {
        // Default to last 24 hours
        end_time = std::chrono::system_clock::now();
        start_time = end_time - std::chrono::hours(24);
    }
};

// Storage engine interface
class IStorageEngine {
public:
    virtual ~IStorageEngine() = default;

    // Lifecycle management
    virtual bool Initialize(const StorageConfig& config) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // Database operations
    virtual bool CreateDatabase() = 0;
    virtual bool OpenDatabase(const std::string& password = "") = 0;
    virtual bool CloseDatabase() = 0;
    virtual bool BackupDatabase(const std::string& backup_path) = 0;
    virtual bool RestoreDatabase(const std::string& backup_path) = 0;

    // Record operations
    virtual uint64_t StoreRecord(const DataRecord& record) = 0;
    virtual bool StoreRecords(const std::vector<DataRecord>& records) = 0;
    virtual bool GetRecord(uint64_t id, DataRecord& record) = 0;
    virtual std::vector<DataRecord> QueryRecords(const QueryParams& params) = 0;
    virtual bool DeleteRecord(uint64_t id) = 0;
    virtual bool DeleteRecords(const QueryParams& params) = 0;

    // Specialized storage methods
    virtual uint64_t StoreWindowActivity(const WindowActivityRecord& activity) = 0;
    virtual uint64_t StoreContentAnalysis(const ContentAnalysisRecord& analysis) = 0;
    virtual uint64_t StoreScreenCapture(const CaptureFrame& frame, 
                                       const std::string& window_title = "") = 0;

    // Search and analytics
    virtual std::vector<std::string> SearchText(const std::string& query, 
                                               const QueryParams& params = QueryParams()) = 0;
    virtual std::vector<ContentAnalysisRecord> GetProductivityData(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) = 0;
    virtual std::unordered_map<std::string, int> GetApplicationUsage(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) = 0;

    // Maintenance operations
    virtual bool CompactDatabase() = 0;
    virtual bool CleanupOldData() = 0;
    virtual bool ReindexDatabase() = 0;
    virtual bool VerifyDataIntegrity() = 0;

    // Statistics and monitoring
    virtual StorageStatistics GetStatistics() = 0;
    virtual std::string GetEngineInfo() const = 0;
    virtual StorageConfig GetConfig() const = 0;
};

// Encrypted storage manager
class EncryptedStorageManager {
public:
    EncryptedStorageManager();
    ~EncryptedStorageManager();

    // Initialization
    bool Initialize(const StorageConfig& config);
    void Shutdown();
    bool IsReady() const;

    // Session management
    bool StartSession(const std::string& session_name = "");
    bool EndSession();
    std::string GetCurrentSessionId() const;

    // High-level storage operations
    bool StoreWindowEvent(const WindowEvent& event, const WindowInfo& info);
    bool StoreScreenCapture(const CaptureFrame& frame, const std::string& context = "");
    bool StoreOCRResult(const OCRDocument& document, const std::string& source = "");
    bool StoreAIAnalysis(const ContentAnalysis& analysis);

    // Batch operations
    bool StoreBatch(const std::vector<WindowActivityRecord>& activities,
                   const std::vector<ContentAnalysisRecord>& analyses);

    // Data retrieval
    std::vector<WindowActivityRecord> GetWindowActivities(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end);
    
    std::vector<ContentAnalysisRecord> GetContentAnalyses(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end);

    // Analytics and reporting
    std::unordered_map<std::string, float> GetProductivityReport(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end);
    
    std::vector<std::pair<std::string, std::chrono::minutes>> GetTimeSpentByApplication(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end);

    // Search functionality
    std::vector<ContentAnalysisRecord> SearchContent(const std::string& query,
                                                     int max_results = 100);

    // Data management
    bool ExportData(const std::string& export_path, 
                   const std::chrono::system_clock::time_point& start,
                   const std::chrono::system_clock::time_point& end);
    bool ImportData(const std::string& import_path);
    bool CleanupOldData(const std::chrono::hours& retention_period);

    // Security operations
    bool ChangePassword(const std::string& old_password, const std::string& new_password);
    bool VerifyIntegrity();
    bool CreateBackup(const std::string& backup_path);
    bool RestoreFromBackup(const std::string& backup_path);

    // Configuration and monitoring
    StorageStatistics GetStatistics() const;
    StorageConfig GetConfig() const;
    void UpdateConfig(const StorageConfig& config);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// Storage engine factory
class StorageEngineFactory {
public:
    enum class EngineType {
        SQLITE_ENCRYPTED,   // SQLite with encryption
        FILE_BASED,         // File-based storage
        MEMORY_ONLY         // In-memory (testing only)
    };
    
    static std::unique_ptr<IStorageEngine> Create(EngineType type = EngineType::SQLITE_ENCRYPTED);
    static std::vector<EngineType> GetAvailableEngines();
};

// Utility functions for storage operations
namespace storage_utils {

// Encryption helpers
std::vector<uint8_t> EncryptData(const std::vector<uint8_t>& data, 
                                const std::string& password,
                                const std::string& salt = "");
std::vector<uint8_t> DecryptData(const std::vector<uint8_t>& encrypted_data,
                                const std::string& password,
                                const std::string& salt = "");

// Compression helpers
std::vector<uint8_t> CompressData(const std::vector<uint8_t>& data);
std::vector<uint8_t> DecompressData(const std::vector<uint8_t>& compressed_data);

// Data integrity
std::string CalculateChecksum(const std::vector<uint8_t>& data);
bool VerifyChecksum(const std::vector<uint8_t>& data, const std::string& checksum);

// Serialization helpers
std::string SerializeToJson(const ContentAnalysisRecord& record);
ContentAnalysisRecord DeserializeFromJson(const std::string& json);
std::string SerializeToJson(const WindowActivityRecord& record);
WindowActivityRecord DeserializeWindowFromJson(const std::string& json);

// File operations
bool EnsureDirectoryExists(const std::string& path);
bool IsValidDatabasePath(const std::string& path);
size_t GetDirectorySize(const std::string& path);
bool SecureDeleteFile(const std::string& path);

// Time utilities
std::string FormatTimestamp(const std::chrono::system_clock::time_point& time);
std::chrono::system_clock::time_point ParseTimestamp(const std::string& timestamp);
std::string GenerateSessionId();

} // namespace storage_utils

} // namespace work_assistant