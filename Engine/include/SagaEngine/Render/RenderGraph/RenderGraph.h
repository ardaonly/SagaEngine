/// @file RenderGraph.h
/// @brief Front-end API for building and executing a render graph each frame.

#pragma once

#include "SagaEngine/Render/RenderGraph/RGTypes.h"
#include "SagaEngine/Render/RenderGraph/RGPass.h"
#include "SagaEngine/Render/RenderGraph/RGCompilation.h"

#include <functional>
#include <string>
#include <vector>

namespace SagaEngine::RHI { class IRHI; }

namespace SagaEngine::Render::RG
{

/// Per-frame render graph.  Users add passes + virtual resources, then
/// call Compile() + Execute().  Designed for rebuild every frame.
class RenderGraph
{
public:
    RenderGraph() = default;

    /// Reset for a new frame.
    void Clear();

    /// Register a virtual texture resource. Returns its handle.
    [[nodiscard]] RGResourceId AddTexture(const RGTextureDesc& desc);

    /// Register a virtual buffer resource. Returns its handle.
    [[nodiscard]] RGResourceId AddBuffer(const RGBufferDesc& desc);

    /// Add a pass. The execute callback will be invoked at the right time.
    void AddPass(const std::string& name,
                 std::vector<RGResourceUsage> inputs,
                 std::vector<RGResourceUsage> outputs,
                 std::function<void(RHI::IRHI&)> execute);

    /// Compile the graph into execution order.
    [[nodiscard]] bool Compile();

    /// Execute the compiled graph against the given RHI.
    void Execute(RHI::IRHI& rhi);

    [[nodiscard]] std::uint32_t PassCount()     const noexcept { return static_cast<std::uint32_t>(m_passes.size()); }
    [[nodiscard]] std::uint32_t ResourceCount() const noexcept { return m_nextResourceId; }

private:
    std::vector<RGPass>        m_passes;
    std::vector<RGTextureDesc> m_textures;
    std::vector<RGBufferDesc>  m_buffers;
    std::uint32_t              m_nextResourceId = 1;
    CompiledGraph              m_compiled{};
};

} // namespace SagaEngine::Render::RG
