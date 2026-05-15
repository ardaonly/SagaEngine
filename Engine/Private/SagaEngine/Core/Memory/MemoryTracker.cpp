#include "SagaEngine/Core/Memory/MemoryTracker.h"
#include "SagaEngine/Core/Log/Log.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <thread>

#if defined(_WIN32) && defined(_DEBUG)
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")
#endif

namespace SagaEngine::Core {

thread_local bool s_InTracker = false;
thread_local const char* s_CurrentTag = "Unknown";

std::atomic<bool> MemoryTracker::s_Active{true};

namespace
{

class TrackerReentryGuard
{
public:
    TrackerReentryGuard() noexcept
        : previous_(s_InTracker)
    {
        s_InTracker = true;
    }

    ~TrackerReentryGuard() noexcept
    {
        s_InTracker = previous_;
    }

    TrackerReentryGuard(const TrackerReentryGuard&) = delete;
    TrackerReentryGuard& operator=(const TrackerReentryGuard&) = delete;

private:
    bool previous_;
};

} // namespace

MemoryTracker& MemoryTracker::Instance() {
    static MemoryTracker instance;
    return instance;
}

MemoryTracker::MemoryTracker() {
#if defined(_WIN32) && defined(_DEBUG)
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
#endif
}

MemoryTracker::~MemoryTracker() {
    if (s_Active.load(std::memory_order_relaxed))
        DumpLeaks();

#if defined(_WIN32) && defined(_DEBUG)
    SymCleanup(GetCurrentProcess());
#endif
}

void MemoryTracker::SetTag(const char* tag) {
    s_CurrentTag = tag ? tag : "Unknown";
}

void MemoryTracker::TrackAllocation(void* ptr, size_t size, const char* file, int line, const char* tag) {

    if (!s_Active.load(std::memory_order_relaxed)) return;
    if (s_InTracker) return;

    TrackerReentryGuard guard;

    AllocationInfo info{};
    info.address = ptr;
    info.size = size;
    info.file = file;
    info.line = line;
    info.tag = tag ? tag : s_CurrentTag;
    info.threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());

#if defined(_WIN32) && defined(_DEBUG)
    info.stackSize = CaptureStackBackTrace(2, 32, info.stack, nullptr);
#endif

    try
    {
        std::lock_guard lock(_mutex);
        _allocations[ptr] = info;
        _totalAllocated.fetch_add(size, std::memory_order_relaxed);
        _allocationCount.fetch_add(1, std::memory_order_relaxed);
    }
    catch (...)
    {
        // Allocation tracking must never make the user's allocation fail.
    }
}

void MemoryTracker::TrackDeallocation(void* ptr) {

    if (!s_Active.load(std::memory_order_relaxed)) return;
    if (s_InTracker) return;

    TrackerReentryGuard guard;

    std::lock_guard lock(_mutex);

    auto it = _allocations.find(ptr);
    if (it != _allocations.end()) {
        _totalAllocated.fetch_sub(it->second.size, std::memory_order_relaxed);
        _allocationCount.fetch_sub(1, std::memory_order_relaxed);
        _allocations.erase(it);
    }
}

void MemoryTracker::DumpLeaks(size_t maxEntries) {

    std::vector<AllocationInfo> snapshot;

    {
        std::lock_guard lock(_mutex);

        if (!_allocations.empty())
        {
            TrackerReentryGuard guard;
            snapshot.reserve(_allocations.size());
            for (const auto& entry : _allocations)
                snapshot.push_back(entry.second);
        }
    }

    if (snapshot.empty()) {
        LOG_INFO("MemoryTracker", "No leaks detected");
        return;
    }

    size_t leakedBytes = 0;
    for (const auto& a : snapshot)
        leakedBytes += a.size;

    const size_t entriesToShow = maxEntries < snapshot.size()
        ? maxEntries
        : snapshot.size();

    LOG_WARN("MemoryTracker",
             "MEMORY LEAKS DETECTED: %zu allocation(s), %zu byte(s). Showing %zu entr%s.",
             snapshot.size(),
             leakedBytes,
             entriesToShow,
             entriesToShow == 1 ? "y" : "ies");

    for (size_t i = 0; i < entriesToShow; ++i) {
        const auto& a = snapshot[i];

        LOG_INFO("Memory", "Address: %p Size: %zu Tag: %s Location: %s:%d Thread: %u",
                a.address,
                a.size,
                a.tag ? a.tag : "Unknown",
                a.file ? a.file : "Unknown",
                a.line,
                a.threadId);

#if defined(_WIN32) && defined(_DEBUG)

        char buffer[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;

        for (uint16_t i = 0; i < a.stackSize; i++) {

            DWORD64 displacement = 0;

            if (SymFromAddr(GetCurrentProcess(), (DWORD64)a.stack[i], &displacement, symbol)) {
                std::cout << "   -> " << symbol->Name << "\n";
            }
        }

#endif
    }

    if (entriesToShow < snapshot.size())
        LOG_WARN("MemoryTracker", "Leak dump truncated: %zu additional allocation(s) omitted.",
                 snapshot.size() - entriesToShow);
}

void MemoryTracker::Shutdown() {
    s_Active.store(false, std::memory_order_relaxed);
}

}
