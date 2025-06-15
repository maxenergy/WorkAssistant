#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace work_assistant {

// Performance statistics
struct PerformanceStats {
    std::string name;
    size_t sample_count = 0;
    double avg_time_us = 0.0;
    int64_t min_time_us = 0;
    int64_t max_time_us = 0;
    int64_t median_time_us = 0;
    int64_t p95_time_us = 0;
    size_t memory_bytes = 0;
    int64_t counter_value = 0;
};

// System statistics
struct SystemStats {
    double cpu_usage_percent = 0.0;
    size_t memory_used_mb = 0;
    size_t memory_total_mb = 0;
    pid_t process_id = 0;
    size_t process_memory_mb = 0;
};

// RAII timer for automatic performance measurement
class PerformanceTimer {
public:
    explicit PerformanceTimer(const std::string& name);
    ~PerformanceTimer();

private:
    std::string m_name;
    std::chrono::high_resolution_clock::time_point m_start_time;
};

// Convenience macro for performance timing
#define PERF_TIMER(name) PerformanceTimer _timer(name)

// Performance monitor singleton
class PerformanceMonitor {
public:
    static PerformanceMonitor& GetInstance();
    
    // Record measurements
    void RecordTiming(const std::string& name, std::chrono::microseconds duration);
    void RecordMemoryUsage(const std::string& name, size_t bytes);
    void RecordCounter(const std::string& name, int64_t value);
    void IncrementCounter(const std::string& name, int64_t delta = 1);
    
    // Get statistics
    PerformanceStats GetStats(const std::string& name) const;
    std::vector<std::string> GetAllMetricNames() const;
    
    // Reporting
    void PrintReport() const;
    void SaveReport(const std::string& filename) const;
    void Reset();
    
private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::vector<std::chrono::microseconds>> m_timings;
    std::unordered_map<std::string, size_t> m_memory_usage;
    std::unordered_map<std::string, int64_t> m_counters;
};

// System monitoring utilities
class SystemMonitor {
public:
    static SystemStats GetSystemStats();
    static double GetCPUUsage();
    static void GetMemoryUsage(size_t& used_mb, size_t& total_mb);
    static void GetProcessMemory(size_t& process_mb);
};

// Benchmark suite for performance testing
class BenchmarkSuite {
public:
    static void RunAllBenchmarks();
    static void BenchmarkOCRPerformance();
    static void BenchmarkScreenCapture();
    static void BenchmarkAIAnalysis();
    static void BenchmarkWebInterface();
};

} // namespace work_assistant