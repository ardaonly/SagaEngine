#pragma once
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace SagaEngine::Core {

extern thread_local bool s_InTracker;
extern thread_local const char* s_CurrentTag;

struct AllocationInfo {
    void* address;
    size_t size;
    const char* file;
    int line;
    const char* tag;
    uint32_t threadId;
#if defined(_WIN32) && defined(_DEBUG)
    void* stack[32];
    uint16_t stackSize;
#endif
};

class MemoryTracker {
public:

    static MemoryTracker& Instance();

    void TrackAllocation(void* ptr, size_t size, const char* file, int line, const char* tag);
    void TrackDeallocation(void* ptr);

    void SetTag(const char* tag);

    size_t GetTotalAllocated() const { return _totalAllocated.load(std::memory_order_acquire); }
    size_t GetAllocationCount() const { return _allocationCount.load(std::memory_order_acquire); }

    void DumpLeaks();
    static void Shutdown();

private:

    MemoryTracker();
    ~MemoryTracker();

    std::mutex _mutex;
    std::vector<AllocationInfo> _allocations;

    std::atomic<size_t> _totalAllocated{0};
    std::atomic<size_t> _allocationCount{0};

    static std::atomic<bool> s_Active;
};

}

#define SAGA_NEW(tag) (SagaEngine::Core::MemoryTracker::Instance().SetTag(tag), new)
#define SAGA_DELETE (SagaEngine::Core::MemoryTracker::Instance().SetTag(nullptr), delete)