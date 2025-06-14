#include "storage_engine.h"
#include "ocr_engine.h"
#include "ai_engine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <filesystem>
#include <sqlite3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

// Note: This is a simplified implementation for demonstration
// In production, you would use a proper encryption library like libsodium

namespace work_assistant {

// DataRecord implementation
void DataRecord::SetStringData(const std::string& str) {
    data.assign(str.begin(), str.end());
}

std::string DataRecord::GetStringData() const {
    return std::string(data.begin(), data.end());
}

void DataRecord::SetJsonData(const std::string& json) {
    SetStringData(json);
    metadata["content_type"] = "application/json";
}

std::string DataRecord::GetJsonData() const {
    return GetStringData();
}

bool DataRecord::IsValid() const {
    return id > 0 && !data.empty() && type != static_cast<RecordType>(0);
}

// StorageConfig validation
bool StorageConfig::IsValid() const {
    if (storage_path.empty() || database_name.empty()) {
        return false;
    }
    if (security_level >= SecurityLevel::BASIC && master_password.empty()) {
        return false;
    }
    return true;
}

// SQLite encrypted storage engine implementation
class SQLiteStorageEngine : public IStorageEngine {
public:
    SQLiteStorageEngine() 
        : m_db(nullptr)
        , m_initialized(false)
        , m_statistics{} {
    }

    ~SQLiteStorageEngine() override {
        Shutdown();
    }

    bool Initialize(const StorageConfig& config) override {
        if (m_initialized) {
            return true;
        }

        if (!config.IsValid()) {
            std::cerr << "Invalid storage configuration" << std::endl;
            return false;
        }

        m_config = config;

        // Ensure storage directory exists
        if (!storage_utils::EnsureDirectoryExists(m_config.storage_path)) {
            std::cerr << "Failed to create storage directory: " << m_config.storage_path << std::endl;
            return false;
        }

        // Initialize encryption if needed
        if (m_config.security_level >= SecurityLevel::BASIC) {
            if (!InitializeEncryption()) {
                std::cerr << "Failed to initialize encryption" << std::endl;
                return false;
            }
        }

        m_initialized = true;
        std::cout << "SQLite Storage Engine initialized" << std::endl;
        return true;
    }

    void Shutdown() override {
        if (!m_initialized) {
            return;
        }

        CloseDatabase();
        m_initialized = false;
        std::cout << "SQLite Storage Engine shut down" << std::endl;
    }

    bool IsInitialized() const override {
        return m_initialized;
    }

    bool CreateDatabase() override {
        if (!m_initialized) {
            return false;
        }

        std::string db_path = m_config.storage_path + "/" + m_config.database_name;
        
        int result = sqlite3_open(db_path.c_str(), &m_db);
        if (result != SQLITE_OK) {
            std::cerr << "Failed to create database: " << sqlite3_errmsg(m_db) << std::endl;
            return false;
        }

        // Create database schema
        if (!CreateTables()) {
            std::cerr << "Failed to create database tables" << std::endl;
            sqlite3_close(m_db);
            m_db = nullptr;
            return false;
        }

        std::cout << "Database created successfully: " << db_path << std::endl;
        return true;
    }

    bool OpenDatabase(const std::string& password = "") override {
        if (!m_initialized) {
            return false;
        }

        if (m_db) {
            return true; // Already open
        }

        std::string db_path = m_config.storage_path + "/" + m_config.database_name;
        
        int result = sqlite3_open(db_path.c_str(), &m_db);
        if (result != SQLITE_OK) {
            std::cerr << "Failed to open database: " << sqlite3_errmsg(m_db) << std::endl;
            return false;
        }

        // Verify password if required
        if (m_config.require_password && !password.empty()) {
            if (!VerifyPassword(password)) {
                std::cerr << "Invalid password" << std::endl;
                sqlite3_close(m_db);
                m_db = nullptr;
                return false;
            }
        }

        // Configure SQLite settings
        ExecuteSQL("PRAGMA foreign_keys = ON");
        ExecuteSQL("PRAGMA journal_mode = WAL");
        ExecuteSQL("PRAGMA synchronous = NORMAL");

        std::cout << "Database opened successfully" << std::endl;
        return true;
    }

