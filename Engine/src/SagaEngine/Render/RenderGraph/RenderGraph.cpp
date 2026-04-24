/// @file RenderGraph.cpp
/// @brief Per-frame render graph — add resources & passes, compile, execute.

#include "SagaEngine/Render/RenderGraph/RenderGraph.h"
#include "SagaEngine/RHI/IRHI.h"

namespace SagaEngine::Render::RG
{

void RenderGraph::Clear()
{
    m_passes.clear();
    m_textures.clear();
    m_buffers.clear();
    m_nextResourceId = 1;
    m_compiled = {};
}

RGResourceId RenderGraph::AddTexture(const RGTextureDesc& desc)
{
    auto id = static_cast<RGResourceId>(m_nextResourceId++);
    m_textures.push_back(desc);
    return id;
}

RGResourceId RenderGraph::AddBuffer(const RGBufferDesc& desc)
{
    auto id = static_cast<RGResourceId>(m_nextResourceId++);
    m_buffers.push_back(desc);
    return id;
}

void RenderGraph::AddPass(const std::string& name,
                          std::vector<RGResourceUsage> inputs,
                          std::vector<RGResourceUsage> outputs,
                          std::function<void(RHI::IRHI&)> execute)
{
    RGPass pass;
    pass.debugName = name;
    pass.inputs    = std::move(inputs);
    pass.outputs   = std::move(outputs);
    pass.execute   = std::move(execute);
    m_passes.push_back(std::move(pass));
}

bool RenderGraph::Compile()
{
    RGCompiler compiler;
    m_compiled = compiler.Compile(m_passes);
    return m_compiled.valid;
}

void RenderGraph::Execute(RHI::IRHI& rhi)
{
    if (!m_compiled.valid) return;

    for (auto* pass : m_compiled.sortedPasses)
    {
        if (pass->execute)
            pass->execute(rhi);
    }
}

} // namespace SagaEngine::Render::RG
