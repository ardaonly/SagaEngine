#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace SagaEngine::Core {

class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t blockSize = 1024 * 1024);
    ~ArenaAllocator();

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
    void Reset();
    size_t GetTotalAllocated() const { return _totalAllocated; }
    size_t GetBlockCount() const { return _blocks.size(); }

    template<typename T, typename... Args>
    T* Create(Args&&... args);

    template<typename T>
    void Destroy(T* ptr);

private:
    struct Block {
        std::unique_ptr<uint8_t[]> data;
        size_t size;
        size_t offset;
    };

    std::vector<Block> _blocks;
    size_t _blockSize;
    size_t _totalAllocated;
    size_t _currentOffset;
    size_t _currentAlignment;

    void NewBlock(size_t minSize);
};

template<typename T, typename... Args>
T* ArenaAllocator::Create(Args&&... args) {
    void* mem = Allocate(sizeof(T), alignof(T));
    return new (mem) T(std::forward<Args>(args)...);
}

template<typename T>
void ArenaAllocator::Destroy(T* ptr) {
    ptr->~T();
}

} // namespace SagaEngine::Core