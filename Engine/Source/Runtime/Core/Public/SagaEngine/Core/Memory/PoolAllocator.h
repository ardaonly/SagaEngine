#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>

namespace SagaEngine::Core {

template<typename T, size_t BlockSize = 256>
class PoolAllocator {
public:
    PoolAllocator() = default;
    ~PoolAllocator() = default;

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    T* Allocate();
    void Deallocate(T* ptr);
    size_t GetAllocatedCount() const { return _allocatedCount; }
    size_t GetFreeCount() const { return _freeList.size(); }

private:
    struct Block {
        std::unique_ptr<T[]> data;
        size_t index;
    };

    std::vector<Block> _blocks;
    std::vector<T*> _freeList;
    size_t _allocatedCount = 0;

    void Expand();
};

template<typename T, size_t BlockSize>
T* PoolAllocator<T, BlockSize>::Allocate() {
    if (_freeList.empty()) {
        Expand();
    }

    T* ptr = _freeList.back();
    _freeList.pop_back();
    _allocatedCount++;
    return ptr;
}

template<typename T, size_t BlockSize>
void PoolAllocator<T, BlockSize>::Deallocate(T* ptr) {
    _freeList.push_back(ptr);
    _allocatedCount--;
}

template<typename T, size_t BlockSize>
void PoolAllocator<T, BlockSize>::Expand() {
    _blocks.emplace_back();
    _blocks.back().data = std::make_unique<T[]>(BlockSize);
    _blocks.back().index = _blocks.size() - 1;

    for (size_t i = 0; i < BlockSize; ++i) {
        _freeList.push_back(&_blocks.back().data[i]);
    }
}

} // namespace SagaEngine::Core