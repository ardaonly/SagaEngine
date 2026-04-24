/// @file RenderCore.h
/// @brief Central render configuration and shared constants.

#pragma once

#include <cstdint>

namespace SagaEngine::Render
{

/// Global render settings — shared across passes and the renderer.
struct RenderConfig
{
    std::uint32_t maxDrawItemsPerView = 8192;
    std::uint32_t shadowMapResolution = 2048;
    bool          enableVSync         = true;
    bool          enableWireframe     = false;
    float         renderScale         = 1.0f;   ///< Internal resolution scale.
};

/// Identifies a render pass in the frame schedule.
enum class RenderPassId : std::uint16_t { kInvalid = 0 };

/// Well-known render queues for sort ordering.
enum class RenderQueue : std::uint8_t
{
    Opaque      = 0,
    AlphaTest   = 25,
    Transparent = 50,
    Overlay     = 100,
};

} // namespace SagaEngine::Render
