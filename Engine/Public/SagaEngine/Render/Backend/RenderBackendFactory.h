/// @file RenderBackendFactory.h
/// @brief Public factory surface for concrete render backend creation.

#pragma once

#include "SagaEngine/Render/Backend/IRenderBackend.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace SagaEngine::Render::Backend
{

/// Selects which graphics API the Diligent backend instantiates.
enum class DiligentBackendAPI : std::uint8_t
{
    kAuto   = 0,
    kD3D12  = 1,
    kVulkan = 2,
    kD3D11  = 3,
    kOpenGL = 4,
};

/// Converts an API enum to a human-readable tag for logging and asserts.
[[nodiscard]] std::string_view ToString(DiligentBackendAPI api) noexcept;

/// Extra Diligent backend knobs beyond the generic swapchain description.
struct DiligentBackendConfig
{
    DiligentBackendAPI preferredAPI     = DiligentBackendAPI::kAuto;
    bool               enableValidation = false;
    float              clearColor[4]    = {0.04f, 0.05f, 0.08f, 1.0f};
    float              clearDepth       = 1.0f;
    std::uint8_t       clearStencil     = 0;
    bool               skipDepthClear   = false;
};

/// Public diagnostic readout for a Diligent backend instance.
struct DiligentBackendStatus
{
    DiligentBackendAPI selectedAPI   = DiligentBackendAPI::kAuto;
    std::uint64_t      frameIndex    = 0;
    bool               initialized   = false;
};

[[nodiscard]] std::unique_ptr<IRenderBackend> CreateDiligentRenderBackend();
[[nodiscard]] std::unique_ptr<IRenderBackend> CreateDiligentRenderBackend(
    DiligentBackendConfig config);

[[nodiscard]] DiligentBackendStatus GetDiligentRenderBackendStatus(
    const IRenderBackend& backend) noexcept;

[[nodiscard]] bool InitDiligentImGuiRendering(IRenderBackend& backend);
void RenderDiligentImGuiDrawData(IRenderBackend& backend, const void* drawData);
void ShutdownDiligentImGuiRendering(IRenderBackend& backend);

} // namespace SagaEngine::Render::Backend
