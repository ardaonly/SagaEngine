/// @file DiligentRenderBackend.h
/// @brief Phase 2 Diligent implementation of the IRenderBackend seam.
///
/// Layer  : SagaEngine / Render / Backend / Diligent
/// Purpose: Concrete backend with a hardcoded triangle pipeline.
///
///   Phase 2 capabilities:
///     * Initialize  — device + context + swapchain + triangle PSO/VB/CB.
///     * BeginFrame  — binds the swap-chain RTV/DSV and clears them.
///     * Submit      — updates camera CB, binds PSO/VB, Draw(3).
///                     RenderView DrawItems are still ignored; mesh/material
///                     upload becomes real in Phase 3.
///     * EndFrame    — Present() via the swap chain.
///     * Shutdown    — releases all GPU resources.
///     * OnResize    — ISwapChain::Resize(). PSO stays valid because
///                     colour/depth formats are immutable across resize.
///
///   Resource upload entry points (CreateMesh / CreateMaterial) still
///   return the invalid sentinel. Real uploads arrive in Phase 3.
///
/// Design rules:
///   - The public header does NOT leak Diligent headers into its includers.
///     All Diligent types live inside the pimpl Impl struct defined in the
///     corresponding .cpp. IRenderBackend.h's contract ("no Diligent in
///     the public seam") stays intact all the way out to call sites.
///   - Init/device errors log and return false. Per-frame draw-time errors
///     log and skip the draw — the frame lifecycle never skips EndFrame.
///   - If triangle resource creation fails (no shader compiler, driver
///     limitation), Submit gracefully skips drawing. The error is logged
///     once at init time.
///   - One renderer per process. Non-copyable, non-movable.
///
/// Not this class's problem (and explicitly out of Phase 2):
///   - Mesh / material / PSO cache, descriptor heaps, resource streaming
///   - Command-list recording, multi-thread submission
///   - Window / input — window ownership lives in Apps, never in Engine.

#pragma once

#include "SagaEngine/Render/Backend/IRenderBackend.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace SagaEngine::Render::Backend
{

// ─── Backend selection ────────────────────────────────────────────────

/// Selects which Diligent graphics API the backend instantiates. Auto
/// walks the preference list (D3D12 → Vulkan → D3D11 → OpenGL) and picks
/// the first one that (a) was compiled into this binary via the
/// diligent-core package and (b) reports a usable device at Initialize
/// time.
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

// ─── Configuration ────────────────────────────────────────────────────

/// Extra knobs the caller can pass beyond what IRenderBackend::SwapchainDesc
/// exposes. None of these tune GPU-side resources (that's Phase 2 and
/// onward); they only affect Initialize + BeginFrame clear semantics.
struct DiligentBackendConfig
{
    /// Which Diligent API to prefer. See DiligentBackendAPI doc.
    DiligentBackendAPI preferredAPI     = DiligentBackendAPI::kAuto;

    /// Enable Diligent validation / debug layers. Slower, noisier — used
    /// during local development and in CI debug jobs.
    bool               enableValidation = false;

    /// Clear colour (RGBA, linear space) applied in BeginFrame. Default
    /// is a dark slate so the "nothing was drawn yet" case is obvious.
    float              clearColor[4]    = {0.04f, 0.05f, 0.08f, 1.0f};

    /// Default depth value written during BeginFrame depth clear.
    float              clearDepth       = 1.0f;

    /// Default stencil value written during BeginFrame depth/stencil
    /// clear. Unused if the swap chain has no stencil component.
    std::uint8_t       clearStencil     = 0;

    /// Skip depth/stencil clear (e.g. tests that only verify colour path).
    bool               skipDepthClear   = false;
};

// ─── DiligentRenderBackend ────────────────────────────────────────────

/// Concrete IRenderBackend implementation. Non-copyable, non-movable.
class DiligentRenderBackend final : public IRenderBackend
{
public:
    DiligentRenderBackend();
    explicit DiligentRenderBackend(DiligentBackendConfig cfg);
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

    // ── IRenderBackend: resource upload (Phase 2) ───────────────
    [[nodiscard]] World::MeshId     CreateMesh    (const MeshAsset&)       override;
    [[nodiscard]] World::MaterialId CreateMaterial(const MaterialRuntime&) override;
    void                            DestroyMesh    (World::MeshId)         override;
    void                            DestroyMaterial(World::MaterialId)     override;

    // ── IRenderBackend: per-frame submission ────────────────────
    void BeginFrame() override;
    void Submit(const Scene::Camera&     camera,
                const Scene::RenderView& view) override;
    void EndFrame() override;

    // ── Diagnostics ─────────────────────────────────────────────

    /// API that actually got picked at Initialize time.
    [[nodiscard]] DiligentBackendAPI SelectedAPI()   const noexcept;

    /// Monotonically increasing frame index — one bump per successful
    /// EndFrame. Useful for asserting frame pumping in integration tests.
    [[nodiscard]] std::uint64_t      FrameIndex()    const noexcept;

    [[nodiscard]] bool               IsInitialized() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace SagaEngine::Render::Backend
