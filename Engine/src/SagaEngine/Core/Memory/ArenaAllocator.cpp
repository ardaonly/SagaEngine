#include "SagaEngine/Core/Memory/ArenaAllocator.h"
#include <cstring>
#include <stdexcept>

namespace SagaEngine::Core {

ArenaAllocator::ArenaAllocator(size_t blockSize)
    : _blockSize(blockSize)
    , _totalAllocated(0)
    , _currentOffset(0)
    , _currentAlignment(1) {
    _blocks.emplace_back();
    _blocks.back().data = std::make_unique<uint8_t[]>(_blockSize);
    _blocks.back().size = _blockSize;
    _blocks.back().offset = 0;
}

ArenaAllocator::~ArenaAllocator() = default;

void* ArenaAllocator::Allocate(size_t size, size_t alignment) {
    size_t alignedOffset = (_currentOffset + alignment - 1) & ~(alignment - 1);
    size_t padding = alignedOffset - _currentOffset;

    if (alignedOffset + size > _blocks.back().size) {
        NewBlock(size + alignment);
        alignedOffset = 0;
        padding = 0;
    }

    void* ptr = _blocks.back().data.get() + alignedOffset;
    _currentOffset = alignedOffset + size;
    _blocks.back().offset = _currentOffset;
    _totalAllocated += size + padding;

    return ptr;
}

void ArenaAllocator::Reset() {
    for (auto& block : _blocks) {
        block.offset = 0;
    }
    _currentOffset = 0;
    _totalAllocated = 0;
}

void ArenaAllocator::NewBlock(size_t minSize) {
    size_t newSize = std::max(_blockSize, minSize);
    _blocks.emplace_back();
    _blocks.back().data = std::make_unique<uint8_t[]>(newSize);
    _blocks.back().size = newSize;
    _blocks.back().offset = 0;
    _currentOffset = 0;
}

} // namespace SagaEngine::Core