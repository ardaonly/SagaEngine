/// @file BufferTypes.h
/// @brief GPU buffer descriptors for the RHI abstraction layer.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEngine::RHI
{

enum class BufferUsage : std::uint8_t
{
    Vertex   = 0,
    Index    = 1,
    Uniform  = 2,
    Storage  = 3,
    Staging  = 4,
    Indirect = 5,
};

enum class BufferAccess : std::uint8_t
{
    GpuOnly  = 0,
    CpuWrite = 1,
    CpuRead  = 2,
};

struct BufferDesc
{
    std::string   debugName;
    std::uint64_t sizeBytes = 0;
    BufferUsage   usage     = BufferUsage::Vertex;
    BufferAccess  access    = BufferAccess::GpuOnly;
    std::uint32_t stride    = 0;
};

enum class BufferHandle : std::uint32_t { kInvalid = 0 };

} // namespace SagaEngine::RHI
