// include/SagaEngine/Core/Profiling/Profiler.h
#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace SagaEngine::Core {
class Profiler {
public:
    static Profiler& Instance();
    
    void BeginFrame();
    void EndFrame();
    
    void BeginSample(const char* name);
    void EndSample(const char* name);
    
    struct SampleStats {
        std::atomic<uint64_t> callCount{0};
        std::atomic<uint64_t> totalTimeNs{0};
        std::atomic<uint64_t> minTimeNs{UINT64_MAX};
        std::atomic<uint64_t> maxTimeNs{0};
    };
    
    SampleStats& GetStats(const char* name);
    void DumpReport(const char* filename);
    
private:
    std::unordered_map<std::string, SampleStats> _samples;
    std::mutex _mutex;
    std::chrono::steady_clock::time_point _frameStart;
};

#define SAGA_PROFILE_SCOPE(name) \
    ::SagaEngine::Core::Profiler::Instance().BeginSample(name); \
    struct __ProfilerGuard { \
        ~__ProfilerGuard() { ::SagaEngine::Core::Profiler::Instance().EndSample(name); } \
    } __profilerGuard
}