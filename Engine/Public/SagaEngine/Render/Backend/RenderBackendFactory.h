/// @file RenderBackendFactory.h
/// @brief Public factory surface for concrete render backend creation.

#pragma once

#include "SagaEngine/Render/Backend/IRenderBackend.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace SagaEngine::Render::Backend
{

/// Selects the requested native GPU backend profile.
enum class GraphicsBackendAPI : std::uint8_t
{
    kAuto           = 0,
    kNativePrimary  = 1,
    kNativePortable = 2,
    kNativeLegacy   = 3,
    kCompatibility  = 4,
};

/// Converts an API enum to a human-readable tag for logging and asserts.
[[nodiscard]] std::string_view ToString(GraphicsBackendAPI api) noexcept;

/// Extra backend knobs beyond the generic swapchain description.
struct RenderBackendConfig
{
    GraphicsBackendAPI preferredAPI     = GraphicsBackendAPI::kAuto;
    bool               enableValidation = false;
    float              clearColor[4]    = {0.04f, 0.05f, 0.08f, 1.0f};
    float              clearDepth       = 1.0f;
    std::uint8_t       clearStencil     = 0;
    bool               skipDepthClear   = false;
};

/// Public diagnostic readout for a backend instance.
struct RenderBackendStatus
{
    GraphicsBackendAPI selectedAPI   = GraphicsBackendAPI::kAuto;
    std::uint64_t      frameIndex    = 0;
    bool               initialized   = false;
};

[[nodiscard]] std::unique_ptr<IRenderBackend> CreateRenderBackend();
[[nodiscard]] std::unique_ptr<IRenderBackend> CreateRenderBackend(
    RenderBackendConfig config);

[[nodiscard]] RenderBackendStatus GetRenderBackendStatus(
    const IRenderBackend& backend) noexcept;

[[nodiscard]] bool InitBackendImGuiRendering(IRenderBackend& backend);
void RenderBackendImGuiDrawData(IRenderBackend& backend, const void* drawData);
void ShutdownBackendImGuiRendering(IRenderBackend& backend);

} // namespace SagaEngine::Render::Backend
