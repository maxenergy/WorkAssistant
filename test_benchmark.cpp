#include <iostream>
#include "performance_monitor.h"

using namespace work_assistant;

int main() {
    std::cout << "Work Assistant - Performance Benchmark" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    // Run comprehensive benchmarks
    BenchmarkSuite::RunAllBenchmarks();
    
    // Get system stats
    auto system_stats = SystemMonitor::GetSystemStats();
    std::cout << "\nSystem Information:" << std::endl;
    std::cout << "CPU Usage: " << system_stats.cpu_usage_percent << "%" << std::endl;
    std::cout << "Memory: " << system_stats.memory_used_mb << "/" 
              << system_stats.memory_total_mb << " MB" << std::endl;
    std::cout << "Process Memory: " << system_stats.process_memory_mb << " MB" << std::endl;
    
    // Save detailed report
    PerformanceMonitor::GetInstance().SaveReport("benchmark_report.csv");
    
    return 0;
}