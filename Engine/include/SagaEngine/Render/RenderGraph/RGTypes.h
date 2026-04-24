/// @file RGTypes.h
/// @brief Core types for the render-graph system.

#pragma once

#include "SagaEngine/RHI/Types/TextureTypes.h"
#include "SagaEngine/RHI/Types/BufferTypes.h"

#include <cstdint>
#include <string>

namespace SagaEngine::Render::RG
{

/// Opaque handle to a virtual resource inside the render graph.
enum class RGResourceId : std::uint32_t { kInvalid = 0 };

/// How a pass accesses a resource.
enum class RGAccess : std::uint8_t
{
    Read       = 0,
    Write      = 1,
    ReadWrite  = 2,
};

/// Resource lifetime hint — transient resources can alias memory.
enum class RGLifetime : std::uint8_t
{
    Transient  = 0,   ///< Lives only within the frame.
    Persistent = 1,   ///< Survives across frames (e.g. history buffer).
};

/// Describes a virtual texture inside the graph.
struct RGTextureDesc
{
    std::string              debugName;
    std::uint32_t            width    = 0;
    std::uint32_t            height   = 0;
    RHI::TextureFormat       format   = RHI::TextureFormat::RGBA8_UNorm;
    RHI::TextureUsageFlags   usage    = RHI::TextureUsageFlags::RenderTarget;
    RGLifetime               lifetime = RGLifetime::Transient;
};

/// Describes a virtual buffer inside the graph.
struct RGBufferDesc
{
    std::string              debugName;
    std::uint64_t            sizeBytes = 0;
    RHI::BufferUsage         usage     = RHI::BufferUsage::Vertex;
    RGLifetime               lifetime  = RGLifetime::Transient;
};

} // namespace SagaEngine::Render::RG
