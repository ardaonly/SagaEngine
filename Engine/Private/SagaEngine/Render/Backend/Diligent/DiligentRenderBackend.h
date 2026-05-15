/// @file DiligentRenderBackend.h
/// @brief Phase 3 Diligent implementation of the IRenderBackend seam.
///
/// Layer  : SagaEngine / Render / Backend / Diligent
/// Purpose: Concrete GPU backend with real mesh/material pipeline.
///
///   Phase 3 capabilities:
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
///   - Factory init and shader source are split into .inl files for clarity.
///
/// Not this class's problem:
///   - Descriptor heaps, resource streaming, command-list multi-threading
///   - Window / input — window ownership lives in Apps, never in Engine.

#pragma once

#include "SagaEngine/Render/Backend/RenderBackendFactory.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace SagaEngine::Render::Backend
{

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

    // ── ImGui rendering ────────────────────────────────────────

    /// Create ImGui PSO, font atlas texture, and projection CB.
    /// Call once after Initialize() succeeds. Returns false on failure.
    [[nodiscard]] bool InitImGuiRendering();

    /// Render ImGui draw data. Call between Submit() and EndFrame().
    /// @param drawData  Raw pointer to ImDrawData (void* avoids leaking
    ///                  imgui.h into this engine header).
    void RenderImGuiDrawData(const void* drawData);

    /// Release all ImGui GPU resources.
    void ShutdownImGuiRendering();

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
