/// @file IRenderBackend.h
/// @brief Abstract seam between the API-agnostic render world and a concrete
///        graphics backend (Diligent, D3D12, Vulkan, ...).
///
/// Layer  : SagaEngine / Render / Backend
/// Purpose: The entire pipeline above this interface (RenderWorld, culling,
///          LOD, RenderView) is pure CPU-side data. This header is the one
///          place that knows "eventually someone turns DrawItems into GPU
///          calls". Until Diligent lands, a NullRenderBackend provided
///          here lets tests and headless tools run the full pipeline.
///
/// Design rules:
///   - No Diligent / D3D / Vulkan types in this header. Only engine types.
///   - The interface is deliberately minimal. Shader binding tables,
///     command recording, resource heaps — all of that lives behind the
///     concrete backend implementation. The backend receives an
///     already-built RenderView and decides how to submit it.
///   - Resource upload is one-shot calls (CreateMesh, CreateMaterial).
///     Streaming / async uploads are backend-internal concerns layered
///     on top of these primitives later.

#pragma once

#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/RenderView.h"
#include "SagaEngine/Render/World/RenderEntity.h"

#include <cstdint>

// Forward declare to avoid pulling MeshAsset / Material into every TU that
// only wants the abstract interface.
namespace SagaEngine::Render
{
struct MeshAsset;
struct MaterialAsset;
struct MaterialRuntime;
}

namespace SagaEngine::Render::Backend
{

namespace World = ::SagaEngine::Render::World;
namespace Scene = ::SagaEngine::Render::Scene;

// ─── Descriptors ──────────────────────────────────────────────────────

/// Presentation target description. The backend allocates swapchain
/// resources from this.
struct SwapchainDesc
{
    void*         nativeWindow = nullptr;   ///< HWND / NSView* / wl_surface*.
    std::uint32_t width        = 0;
    std::uint32_t height       = 0;
    bool          vsync        = true;
    bool          hdr          = false;
};

// ─── Interface ────────────────────────────────────────────────────────

/// One-renderer-per-process interface. Concrete implementation lives next
/// to the backend (e.g. DiligentRenderBackend, VulkanRenderBackend).
class IRenderBackend
{
public:
    virtual ~IRenderBackend() = default;

    // ── Lifecycle ────────────────────────────────────────────────

    [[nodiscard]] virtual bool Initialize(const SwapchainDesc& desc) = 0;
    virtual void               Shutdown()                             = 0;

    /// Handle a window resize. Recreates swapchain surfaces.
    virtual void OnResize(std::uint32_t width, std::uint32_t height) = 0;

    // ── Resource upload ──────────────────────────────────────────

    /// Upload a mesh asset and return the handle the render world will
    /// reference. Returning MeshId::kInvalid means the upload failed.
    [[nodiscard]] virtual World::MeshId CreateMesh(const MeshAsset& asset) = 0;

    /// Upload a material runtime. Returns MaterialId::kInvalid on failure.
    [[nodiscard]] virtual World::MaterialId CreateMaterial(const MaterialRuntime& runtime) = 0;

    virtual void DestroyMesh    (World::MeshId mesh)         = 0;
    virtual void DestroyMaterial(World::MaterialId material) = 0;

    // ── Per-frame submission ─────────────────────────────────────

    /// Called once per frame before any Submit(). Gives the backend a
    /// chance to rotate per-frame command allocators, ring buffers, etc.
    virtual void BeginFrame() = 0;

    /// Submit a RenderView captured for one camera. The backend is free
    /// to sort / batch draws internally before recording GPU commands.
    virtual void Submit(const Scene::Camera&      camera,
                          const Scene::RenderView&  view) = 0;

    /// Present any swapchain targets and finish the frame.
    virtual void EndFrame() = 0;
};

// ─── Null backend ─────────────────────────────────────────────────────

/// No-op backend used by tests, headless tools, and dedicated-server
/// builds that share the client pipeline. Counts submitted draws so tests
/// can assert on pipeline output without linking a real GPU backend.
class NullRenderBackend final : public IRenderBackend
{
public:
    bool Initialize(const SwapchainDesc&) override { return true; }
    void Shutdown() override {}
    void OnResize(std::uint32_t, std::uint32_t) override {}

    World::MeshId     CreateMesh    (const MeshAsset&)        override { return static_cast<World::MeshId>(m_NextMesh++); }
    World::MaterialId CreateMaterial(const MaterialRuntime&)  override { return static_cast<World::MaterialId>(m_NextMaterial++); }
    void              DestroyMesh    (World::MeshId)          override {}
    void              DestroyMaterial(World::MaterialId)      override {}

    void BeginFrame() override { m_FrameIndex++; m_LastDrawCount = 0; }
    void Submit(const Scene::Camera&, const Scene::RenderView& view) override
    {
        m_LastDrawCount += view.DrawCount();
    }
    void EndFrame() override {}

    // Diagnostic readout.
    [[nodiscard]] std::uint64_t FrameIndex()    const noexcept { return m_FrameIndex; }
    [[nodiscard]] std::size_t   LastDrawCount() const noexcept { return m_LastDrawCount; }

private:
    std::uint64_t m_FrameIndex     = 0;
    std::size_t   m_LastDrawCount  = 0;
    std::uint32_t m_NextMesh       = 1;
    std::uint32_t m_NextMaterial   = 1;
};

} // namespace SagaEngine::Render::Backend
