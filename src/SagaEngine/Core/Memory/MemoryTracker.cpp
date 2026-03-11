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

    s_InTracker = true;

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

    {
        std::lock_guard lock(_mutex);
        _allocations.push_back(info);
    }

    _totalAllocated.fetch_add(size, std::memory_order_relaxed);
    _allocationCount.fetch_add(1, std::memory_order_relaxed);

    s_InTracker = false;
}

void MemoryTracker::TrackDeallocation(void* ptr) {

    if (!s_Active.load(std::memory_order_relaxed)) return;
    if (s_InTracker) return;

    s_InTracker = true;

    std::lock_guard lock(_mutex);

    for (auto it = _allocations.begin(); it != _allocations.end(); ++it) {
        if (it->address == ptr) {
            _totalAllocated.fetch_sub(it->size, std::memory_order_relaxed);
            _allocationCount.fetch_sub(1, std::memory_order_relaxed);
            _allocations.erase(it);
            break;
        }
    }

    s_InTracker = false;
}

void MemoryTracker::DumpLeaks() {

    std::lock_guard lock(_mutex);

    if (_allocations.empty()) {
        LOG_INFO("[MemoryTracker]", "No leaks detected");
        return;
    }

    LOG_WARN("[MemoryTracker]", "MEMORY LEAKS DETECTED");

    for (const auto& a : _allocations) {

        LOG_INFO("Memory", "Address: %p Size: %zu Tag: %s Location: %s:%d\n",
                a.address,
                a.size,
                a.tag ? a.tag : "Unknown",
                a.file ? a.file : "Unknown",
                a.line);LOG_INFO("Memory", "Address: %p Size: %zu Tag: %s Location: %s:%d\n",
                a.address,
                a.size,
                a.tag ? a.tag : "Unknown",
                a.file ? a.file : "Unknown",
                a.line);

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

        std::cout << "\n";
    }
}

void MemoryTracker::Shutdown() {
    s_Active.store(false, std::memory_order_relaxed);
}

}