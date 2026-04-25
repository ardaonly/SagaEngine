/// @file IRHI.h
/// @brief Abstract Rendering Hardware Interface — one level below the renderer.
///
/// IRHI is the backend-agnostic GPU abstraction.  The concrete DiligentRHI
/// (or a future VulkanRHI / D3D12RHI) implements this interface.  The
/// Renderer and RenderGraph talk only through IRHI — never directly to
/// Diligent / Vulkan / D3D types.

#pragma once

#include "SagaEngine/RHI/Types/BufferTypes.h"
#include "SagaEngine/RHI/Types/TextureTypes.h"
#include "SagaEngine/RHI/Types/ShaderTypes.h"
#include "SagaEngine/RHI/Types/PipelineTypes.h"

#include <cstdint>

namespace SagaEngine::RHI
{

class IRHI
{
public:
    virtual ~IRHI() = default;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    [[nodiscard]] virtual bool Initialize()  = 0;
    virtual void               Shutdown()    = 0;

    // ── Resource creation ────────────────────────────────────────────────────

    [[nodiscard]] virtual BufferHandle   CreateBuffer(const BufferDesc& desc)             = 0;
    [[nodiscard]] virtual ShaderHandle   CreateShader(const ShaderDesc& desc)             = 0;
    [[nodiscard]] virtual PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;

    virtual void DestroyBuffer(BufferHandle h)     = 0;
    virtual void DestroyShader(ShaderHandle h)     = 0;
    virtual void DestroyPipeline(PipelineHandle h) = 0;

    // ── Buffer data ──────────────────────────────────────────────────────────

    /// Map a dynamic buffer for CPU write. Returns nullptr on failure.
    [[nodiscard]] virtual void* MapBuffer(BufferHandle h)    = 0;
    virtual void                UnmapBuffer(BufferHandle h)  = 0;

    // ── Frame markers ────────────────────────────────────────────────────────

    virtual void BeginFrame() = 0;
    virtual void EndFrame()   = 0;
};

} // namespace SagaEngine::RHI
