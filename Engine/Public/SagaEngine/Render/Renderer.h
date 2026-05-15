/// @file Renderer.h
/// @brief High-level renderer that owns the frame graph, passes, and RHI.

#pragma once

#include "SagaEngine/Render/RenderCore.h"
#include "SagaEngine/Render/FrameGraphExecutor.h"
#include "SagaEngine/Render/Scene/RenderView.h"

#include <memory>

namespace SagaEngine::RHI { class IRHI; }

namespace SagaEngine::Render
{

/// Top-level renderer.  Holds a reference to the active IRHI backend and
/// orchestrates frame-graph compilation + execution each frame.
///
/// Currently a thin shell — the real drawing is done by
/// DiligentRenderBackend::Submit().  As we migrate to a pass-based
/// architecture this class will compile pass lists and hand them to
/// FrameGraphExecutor.
class Renderer
{
public:
    Renderer() = default;
    ~Renderer();

    /// Attach an RHI backend (non-owning).
    void SetRHI(RHI::IRHI* rhi) { m_rhi = rhi; }
    [[nodiscard]] RHI::IRHI* GetRHI() const noexcept { return m_rhi; }

    void SetConfig(const RenderConfig& cfg) { m_config = cfg; }
    [[nodiscard]] const RenderConfig& GetConfig() const noexcept { return m_config; }

    /// Called once after the RHI is ready.
    [[nodiscard]] bool Initialize();

    /// Build & execute the frame graph for one frame.
    void RenderFrame(const Scene::RenderView& view);

    void Shutdown();

private:
    RHI::IRHI*         m_rhi = nullptr;
    RenderConfig       m_config{};
    FrameGraphExecutor m_executor;
    bool               m_initialized = false;
};

} // namespace SagaEngine::Render
