// Engine/include/SagaEngine/Core/Profiling/Profiler.h
#pragma once
#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <cstring>

namespace SagaEngine::Core {

class Profiler {
public:
    static Profiler& Instance();
    
    void BeginFrame();
    void EndFrame();
    void BeginSample(const char* name);
    void EndSample(const char* name);
    
    void Clear();
    
    struct SampleStats {
        std::atomic<uint64_t> callCount{0};
        std::atomic<uint64_t> totalTimeNs{0};
        std::atomic<uint64_t> minTimeNs{UINT64_MAX};
        std::atomic<uint64_t> maxTimeNs{0};
        
        SampleStats() = default;
        SampleStats(const SampleStats& other)
            : callCount(other.callCount.load())
            , totalTimeNs(other.totalTimeNs.load())
            , minTimeNs(other.minTimeNs.load())
            , maxTimeNs(other.maxTimeNs.load()) {}
    };
    
    SampleStats& GetStats(const char* name);
    void DumpReport(const char* filename);

private:
    Profiler() = default;
    
    struct SampleEntry {
        const char* name;
        std::chrono::steady_clock::time_point startTime;
    };
    
    std::unordered_map<std::string, SampleStats> _samples;
    
    static thread_local std::vector<SampleEntry> _sampleStack;
    
    mutable std::mutex _mutex;
    std::chrono::steady_clock::time_point _frameStart;
};

#define SAGA_PROFILE_SCOPE(name) \
    ::SagaEngine::Core::Profiler::Instance().BeginSample(name); \
    struct __ProfilerGuard_##__LINE__ { \
        ~__ProfilerGuard_##__LINE__() { \
            ::SagaEngine::Core::Profiler::Instance().EndSample(name); \
        } \
    } __profilerGuard_##__LINE__

} // namespace SagaEngine::Core