    bool CloseDatabase() override {
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return true;
    }

    bool BackupDatabase(const std::string& backup_path) override {
        if (!m_db) {
            return false;
        }

        // Simple file copy backup
        std::string db_path = m_config.storage_path + "/" + m_config.database_name;
        try {
            std::filesystem::copy_file(db_path, backup_path, 
                                     std::filesystem::copy_options::overwrite_existing);
            std::cout << "Database backed up to: " << backup_path << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Backup failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool RestoreDatabase(const std::string& backup_path) override {
        if (!std::filesystem::exists(backup_path)) {
            std::cerr << "Backup file not found: " << backup_path << std::endl;
            return false;
        }

        CloseDatabase();
        
        std::string db_path = m_config.storage_path + "/" + m_config.database_name;
        try {
            std::filesystem::copy_file(backup_path, db_path,
                                     std::filesystem::copy_options::overwrite_existing);
            std::cout << "Database restored from: " << backup_path << std::endl;
            return OpenDatabase();
        } catch (const std::exception& e) {
            std::cerr << "Restore failed: " << e.what() << std::endl;
            return false;
        }
    }

    uint64_t StoreRecord(const DataRecord& record) override {
        if (!m_db || !record.IsValid()) {
            return 0;
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        const char* sql = R"(
            INSERT INTO data_records (type, timestamp, session_id, metadata, data, checksum)
            VALUES (?, ?, ?, ?, ?, ?)
        )";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
            return 0;
        }

        // Bind parameters
        sqlite3_bind_int(stmt, 1, static_cast<int>(record.type));
        sqlite3_bind_int64(stmt, 2, std::chrono::duration_cast<std::chrono::seconds>(
            record.timestamp.time_since_epoch()).count());
        sqlite3_bind_text(stmt, 3, record.session_id.c_str(), -1, SQLITE_STATIC);
        
        // Serialize metadata
        std::string metadata_json = SerializeMetadata(record.metadata);
        sqlite3_bind_text(stmt, 4, metadata_json.c_str(), -1, SQLITE_STATIC);

        // Encrypt data if needed
        std::vector<uint8_t> data_to_store = record.data;
        if (m_config.security_level >= SecurityLevel::BASIC) {
            data_to_store = storage_utils::EncryptData(record.data, m_config.master_password);
        }

        sqlite3_bind_blob(stmt, 5, data_to_store.data(), data_to_store.size(), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, record.checksum.c_str(), -1, SQLITE_STATIC);

        int result = sqlite3_step(stmt);
        uint64_t record_id = 0;
        
        if (result == SQLITE_DONE) {
            record_id = sqlite3_last_insert_rowid(m_db);
        } else {
            std::cerr << "Failed to insert record: " << sqlite3_errmsg(m_db) << std::endl;
        }

        sqlite3_finalize(stmt);

        // Update statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateWriteStatistics(duration.count());

        return record_id;
    }

    bool StoreRecords(const std::vector<DataRecord>& records) override {
        if (!m_db || records.empty()) {
            return false;
        }

        // Use transaction for batch insert
        ExecuteSQL("BEGIN TRANSACTION");

        bool success = true;
        for (const auto& record : records) {
            if (StoreRecord(record) == 0) {
                success = false;
                break;
            }
        }

        if (success) {
            ExecuteSQL("COMMIT");
        } else {
            ExecuteSQL("ROLLBACK");
        }

        return success;
    }

    bool GetRecord(uint64_t id, DataRecord& record) override {
        if (!m_db) {
            return false;
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        const char* sql = R"(
            SELECT id, type, timestamp, session_id, metadata, data, checksum
            FROM data_records WHERE id = ?
        )";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int64(stmt, 1, id);

        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            found = ParseRecordFromStatement(stmt, record);
        }

        sqlite3_finalize(stmt);

        // Update statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateReadStatistics(duration.count());

        return found;
    }

    std::vector<DataRecord> QueryRecords(const QueryParams& params) override {
        std::vector<DataRecord> results;
        if (!m_db) {
            return results;
        }

        std::string sql = BuildQuerySQL(params);
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare query: " << sqlite3_errmsg(m_db) << std::endl;
            return results;
        }

