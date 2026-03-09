#pragma once
#include <atomic>
#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <string>

namespace SagaEngine::Core {

// Recursion prevention flag for operator new/delete
extern thread_local bool s_InTracker;

struct AllocationInfo {
    size_t size;
    const char* file;
    int line;
    const char* tag;
};

class MemoryTracker {
public:
    static MemoryTracker& Instance();
    void TrackAllocation(void* ptr, size_t size, const char* file, int line, const char* tag);
    void TrackDeallocation(void* ptr);
    void SetTag(const char* tag);
    size_t GetTotalAllocated() const { return _totalAllocated.load(std::memory_order_acquire); }
    size_t GetAllocationCount() const { return _allocationCount.load(std::memory_order_acquire); }
    size_t GetPeakUsage() const { return _peakUsage.load(std::memory_order_acquire); }
    void DumpLeaks();
private:
    MemoryTracker() = default;
    ~MemoryTracker();
    std::mutex _mutex;
    std::unordered_map<void*, AllocationInfo> _allocations;
    std::atomic<size_t> _totalAllocated{0};
    std::atomic<size_t> _allocationCount{0};
    std::atomic<size_t> _peakUsage{0};
    const char* _currentTag = "Unknown";
};

}

#define SAGA_NEW(tag) (SagaEngine::Core::MemoryTracker::Instance().SetTag(tag), new)
#define SAGA_DELETE (SagaEngine::Core::MemoryTracker::Instance().SetTag(nullptr), delete)