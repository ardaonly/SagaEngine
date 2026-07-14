/// @file RenderBackendFactory.h
/// @brief Public factory surface for concrete render backend creation.

#pragma once

#include "SagaEngine/Render/Backend/IRenderBackend.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

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

struct RenderOverlayTextureHandle
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return index != 0u && generation != 0u;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return IsValid();
    }

    [[nodiscard]] friend bool operator==(
        RenderOverlayTextureHandle,
        RenderOverlayTextureHandle) noexcept = default;
};

struct RenderOverlayTextureDesc
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t rowPitchBytes = 0;
};

struct RenderOverlayVertex
{
    float pos[2] = {0.0f, 0.0f};
    float uv[2] = {0.0f, 0.0f};
    std::uint32_t colorRgba8 = 0xFFFFFFFFu;
};

struct RenderOverlayClipRect
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct RenderOverlayDrawCommand
{
    RenderOverlayTextureHandle texture{};
    RenderOverlayClipRect clipRect{};
    std::uint32_t indexOffset = 0;
    std::uint32_t vertexOffset = 0;
    std::uint32_t elementCount = 0;
};

struct RenderOverlayDrawList
{
    std::vector<RenderOverlayVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<RenderOverlayDrawCommand> commands;
};

struct RenderOverlayFrame
{
    float displayPos[2] = {0.0f, 0.0f};
    float displaySize[2] = {0.0f, 0.0f};
    float framebufferScale[2] = {1.0f, 1.0f};
    std::vector<RenderOverlayDrawList> drawLists;
};

[[nodiscard]] std::unique_ptr<IRenderBackend> CreateRenderBackend();
[[nodiscard]] std::unique_ptr<IRenderBackend> CreateRenderBackend(
    RenderBackendConfig config);

[[nodiscard]] RenderBackendStatus GetRenderBackendStatus(
    const IRenderBackend& backend) noexcept;

[[nodiscard]] bool InitBackendOverlayRendering(IRenderBackend& backend);
[[nodiscard]] RenderOverlayTextureHandle CreateBackendOverlayTexture(
    IRenderBackend& backend,
    const RenderOverlayTextureDesc& desc,
    const std::uint8_t* rgbaPixels);
void DestroyBackendOverlayTexture(
    IRenderBackend& backend,
    RenderOverlayTextureHandle texture);
void RenderBackendOverlayFrame(
    IRenderBackend& backend,
    const RenderOverlayFrame& frame);
void ShutdownBackendOverlayRendering(IRenderBackend& backend);

} // namespace SagaEngine::Render::Backend