        // Bind query parameters
        BindQueryParameters(stmt, params);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DataRecord record;
            if (ParseRecordFromStatement(stmt, record)) {
                results.push_back(record);
            }
        }

        sqlite3_finalize(stmt);
        return results;
    }

    bool DeleteRecord(uint64_t id) override {
        if (!m_db) {
            return false;
        }

        const char* sql = "DELETE FROM data_records WHERE id = ?";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int64(stmt, 1, id);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);

        return success;
    }

    bool DeleteRecords(const QueryParams& params) override {
        // Implementation would build DELETE query with WHERE clause
        // For now, return false as this is a dangerous operation
        return false;
    }

    uint64_t StoreWindowActivity(const WindowActivityRecord& activity) override {
        DataRecord record = activity.ToDataRecord();
        return StoreRecord(record);
    }

    uint64_t StoreContentAnalysis(const ContentAnalysisRecord& analysis) override {
        DataRecord record = analysis.ToDataRecord();
        return StoreRecord(record);
    }

    uint64_t StoreScreenCapture(const CaptureFrame& frame, const std::string& window_title) override {
        DataRecord record;
        record.type = RecordType::SCREEN_CAPTURE;
        record.metadata["window_title"] = window_title;
        record.metadata["width"] = std::to_string(frame.width);
        record.metadata["height"] = std::to_string(frame.height);
        record.metadata["bytes_per_pixel"] = std::to_string(frame.bytes_per_pixel);
        
        // Compress image data
        if (m_config.enable_compression) {
            record.data = storage_utils::CompressData(frame.data);
            record.metadata["compressed"] = "true";
        } else {
            record.data = frame.data;
        }
        
        record.checksum = storage_utils::CalculateChecksum(record.data);
        return StoreRecord(record);
    }

    std::vector<std::string> SearchText(const std::string& query, const QueryParams& params) override {
        std::vector<std::string> results;
        // Implementation would use FTS (Full Text Search) if available
        return results;
    }

    std::vector<ContentAnalysisRecord> GetProductivityData(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) override {
        
        QueryParams params;
        params.start_time = start;
        params.end_time = end;
        params.record_types = {RecordType::AI_ANALYSIS};
        
        auto records = QueryRecords(params);
        std::vector<ContentAnalysisRecord> analyses;
        
        for (const auto& record : records) {
            if (record.type == RecordType::AI_ANALYSIS) {
                analyses.push_back(ContentAnalysisRecord::FromDataRecord(record));
            }
        }
        
        return analyses;
    }

    std::unordered_map<std::string, int> GetApplicationUsage(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) override {
        
        std::unordered_map<std::string, int> usage;
        
        QueryParams params;
        params.start_time = start;
        params.end_time = end;
        params.record_types = {RecordType::WINDOW_EVENT};
        
        auto records = QueryRecords(params);
        for (const auto& record : records) {
            if (record.metadata.find("application_name") != record.metadata.end()) {
                usage[record.metadata.at("application_name")]++;
            }
        }
        
        return usage;
    }

    bool CompactDatabase() override {
        return ExecuteSQL("VACUUM");
    }

    bool CleanupOldData() override {
        auto cutoff_time = std::chrono::system_clock::now() - m_config.data_retention_hours;
        int64_t cutoff_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            cutoff_time.time_since_epoch()).count();

        std::string sql = "DELETE FROM data_records WHERE timestamp < " + std::to_string(cutoff_timestamp);
        return ExecuteSQL(sql);
    }

    bool ReindexDatabase() override {
        return ExecuteSQL("REINDEX");
    }

    bool VerifyDataIntegrity() override {
        return ExecuteSQL("PRAGMA integrity_check");
    }

    StorageStatistics GetStatistics() override {
        UpdateStatistics();
        return m_statistics;
    }

    std::string GetEngineInfo() const override {
        return "SQLite Storage Engine v3.0 with AES-256 encryption";
    }

    StorageConfig GetConfig() const override {
        return m_config;
    }

