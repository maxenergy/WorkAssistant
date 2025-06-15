#include "performance_monitor.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <random>
#include <cstdlib>

namespace work_assistant {

// PerformanceTimer implementation
PerformanceTimer::PerformanceTimer(const std::string& name) 
    : m_name(name)
    , m_start_time(std::chrono::high_resolution_clock::now()) {
}

PerformanceTimer::~PerformanceTimer() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - m_start_time);
    PerformanceMonitor::GetInstance().RecordTiming(m_name, duration);
}

// PerformanceMonitor implementation
PerformanceMonitor& PerformanceMonitor::GetInstance() {
    static PerformanceMonitor instance;
    return instance;
}

void PerformanceMonitor::RecordTiming(const std::string& name, std::chrono::microseconds duration) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_timings[name].push_back(duration);
    
    // Keep only recent measurements (last 1000)
    if (m_timings[name].size() > 1000) {
        m_timings[name].erase(m_timings[name].begin());
    }
}

void PerformanceMonitor::RecordMemoryUsage(const std::string& name, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memory_usage[name] = bytes;
}

void PerformanceMonitor::RecordCounter(const std::string& name, int64_t value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_counters[name] = value;
}

void PerformanceMonitor::IncrementCounter(const std::string& name, int64_t delta) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_counters[name] += delta;
}

PerformanceStats PerformanceMonitor::GetStats(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    PerformanceStats stats;
    stats.name = name;
    
    auto it = m_timings.find(name);
    if (it != m_timings.end() && !it->second.empty()) {
        const auto& timings = it->second;
        
        // Calculate statistics
        auto total_us = std::accumulate(timings.begin(), timings.end(), std::chrono::microseconds(0));
        stats.avg_time_us = total_us.count() / timings.size();
        
        auto sorted_timings = timings;
        std::sort(sorted_timings.begin(), sorted_timings.end());
        
        stats.min_time_us = sorted_timings.front().count();
        stats.max_time_us = sorted_timings.back().count();
        stats.median_time_us = sorted_timings[sorted_timings.size() / 2].count();
        
        // 95th percentile
        size_t p95_index = static_cast<size_t>(sorted_timings.size() * 0.95);
        stats.p95_time_us = sorted_timings[p95_index].count();
        
        stats.sample_count = timings.size();
    }
    
    // Add memory usage if available
    auto mem_it = m_memory_usage.find(name);
    if (mem_it != m_memory_usage.end()) {
        stats.memory_bytes = mem_it->second;
    }
    
    // Add counter if available
    auto counter_it = m_counters.find(name);
    if (counter_it != m_counters.end()) {
        stats.counter_value = counter_it->second;
    }
    
    return stats;
}

std::vector<std::string> PerformanceMonitor::GetAllMetricNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> names;
    
    for (const auto& [name, _] : m_timings) {
        names.push_back(name);
    }
    
    for (const auto& [name, _] : m_memory_usage) {
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
    }
    
    for (const auto& [name, _] : m_counters) {
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
    }
    
    std::sort(names.begin(), names.end());
    return names;
}

void PerformanceMonitor::PrintReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cout << "\n=== Performance Report ===" << std::endl;
    std::cout << std::left << std::setw(25) << "Metric"
              << std::setw(12) << "Count"
              << std::setw(12) << "Avg (μs)"
              << std::setw(12) << "Min (μs)"
              << std::setw(12) << "Max (μs)"
              << std::setw(12) << "P95 (μs)"
              << std::endl;
    std::cout << std::string(85, '-') << std::endl;
    
    for (const auto& [name, timings] : m_timings) {
        if (timings.empty()) continue;
        
        auto stats = GetStats(name);
        std::cout << std::left << std::setw(25) << name
                  << std::setw(12) << stats.sample_count
                  << std::setw(12) << std::fixed << std::setprecision(1) << stats.avg_time_us
                  << std::setw(12) << stats.min_time_us
                  << std::setw(12) << stats.max_time_us
                  << std::setw(12) << stats.p95_time_us
                  << std::endl;
    }
    
    // Print memory usage
    if (!m_memory_usage.empty()) {
        std::cout << "\n=== Memory Usage ===" << std::endl;
        std::cout << std::left << std::setw(25) << "Component"
                  << std::setw(15) << "Memory (KB)"
                  << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        
        for (const auto& [name, bytes] : m_memory_usage) {
            std::cout << std::left << std::setw(25) << name
                      << std::setw(15) << std::fixed << std::setprecision(1) << (bytes / 1024.0)
                      << std::endl;
        }
    }
    
    // Print counters
    if (!m_counters.empty()) {
        std::cout << "\n=== Counters ===" << std::endl;
        std::cout << std::left << std::setw(25) << "Counter"
                  << std::setw(15) << "Value"
                  << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        
        for (const auto& [name, value] : m_counters) {
            std::cout << std::left << std::setw(25) << name
                      << std::setw(15) << value
                      << std::endl;
        }
    }
    
    std::cout << std::endl;
}

