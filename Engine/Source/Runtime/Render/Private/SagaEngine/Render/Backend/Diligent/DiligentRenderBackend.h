/// @file DiligentRenderBackend.h
/// @brief Diligent implementation of the IRenderBackend seam.
///
/// Layer  : SagaEngine / Render / Backend / Diligent
/// Purpose: Concrete GPU backend with real mesh/material pipeline.
///
///   Current capabilities:
///     * Initialize    — device + context + swapchain + CameraCB + solid shaders.
///     * CreateMesh    — upload VB + IB from MeshAsset LOD 0, cache by MeshId.
///     * CreateMaterial— lookup/create PSO (keyed by cull/queue/depth), create SRB.
///     * BeginFrame    — bind swap-chain RTV/DSV and clear.
///     * Submit        — update CameraCB (g_ViewProj + g_Model per DrawItem),
///                       bind PSO/VB/IB, draw indexed or non-indexed.
///     * EndFrame      — Present() via the swap chain.
///     * Shutdown      — release all GPU resources.
///     * OnResize      — ISwapChain::Resize().
///
/// Design rules:
///   - The public header does NOT leak Diligent headers into its includers.
///     All Diligent types live inside the pimpl Impl struct defined in the
///     corresponding .cpp. IRenderBackend.h's contract ("no Diligent in
///     the public seam") stays intact all the way out to call sites.
///   - Init/device errors log and return false. Per-frame draw-time errors
///     log and skip the draw — the frame lifecycle never skips EndFrame.
///   - One renderer per process. Non-copyable, non-movable.
///   - Factory init is runtime-owned; shader source is kept private here.
///
/// Not this class's problem:
///   - Descriptor heaps, resource streaming, command-list multi-threading
///   - Window / input — window ownership lives in Apps, never in Engine.

#pragma once

#include "SagaEngine/Render/Backend/Diligent/DiligentDeviceServices.h"
#include "SagaEngine/Render/Backend/RenderBackendFactory.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace SagaEngine::Graphics::Backends::Diligent::Runtime
{
class DiligentGraphicsRuntime;
}

namespace SagaEngine::Render::Backend
{

struct RenderFrameDiagnostics
{
    std::uint32_t submittedDrawItems    = 0;
    std::uint32_t rejectedDrawItems     = 0;
    std::uint32_t indexedDrawCalls      = 0;
    std::uint32_t nonIndexedDrawCalls   = 0;
    std::uint32_t lastIndexedIndexCount = 0;
    std::uint32_t presentCalls          = 0;
    std::uint32_t shadowPassDrawCalls   = 0;
    std::uint32_t mainPassDrawCalls     = 0;
    std::uint32_t shadowResourceCreationCount   = 0;
    std::uint32_t shadowResourceRecreationCount = 0;
    std::uint32_t shadowMapWidth        = 0;
    std::uint32_t shadowMapHeight       = 0;
    std::uint32_t shadowPassExecuted    = 0;
    std::uint32_t shadowSamplingEnabled = 0;
    std::uint32_t invalidNormalTransformDraws = 0;
    std::uint32_t additionalFrameSubmitsRejected = 0;
    std::uint64_t gpuSubmissionSerial = 0;
    std::uint64_t gpuCompletedSerial = 0;
    std::uint64_t gpuTargetedWaitCount = 0;
    std::uint64_t gpuDeviceWideWaitCount = 0;
    std::uint64_t gpuTimelinePollCount = 0;
    std::uint64_t gpuTimelineSignalCount = 0;
};

enum class RenderPixelFormat : std::uint8_t
{
    RGBA8_UNORM,
};

struct RenderFrameCapture
{
    std::uint32_t width    = 0;
    std::uint32_t height   = 0;
    std::uint32_t rowPitch = 0;
    RenderPixelFormat format = RenderPixelFormat::RGBA8_UNORM;
    std::vector<std::uint8_t> pixels;
};

enum class RenderCaptureResult : std::uint8_t
{
    kSuccess,
    kNotInitialized,
    kBackBufferUnavailable,
    kUnsupportedFormat,
    kCopyFailed,
};

class DiligentNativeResourceOwner;

// ─── DiligentRenderBackend ────────────────────────────────────────────

/// Concrete IRenderBackend implementation. Non-copyable, non-movable.
class DiligentRenderBackend final : public IRenderBackend
{
public:
    DiligentRenderBackend();
    explicit DiligentRenderBackend(RenderBackendConfig cfg);
    ~DiligentRenderBackend() override;

    DiligentRenderBackend(const DiligentRenderBackend&)            = delete;
    DiligentRenderBackend& operator=(const DiligentRenderBackend&) = delete;
    DiligentRenderBackend(DiligentRenderBackend&&)                 = delete;
    DiligentRenderBackend& operator=(DiligentRenderBackend&&)      = delete;

    // ── IRenderBackend: lifecycle ───────────────────────────────
    [[nodiscard]] bool Initialize(const SwapchainDesc& desc) override;
    void               Shutdown() override;
    void               OnResize(std::uint32_t width,
                                 std::uint32_t height) override;

    // ── IRenderBackend: resource upload ───────────────────────────
    [[nodiscard]] World::MeshId     CreateMesh    (const MeshAsset&)       override;
    [[nodiscard]] World::MaterialId CreateMaterial(const MaterialRuntime&) override;
    void                            DestroyMesh    (World::MeshId)         override;
    void                            DestroyMaterial(World::MaterialId)     override;

    [[nodiscard]] ::SagaEngine::Render::TextureHandle
        CreateTexture(uint32_t width, uint32_t height,
                      const uint8_t* rgba) override;
    void DestroyTexture(::SagaEngine::Render::TextureHandle tex) override;

    // ── IRenderBackend: per-frame submission ────────────────────
    void BeginFrame() override;
    void Submit(const Scene::Camera&     camera,
                const Scene::RenderView& view) override;
    void EndFrame() override;

    // ── Overlay rendering ──────────────────────────────────────

    [[nodiscard]] bool InitOverlayRendering();
    [[nodiscard]] RenderOverlayTextureHandle CreateOverlayTexture(
        const RenderOverlayTextureDesc& desc,
        const std::uint8_t* rgbaPixels);
    void DestroyOverlayTexture(RenderOverlayTextureHandle texture);
    void RenderOverlayFrame(const RenderOverlayFrame& frame);
    void ShutdownOverlayRendering();

    // ── Diagnostics ─────────────────────────────────────────────

    /// API that actually got picked at Initialize time.
    [[nodiscard]] GraphicsBackendAPI SelectedAPI()   const noexcept;

    /// Monotonically increasing frame index — one bump per successful
    /// EndFrame. Useful for asserting frame pumping in integration tests.
    [[nodiscard]] std::uint64_t      FrameIndex()    const noexcept;

    [[nodiscard]] bool               IsInitialized() const noexcept;

    [[nodiscard]] RenderFrameDiagnostics LastFrameDiagnostics() const noexcept;

    [[nodiscard]]
    ::SagaEngine::Graphics::Backends::Diligent::Runtime::DiligentGraphicsRuntime&
    RuntimeForIntegrationTesting() noexcept;
    [[nodiscard]] RenderCaptureResult CaptureCurrentColorFrame(
        RenderFrameCapture& outCapture);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace SagaEngine::Render::Backend