private:
    bool InitializeEncryption() {
        // Initialize OpenSSL
        EVP_add_cipher(EVP_aes_256_cbc());
        return true;
    }

    bool CreateTables() {
        const char* create_tables_sql = R"(
            CREATE TABLE IF NOT EXISTS data_records (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                type INTEGER NOT NULL,
                timestamp INTEGER NOT NULL,
                session_id TEXT,
                metadata TEXT,
                data BLOB,
                checksum TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            );

            CREATE INDEX IF NOT EXISTS idx_records_type_timestamp ON data_records(type, timestamp);
            CREATE INDEX IF NOT EXISTS idx_records_session ON data_records(session_id);
            CREATE INDEX IF NOT EXISTS idx_records_timestamp ON data_records(timestamp);

            CREATE TABLE IF NOT EXISTS metadata (
                key TEXT PRIMARY KEY,
                value TEXT,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
            );
        )";

        return ExecuteSQL(create_tables_sql);
    }

    bool ExecuteSQL(const std::string& sql) {
        char* error_msg = nullptr;
        int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &error_msg);
        
        if (result != SQLITE_OK) {
            std::cerr << "SQL error: " << error_msg << std::endl;
            sqlite3_free(error_msg);
            return false;
        }
        return true;
    }

    bool VerifyPassword(const std::string& password) {
        // Simple password verification - in production use proper key derivation
        return password == m_config.master_password;
    }

    std::string SerializeMetadata(const std::unordered_map<std::string, std::string>& metadata) {
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        for (const auto& [key, value] : metadata) {
            if (!first) oss << ",";
            oss << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
        oss << "}";
        return oss.str();
    }

    std::string BuildQuerySQL(const QueryParams& params) {
        std::ostringstream sql;
        sql << "SELECT id, type, timestamp, session_id, metadata, data, checksum FROM data_records WHERE 1=1";
        
        // Add time range filter
        sql << " AND timestamp >= " << std::chrono::duration_cast<std::chrono::seconds>(
            params.start_time.time_since_epoch()).count();
        sql << " AND timestamp <= " << std::chrono::duration_cast<std::chrono::seconds>(
            params.end_time.time_since_epoch()).count();
        
        // Add type filter
        if (!params.record_types.empty()) {
            sql << " AND type IN (";
            for (size_t i = 0; i < params.record_types.size(); ++i) {
                if (i > 0) sql << ",";
                sql << static_cast<int>(params.record_types[i]);
            }
            sql << ")";
        }
        
        // Add ordering and limit
        sql << " ORDER BY timestamp " << (params.order_descending ? "DESC" : "ASC");
        sql << " LIMIT " << params.limit;
        if (params.offset > 0) {
            sql << " OFFSET " << params.offset;
        }
        
        return sql.str();
    }

    void BindQueryParameters(sqlite3_stmt* stmt, const QueryParams& params) {
        // Parameters are already embedded in SQL for simplicity
        // In production, use proper parameter binding
    }

    bool ParseRecordFromStatement(sqlite3_stmt* stmt, DataRecord& record) {
        record.id = sqlite3_column_int64(stmt, 0);
        record.type = static_cast<RecordType>(sqlite3_column_int(stmt, 1));
        
        int64_t timestamp = sqlite3_column_int64(stmt, 2);
        record.timestamp = std::chrono::system_clock::from_time_t(timestamp);
        
        const char* session_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (session_id) {
            record.session_id = session_id;
        }
        
        const char* metadata = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (metadata) {
            // Parse metadata JSON (simplified)
            record.metadata["raw"] = metadata;
        }
        
        const void* data_blob = sqlite3_column_blob(stmt, 5);
        int data_size = sqlite3_column_bytes(stmt, 5);
        if (data_blob && data_size > 0) {
            const uint8_t* data_ptr = static_cast<const uint8_t*>(data_blob);
            record.data.assign(data_ptr, data_ptr + data_size);
            
            // Decrypt if needed
            if (m_config.security_level >= SecurityLevel::BASIC) {
                record.data = storage_utils::DecryptData(record.data, m_config.master_password);
            }
        }
        
        const char* checksum = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (checksum) {
            record.checksum = checksum;
        }
        
        return true;
    }

    void UpdateWriteStatistics(double write_time_ms) {
        m_statistics.total_writes++;
        double total_time = m_statistics.avg_write_time_ms * (m_statistics.total_writes - 1);
        total_time += write_time_ms;
        m_statistics.avg_write_time_ms = total_time / m_statistics.total_writes;
    }

    void UpdateReadStatistics(double read_time_ms) {
        m_statistics.total_reads++;
        double total_time = m_statistics.avg_read_time_ms * (m_statistics.total_reads - 1);
        total_time += read_time_ms;
        m_statistics.avg_read_time_ms = total_time / m_statistics.total_reads;
    }

    void UpdateStatistics() {
        if (!m_db) {
            return;
        }

        // Count total records
        const char* count_sql = "SELECT COUNT(*) FROM data_records";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(m_db, count_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                m_statistics.total_records = sqlite3_column_int64(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        // Get database file size
        std::string db_path = m_config.storage_path + "/" + m_config.database_name;
        try {
            m_statistics.database_size_bytes = std::filesystem::file_size(db_path);
        } catch (...) {
            m_statistics.database_size_bytes = 0;
        }
    }

private:
    sqlite3* m_db;
    bool m_initialized;
    StorageConfig m_config;
    StorageStatistics m_statistics;
};

// Storage engine factory implementation
std::unique_ptr<IStorageEngine> StorageEngineFactory::Create(EngineType type) {
    switch (type) {
        case EngineType::SQLITE_ENCRYPTED:
            return std::make_unique<SQLiteStorageEngine>();
        case EngineType::FILE_BASED:
        case EngineType::MEMORY_ONLY:
            std::cerr << "Storage engine type not implemented yet" << std::endl;
            return nullptr;
        default:
            return nullptr;
    }
}

std::vector<StorageEngineFactory::EngineType> StorageEngineFactory::GetAvailableEngines() {
    return {EngineType::SQLITE_ENCRYPTED};
}

// Record conversion implementations
DataRecord WindowActivityRecord::ToDataRecord() const {
    DataRecord record;
    record.type = RecordType::WINDOW_EVENT;
    record.timestamp = timestamp;
    record.metadata["window_title"] = window_title;
    record.metadata["application_name"] = application_name;
    record.metadata["process_id"] = std::to_string(process_id);
    record.metadata["event_type"] = event_type;
    record.metadata["x"] = std::to_string(x);
    record.metadata["y"] = std::to_string(y);
    record.metadata["width"] = std::to_string(width);
    record.metadata["height"] = std::to_string(height);
    
    // Serialize to JSON and store as data
    std::string json = storage_utils::SerializeToJson(*this);
    record.SetJsonData(json);
    record.checksum = storage_utils::CalculateChecksum(record.data);
    
    return record;
}

WindowActivityRecord WindowActivityRecord::FromDataRecord(const DataRecord& record) {
    if (record.type != RecordType::WINDOW_EVENT) {
        return WindowActivityRecord();
    }
    
    std::string json = record.GetJsonData();
    return storage_utils::DeserializeWindowFromJson(json);
}

DataRecord ContentAnalysisRecord::ToDataRecord() const {
    DataRecord record;
    record.type = RecordType::AI_ANALYSIS;
    record.timestamp = timestamp;
    record.session_id = session_id;
    record.metadata["window_title"] = window_title;
    record.metadata["application_name"] = application_name;
    record.metadata["content_type"] = ai_utils::ContentTypeToString(content_type);
    record.metadata["work_category"] = ai_utils::WorkCategoryToString(work_category);
    record.metadata["is_productive"] = is_productive ? "true" : "false";
    
    // Serialize to JSON and store as data
    std::string json = storage_utils::SerializeToJson(*this);
    record.SetJsonData(json);
    record.checksum = storage_utils::CalculateChecksum(record.data);
    
    return record;
}

ContentAnalysisRecord ContentAnalysisRecord::FromDataRecord(const DataRecord& record) {
    if (record.type != RecordType::AI_ANALYSIS) {
        return ContentAnalysisRecord();
    }
    
    std::string json = record.GetJsonData();
    return storage_utils::DeserializeFromJson(json);
}

} // namespace work_assistant