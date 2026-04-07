#include "SagaEngine/Core/Profiling/Profiler.h"
#include "SagaEngine/Core/Log/Log.h"
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <cstring>

namespace SagaEngine::Core {

thread_local std::vector<Profiler::SampleEntry> Profiler::_sampleStack;

Profiler& Profiler::Instance() {
    static Profiler instance;
    return instance;
}

void Profiler::BeginFrame() {
    _frameStart = std::chrono::steady_clock::now();
}

void Profiler::EndFrame() {
    auto duration = std::chrono::steady_clock::now() - _frameStart;
    auto ms = std::chrono::duration<float, std::milli>(duration).count();
    if (ms > 16.67f) {
        LOG_WARN("Profiler", "Frame time: %.2fms (target: 16.67ms)", ms);
    }
}

void Profiler::BeginSample(const char* name) {
    auto& stats = GetStats(name);
    stats.callCount.fetch_add(1, std::memory_order_relaxed);
    _sampleStack.push_back({name, std::chrono::steady_clock::now()});
}

void Profiler::EndSample(const char* name) {
    if (_sampleStack.empty()) {
        LOG_WARN("Profiler", "EndSample without BeginSample: %s", name);
        return;
    }
    
    auto& top = _sampleStack.back();
    
    if (std::strcmp(top.name, name) != 0) {
        LOG_WARN("Profiler", "Sample mismatch: expected %s, got %s", top.name, name);
        _sampleStack.pop_back();
        return;
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        endTime - top.startTime).count();
    
    auto& stats = GetStats(name);
    stats.totalTimeNs.fetch_add(duration, std::memory_order_relaxed);
    
    uint64_t currentMin = stats.minTimeNs.load(std::memory_order_relaxed);
    while (duration < currentMin) {
        if (stats.minTimeNs.compare_exchange_weak(
                currentMin, duration,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            break;
        }
    }
    
    uint64_t currentMax = stats.maxTimeNs.load(std::memory_order_relaxed);
    while (duration > currentMax) {
        if (stats.maxTimeNs.compare_exchange_weak(
                currentMax, duration,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            break;
        }
    }
    
    _sampleStack.pop_back();
}

void Profiler::Clear() {
    std::lock_guard<std::mutex> lock(_mutex);
    _samples.clear();
    _sampleStack.clear();
}

Profiler::SampleStats& Profiler::GetStats(const char* name) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _samples.find(name);
    if (it == _samples.end()) {
        it = _samples.emplace(name, SampleStats{}).first;
    }
    return it->second;
}

std::vector<std::pair<std::string, Profiler::SampleStats>>

Profiler::GetAllSamplesSnapshot() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<std::pair<std::string, SampleStats>> out;
    out.reserve(_samples.size());
    for (const auto& [n, s] : _samples)
        out.emplace_back(n, s);
    return out;
}

void Profiler::DumpReport(const char* filename) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_samples.empty()) return;
    
    std::ofstream out(filename);
    if (!out.is_open()) {
        LOG_ERROR("Profiler", "Failed to open: %s", filename);
        return;
    }
    
    std::vector<std::pair<const std::string*, SampleStats*>> sorted;
    sorted.reserve(_samples.size());
    for (auto& [name, stats] : _samples) {
        sorted.emplace_back(&name, &stats);
    }
    
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) {
            return a.second->totalTimeNs.load() > b.second->totalTimeNs.load();
        });
    
    out << "SagaEngine Profiling Report\n";
    out << "===========================\n";
    out << std::left << std::setw(40) << "Sample"
        << std::right << std::setw(12) << "Calls"
        << std::setw(15) << "Total(ms)"
        << std::setw(15) << "Avg(us)"
        << std::setw(15) << "Min(us)"
        << std::setw(15) << "Max(us)" << "\n";
    out << std::string(112, '-') << "\n";
    
    for (const auto& [namePtr, stats] : sorted) {
        uint64_t calls = stats->callCount.load();
        uint64_t totalNs = stats->totalTimeNs.load();
        if (calls == 0) continue;
        
        double totalMs = static_cast<double>(totalNs) / 1'000'000.0;
        double avgUs = static_cast<double>(totalNs) / static_cast<double>(calls) / 1'000.0;
        double minUs = static_cast<double>(stats->minTimeNs.load()) / 1'000.0;
        double maxUs = static_cast<double>(stats->maxTimeNs.load()) / 1'000.0;
        
        out << std::left << std::setw(40) << namePtr->c_str()
            << std::right << std::setw(12) << calls
            << std::setw(15) << std::fixed << std::setprecision(2) << totalMs
            << std::setw(15) << std::setprecision(2) << avgUs
            << std::setw(15) << std::setprecision(2) << minUs
            << std::setw(15) << std::setprecision(2) << maxUs << "\n";
    }
    
    LOG_INFO("Profiler", "Report: %s (%zu samples)", filename, _samples.size());
}

} // namespace SagaEngine::Core