void PerformanceMonitor::SaveReport(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return;
    }
    
    file << "Performance Report\n";
    file << "Generated: " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << "\n\n";
    
    // Write timing data
    file << "Timing Metrics:\n";
    file << "Metric,Count,Avg(μs),Min(μs),Max(μs),P95(μs)\n";
    
    for (const auto& [name, timings] : m_timings) {
        if (timings.empty()) continue;
        
        auto stats = GetStats(name);
        file << name << ","
             << stats.sample_count << ","
             << stats.avg_time_us << ","
             << stats.min_time_us << ","
             << stats.max_time_us << ","
             << stats.p95_time_us << "\n";
    }
    
    // Write memory data
    file << "\nMemory Usage:\n";
    file << "Component,Memory(bytes)\n";
    for (const auto& [name, bytes] : m_memory_usage) {
        file << name << "," << bytes << "\n";
    }
    
    // Write counter data
    file << "\nCounters:\n";
    file << "Counter,Value\n";
    for (const auto& [name, value] : m_counters) {
        file << name << "," << value << "\n";
    }
    
    file.close();
    std::cout << "Performance report saved to: " << filename << std::endl;
}

void PerformanceMonitor::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_timings.clear();
    m_memory_usage.clear();
    m_counters.clear();
}

// SystemMonitor implementation
SystemStats SystemMonitor::GetSystemStats() {
    SystemStats stats;
    
    // Get CPU usage (simplified)
    stats.cpu_usage_percent = GetCPUUsage();
    
    // Get memory usage
    GetMemoryUsage(stats.memory_used_mb, stats.memory_total_mb);
    
    // Get process info
    stats.process_id = getpid();
    GetProcessMemory(stats.process_memory_mb);
    
    return stats;
}

double SystemMonitor::GetCPUUsage() {
    // Simplified CPU usage calculation
    // In a real implementation, this would read from /proc/stat on Linux
    // or use Windows performance counters
    static double mock_cpu = 2.5;
    mock_cpu += (rand() % 20 - 10) * 0.1; // Simulate fluctuation
    return std::max(0.0, std::min(100.0, mock_cpu));
}

void SystemMonitor::GetMemoryUsage(size_t& used_mb, size_t& total_mb) {
    // Simplified memory usage
    // In a real implementation, this would read from /proc/meminfo on Linux
    // or use GlobalMemoryStatusEx on Windows
    total_mb = 8192; // 8GB mock
    used_mb = 4096 + (rand() % 1024); // Mock usage
}

void SystemMonitor::GetProcessMemory(size_t& process_mb) {
    // Simplified process memory
    // In a real implementation, this would read from /proc/self/status on Linux
    // or use GetProcessMemoryInfo on Windows
    process_mb = 150 + (rand() % 50); // Mock 150-200MB
}

// BenchmarkSuite implementation
void BenchmarkSuite::RunAllBenchmarks() {
    std::cout << "Running benchmark suite..." << std::endl;
    
    BenchmarkOCRPerformance();
    BenchmarkScreenCapture();
    BenchmarkAIAnalysis();
    BenchmarkWebInterface();
    
    std::cout << "Benchmark suite completed." << std::endl;
    PerformanceMonitor::GetInstance().PrintReport();
}

void BenchmarkSuite::BenchmarkOCRPerformance() {
    std::cout << "Benchmarking OCR performance..." << std::endl;
    
    // Simulate OCR benchmarks
    for (int i = 0; i < 10; ++i) {
        {
            PERF_TIMER("OCR_Fast_Mode");
            std::this_thread::sleep_for(std::chrono::milliseconds(50 + rand() % 50));
        }
        
        {
            PERF_TIMER("OCR_Accurate_Mode");
            std::this_thread::sleep_for(std::chrono::milliseconds(150 + rand() % 100));
        }
        
        PerformanceMonitor::GetInstance().IncrementCounter("OCR_Documents_Processed", 1);
    }
}

void BenchmarkSuite::BenchmarkScreenCapture() {
    std::cout << "Benchmarking screen capture..." << std::endl;
    
    for (int i = 0; i < 20; ++i) {
        {
            PERF_TIMER("Screen_Capture");
            std::this_thread::sleep_for(std::chrono::milliseconds(16 + rand() % 10)); // ~60 FPS
        }
        
        PerformanceMonitor::GetInstance().IncrementCounter("Frames_Captured", 1);
    }
}

void BenchmarkSuite::BenchmarkAIAnalysis() {
    std::cout << "Benchmarking AI analysis..." << std::endl;
    
    for (int i = 0; i < 5; ++i) {
        {
            PERF_TIMER("AI_Content_Analysis");
            std::this_thread::sleep_for(std::chrono::milliseconds(200 + rand() % 300));
        }
        
        PerformanceMonitor::GetInstance().IncrementCounter("AI_Analyses_Completed", 1);
    }
}

void BenchmarkSuite::BenchmarkWebInterface() {
    std::cout << "Benchmarking web interface..." << std::endl;
    
    for (int i = 0; i < 50; ++i) {
        {
            PERF_TIMER("Web_API_Request");
            std::this_thread::sleep_for(std::chrono::milliseconds(5 + rand() % 20));
        }
        
        PerformanceMonitor::GetInstance().IncrementCounter("API_Requests_Served", 1);
    }
}

} // namespace work_assistant