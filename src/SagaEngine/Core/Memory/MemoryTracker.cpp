#include "SagaEngine/Core/Memory/MemoryTracker.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>

namespace SagaEngine::Core {

// Definition of extern thread_local flag (NO static)
thread_local bool s_InTracker = false;

MemoryTracker& MemoryTracker::Instance() {
    static MemoryTracker instance;
    return instance;
}

void MemoryTracker::TrackAllocation(void* ptr, size_t size, const char* file, int line, const char* tag) {
    if (s_InTracker) return;
    s_InTracker = true;
    
    std::lock_guard lock(_mutex);
    _allocations[ptr] = {size, file, line, tag ? tag : _currentTag};
    _totalAllocated.fetch_add(size, std::memory_order_relaxed);
    _allocationCount.fetch_add(1, std::memory_order_relaxed);
    size_t current = _totalAllocated.load(std::memory_order_relaxed);
    size_t peak = _peakUsage.load(std::memory_order_relaxed);
    while (current > peak && !_peakUsage.compare_exchange_weak(peak, current)) {}
    
    s_InTracker = false;
}

void MemoryTracker::TrackDeallocation(void* ptr) {
    if (s_InTracker) return;
    s_InTracker = true;
    
    std::lock_guard lock(_mutex);
    auto it = _allocations.find(ptr);
    if (it != _allocations.end()) {
        _totalAllocated.fetch_sub(it->second.size, std::memory_order_relaxed);
        _allocationCount.fetch_sub(1, std::memory_order_relaxed);
        _allocations.erase(it);
    }
    
    s_InTracker = false;
}

void MemoryTracker::SetTag(const char* tag) {
    _currentTag = tag;
}

void MemoryTracker::DumpLeaks() {
    std::lock_guard lock(_mutex);
    if (_allocations.empty()) {
        std::cout << "[MemoryTracker] No leaks detected." << std::endl;
        return;
    }
    std::cout << "[MemoryTracker] LEAK DETECTED: " << _allocations.size() << " allocations" << std::endl;
    std::cout << std::left << std::setw(20) << "Address"
              << std::setw(15) << "Size"
              << std::setw(30) << "Tag"
              << std::setw(40) << "Location" << std::endl;
    for (const auto& [ptr, info] : _allocations) {
        std::cout << std::hex << ptr << std::dec
                  << std::setw(15) << info.size
                  << std::setw(30) << (info.tag ? info.tag : "Unknown")
                  << info.file << ":" << info.line << std::endl;
    }
}

MemoryTracker::~MemoryTracker() {
    if (!_allocations.empty()) {
        DumpLeaks();
    }
}

}

// Global operator new/delete with recursion prevention
void* operator new(size_t size) {
    void* ptr = std::malloc(size);
    if (!SagaEngine::Core::s_InTracker) {
        SagaEngine::Core::MemoryTracker::Instance().TrackAllocation(ptr, size, "Unknown", 0, nullptr);
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (!SagaEngine::Core::s_InTracker) {
        SagaEngine::Core::MemoryTracker::Instance().TrackDeallocation(ptr);
    }
    std::free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    if (!SagaEngine::Core::s_InTracker) {
        SagaEngine::Core::MemoryTracker::Instance().TrackDeallocation(ptr);
    }
    std::free(ptr);
}