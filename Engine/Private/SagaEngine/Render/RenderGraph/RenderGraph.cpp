/// @file RenderGraph.cpp
/// @brief Per-frame render graph — add resources & passes, compile, execute.

#include "SagaEngine/Render/RenderGraph/RGCompilation.h"
#include "SagaEngine/Render/RenderGraph/RenderGraph.h"
#include "SagaEngine/RHI/IRHI.h"

#include <sstream>
#include <utility>

namespace SagaEngine::Render::RG
{

void RenderGraph::Clear()
{
    m_passes.clear();
    m_textures.clear();
    m_buffers.clear();
    m_resourceRecords.clear();
    m_nextResourceId = 1;
    m_compiledPasses.clear();
    m_compiledValid = false;
    m_lastCompileSnapshot = {};
}

RGResourceId RenderGraph::AddTexture(const RGTextureDesc& desc)
{
    auto id = static_cast<RGResourceId>(m_nextResourceId++);
    m_textures.push_back(desc);
    m_resourceRecords.push_back({
        id,
        ResourceRecordKind::Texture,
        desc.debugName,
    });
    return id;
}

RGResourceId RenderGraph::AddBuffer(const RGBufferDesc& desc)
{
    auto id = static_cast<RGResourceId>(m_nextResourceId++);
    m_buffers.push_back(desc);
    m_resourceRecords.push_back({
        id,
        ResourceRecordKind::Buffer,
        desc.debugName,
    });
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
    m_lastCompileSnapshot = {};
    m_lastCompileSnapshot.diagnostics = ValidateGraph();
    if (!m_lastCompileSnapshot.diagnostics.empty())
    {
        m_compiledPasses.clear();
        m_compiledValid = false;
        m_lastCompileSnapshot.valid = false;
        m_lastCompileSnapshot.deterministicDump = BuildDeterministicDump();
        return false;
    }

    RGCompiler compiler;
    const auto compiled = compiler.Compile(m_passes);
    m_compiledPasses = compiled.sortedPasses;
    m_compiledValid = compiled.valid;
    m_lastCompileSnapshot.valid = m_compiledValid;
    if (!m_compiledValid)
    {
        m_lastCompileSnapshot.diagnostics.push_back({
            RGDiagnosticCode::CycleDetected,
            0u,
            {},
            RGResourceId::kInvalid,
        });
    }
    m_lastCompileSnapshot.deterministicDump = BuildDeterministicDump();
    return m_compiledValid;
}

void RenderGraph::Execute(RHI::IRHI& rhi)
{
    if (!m_compiledValid) return;

    for (auto* pass : m_compiledPasses)
    {
        if (pass->execute)
            pass->execute(rhi);
    }
}

std::vector<RGDiagnostic> RenderGraph::ValidateGraph() const
{
    std::vector<RGDiagnostic> diagnostics;
    std::vector<std::pair<RGResourceId, std::uint32_t>> writers;

    for (std::uint32_t passIndex = 0;
         passIndex < static_cast<std::uint32_t>(m_passes.size());
         ++passIndex)
    {
        const auto& pass = m_passes[passIndex];
        if (pass.debugName.empty())
        {
            diagnostics.push_back({
                RGDiagnosticCode::EmptyPassName,
                passIndex,
                pass.debugName,
                RGResourceId::kInvalid,
            });
        }

        for (const auto& output : pass.outputs)
        {
            if (!ResourceExists(output.resource))
            {
                diagnostics.push_back({
                    RGDiagnosticCode::InvalidResourceId,
                    passIndex,
                    pass.debugName,
                    output.resource,
                });
                continue;
            }

            bool duplicateWriter = false;
            for (const auto& writer : writers)
            {
                if (writer.first == output.resource)
                {
                    duplicateWriter = true;
                    break;
                }
            }

            if (duplicateWriter)
            {
                diagnostics.push_back({
                    RGDiagnosticCode::DuplicateWriter,
                    passIndex,
                    pass.debugName,
                    output.resource,
                });
            }
            else
            {
                writers.push_back({output.resource, passIndex});
            }
        }
    }

    for (std::uint32_t passIndex = 0;
         passIndex < static_cast<std::uint32_t>(m_passes.size());
         ++passIndex)
    {
        const auto& pass = m_passes[passIndex];
        for (const auto& input : pass.inputs)
        {
            if (!ResourceExists(input.resource))
            {
                diagnostics.push_back({
                    RGDiagnosticCode::InvalidResourceId,
                    passIndex,
                    pass.debugName,
                    input.resource,
                });
                continue;
            }

            bool hasWriter = false;
            bool hasPriorWriter = false;
            for (const auto& writer : writers)
            {
                if (writer.first != input.resource)
                {
                    continue;
                }

                hasWriter = true;
                if (writer.second < passIndex)
                {
                    hasPriorWriter = true;
                }
            }

            if (!hasWriter)
            {
                diagnostics.push_back({
                    RGDiagnosticCode::MissingOutputProducer,
                    passIndex,
                    pass.debugName,
                    input.resource,
                });
            }
            else if (!hasPriorWriter)
            {
                diagnostics.push_back({
                    RGDiagnosticCode::ReadBeforeWrite,
                    passIndex,
                    pass.debugName,
                    input.resource,
                });
            }
        }
    }

    return diagnostics;
}

bool RenderGraph::ResourceExists(RGResourceId id) const noexcept
{
    const auto value = static_cast<std::uint32_t>(id);
    return value != 0u && value < m_nextResourceId;
}

std::string RenderGraph::BuildDeterministicDump() const
{
    std::ostringstream out;
    out << "RenderGraphDump v0\n";
    out << "valid: " << (m_compiledValid ? "true" : "false") << "\n";
    out << "resources:\n";
    for (const auto& resource : m_resourceRecords)
    {
        out << "  " << static_cast<std::uint32_t>(resource.id) << " ";
        out << (resource.kind == ResourceRecordKind::Texture ? "texture"
                                                             : "buffer");
        out << " \"" << resource.debugName << "\"\n";
    }

    out << "passes:\n";
    for (std::uint32_t i = 0;
         i < static_cast<std::uint32_t>(m_passes.size());
         ++i)
    {
        const auto& pass = m_passes[i];
        out << "  [" << i << "] \"" << pass.debugName << "\" inputs=[";
        for (std::size_t inputIndex = 0; inputIndex < pass.inputs.size();
             ++inputIndex)
        {
            if (inputIndex != 0u)
            {
                out << ",";
            }
            out << static_cast<std::uint32_t>(
                pass.inputs[inputIndex].resource);
        }
        out << "] outputs=[";
        for (std::size_t outputIndex = 0; outputIndex < pass.outputs.size();
             ++outputIndex)
        {
            if (outputIndex != 0u)
            {
                out << ",";
            }
            out << static_cast<std::uint32_t>(
                pass.outputs[outputIndex].resource);
        }
        out << "]\n";
    }

    out << "compiled:\n";
    for (std::uint32_t i = 0;
         i < static_cast<std::uint32_t>(m_compiledPasses.size());
         ++i)
    {
        out << "  [" << i << "] \"" << m_compiledPasses[i]->debugName
            << "\"\n";
    }

    out << "diagnostics:\n";
    for (const auto& diagnostic : m_lastCompileSnapshot.diagnostics)
    {
        out << "  code=" << static_cast<std::uint32_t>(diagnostic.code)
            << " pass=" << diagnostic.passIndex << " \""
            << diagnostic.passName << "\" resource="
            << static_cast<std::uint32_t>(diagnostic.resource) << "\n";
    }

    return out.str();
}

} // namespace SagaEngine::Render::RG
