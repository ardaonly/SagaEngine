/// @file RenderGraph.cpp
/// @brief Per-frame render graph — add resources & passes, compile, execute.

#include "SagaEngine/Render/RenderGraph/RGCompilation.h"
#include "SagaEngine/Render/RenderGraph/RenderGraph.h"
#include "SagaEngine/RHI/IRHI.h"

#include <sstream>
#include <utility>
#include <vector>

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
    m_compileAttempted = false;
    m_compiledStateDirty = false;
    m_lastCompileSnapshot = {};
    m_lastExecutionSnapshot = {};
}

RGResourceId RenderGraph::AddTexture(const RGTextureDesc& desc)
{
    InvalidateCompiledStateForMutation();

    auto id = static_cast<RGResourceId>(m_nextResourceId++);
    const auto descIndex = static_cast<std::uint32_t>(m_textures.size());
    m_textures.push_back(desc);
    m_resourceRecords.push_back({
        id,
        ResourceRecordKind::Texture,
        descIndex,
        desc.debugName,
    });
    return id;
}

RGResourceId RenderGraph::AddBuffer(const RGBufferDesc& desc)
{
    InvalidateCompiledStateForMutation();

    auto id = static_cast<RGResourceId>(m_nextResourceId++);
    const auto descIndex = static_cast<std::uint32_t>(m_buffers.size());
    m_buffers.push_back(desc);
    m_resourceRecords.push_back({
        id,
        ResourceRecordKind::Buffer,
        descIndex,
        desc.debugName,
    });
    return id;
}

void RenderGraph::AddPass(const std::string& name,
                          std::vector<RGResourceUsage> inputs,
                          std::vector<RGResourceUsage> outputs,
                          std::function<void(RHI::IRHI&)> execute)
{
    InvalidateCompiledStateForMutation();

    RGPass pass;
    pass.debugName = name;
    pass.inputs    = std::move(inputs);
    pass.outputs   = std::move(outputs);
    pass.execute   = std::move(execute);
    m_passes.push_back(std::move(pass));
}

void RenderGraph::AddPass(const std::string& name,
                          std::function<void(RHI::IRHI&)> execute)
{
    AddPass(name, {}, {}, std::move(execute));
}

bool RenderGraph::Compile()
{
    m_compileAttempted = true;
    m_compiledStateDirty = false;
    m_lastCompileSnapshot = {};
    m_lastCompileSnapshot.passCount =
        static_cast<std::uint32_t>(m_passes.size());
    m_lastCompileSnapshot.resourceCount =
        static_cast<std::uint32_t>(m_resourceRecords.size());
    m_lastCompileSnapshot.diagnostics = ValidateGraph();
    if (!m_lastCompileSnapshot.diagnostics.empty())
    {
        m_compiledPasses.clear();
        m_compiledValid = false;
        m_lastCompileSnapshot.valid = false;
        m_lastCompileSnapshot.compiledPassCount = 0;
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
    m_lastCompileSnapshot.compiledPassCount =
        static_cast<std::uint32_t>(m_compiledPasses.size());
    m_lastCompileSnapshot.deterministicDump = BuildDeterministicDump();
    return m_compiledValid;
}

void RenderGraph::Execute(RHI::IRHI& rhi)
{
    m_lastExecutionSnapshot = {};
    m_lastExecutionSnapshot.attempted = true;
    m_lastExecutionSnapshot.validCompile =
        m_compileAttempted && !m_compiledStateDirty && m_compiledValid;

    if (!m_compileAttempted)
    {
        m_lastExecutionSnapshot.skipReason =
            RGExecutionSkipReason::GraphNotCompiled;
        return;
    }

    if (m_compiledStateDirty)
    {
        m_lastExecutionSnapshot.skipReason =
            RGExecutionSkipReason::CompiledStateInvalidated;
        return;
    }

    if (!m_compiledValid)
    {
        m_lastExecutionSnapshot.skipReason =
            RGExecutionSkipReason::InvalidCompile;
        return;
    }

    m_lastExecutionSnapshot.skipReason = RGExecutionSkipReason::None;

    for (auto* pass : m_compiledPasses)
    {
        if (pass->execute)
        {
            pass->execute(rhi);
        }
        m_lastExecutionSnapshot.passOrder.push_back(pass->debugName);
        ++m_lastExecutionSnapshot.executedPassCount;
    }

    m_lastExecutionSnapshot.executed = true;
}

std::vector<RGDiagnostic> RenderGraph::ValidateGraph() const
{
    std::vector<RGDiagnostic> diagnostics;
    std::vector<std::pair<RGResourceId, std::uint32_t>> writers;

    for (const auto& resource : m_resourceRecords)
    {
        if (resource.debugName.empty())
        {
            diagnostics.push_back({
                RGDiagnosticCode::EmptyResourceName,
                0u,
                {},
                resource.id,
            });
        }

        if (resource.kind == ResourceRecordKind::Texture)
        {
            const auto& desc = m_textures[resource.descIndex];
            if (desc.width == 0u || desc.height == 0u)
            {
                diagnostics.push_back({
                    RGDiagnosticCode::InvalidTextureDesc,
                    0u,
                    {},
                    resource.id,
                });
            }
        }
        else
        {
            const auto& desc = m_buffers[resource.descIndex];
            if (desc.sizeBytes == 0u)
            {
                diagnostics.push_back({
                    RGDiagnosticCode::InvalidBufferDesc,
                    0u,
                    {},
                    resource.id,
                });
            }
        }
    }

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

        if (!pass.debugName.empty())
        {
            for (std::uint32_t previousPassIndex = 0;
                 previousPassIndex < passIndex;
                 ++previousPassIndex)
            {
                if (m_passes[previousPassIndex].debugName == pass.debugName)
                {
                    diagnostics.push_back({
                        RGDiagnosticCode::DuplicatePassName,
                        passIndex,
                        pass.debugName,
                        RGResourceId::kInvalid,
                    });
                    break;
                }
            }
        }

        for (std::size_t inputIndex = 0; inputIndex < pass.inputs.size();
             ++inputIndex)
        {
            for (std::size_t compareIndex = inputIndex + 1u;
                 compareIndex < pass.inputs.size();
                 ++compareIndex)
            {
                if (pass.inputs[inputIndex].resource ==
                    pass.inputs[compareIndex].resource)
                {
                    diagnostics.push_back({
                        RGDiagnosticCode::DuplicatePassResourceUsage,
                        passIndex,
                        pass.debugName,
                        pass.inputs[inputIndex].resource,
                    });
                    break;
                }
            }
        }

        for (std::size_t outputIndex = 0; outputIndex < pass.outputs.size();
             ++outputIndex)
        {
            for (std::size_t compareIndex = outputIndex + 1u;
                 compareIndex < pass.outputs.size();
                 ++compareIndex)
            {
                if (pass.outputs[outputIndex].resource ==
                    pass.outputs[compareIndex].resource)
                {
                    diagnostics.push_back({
                        RGDiagnosticCode::DuplicatePassResourceUsage,
                        passIndex,
                        pass.debugName,
                        pass.outputs[outputIndex].resource,
                    });
                    break;
                }
            }
        }

        bool readsAndWritesSameResource = false;
        for (const auto& input : pass.inputs)
        {
            for (const auto& output : pass.outputs)
            {
                if (input.resource == output.resource)
                {
                    diagnostics.push_back({
                        RGDiagnosticCode::PassReadsAndWritesSameResource,
                        passIndex,
                        pass.debugName,
                        input.resource,
                    });
                    readsAndWritesSameResource = true;
                    break;
                }
            }

            if (readsAndWritesSameResource)
            {
                break;
            }
        }
    }

    for (std::uint32_t passIndex = 0;
         passIndex < static_cast<std::uint32_t>(m_passes.size());
         ++passIndex)
    {
        const auto& pass = m_passes[passIndex];
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
                if (writer.first == output.resource &&
                    writer.second != passIndex)
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

void RenderGraph::InvalidateCompiledStateForMutation()
{
    if (!m_compileAttempted)
    {
        return;
    }

    m_compiledPasses.clear();
    m_compiledValid = false;
    m_compiledStateDirty = true;
}

std::string RenderGraph::BuildDeterministicDump() const
{
    std::ostringstream out;
    out << "RenderGraphDump v1\n";
    out << "valid: " << (m_compiledValid ? "true" : "false") << "\n";
    out << "dirty: " << (m_compiledStateDirty ? "true" : "false") << "\n";
    out << "passCount: " << m_lastCompileSnapshot.passCount << "\n";
    out << "resourceCount: " << m_lastCompileSnapshot.resourceCount << "\n";
    out << "compiledPassCount: "
        << m_lastCompileSnapshot.compiledPassCount << "\n";
    out << "resources:\n";
    for (const auto& resource : m_resourceRecords)
    {
        out << "  " << static_cast<std::uint32_t>(resource.id) << " ";
        out << (resource.kind == ResourceRecordKind::Texture ? "texture"
                                                             : "buffer");
        out << " \"" << resource.debugName << "\"";
        if (resource.kind == ResourceRecordKind::Texture)
        {
            const auto& desc = m_textures[resource.descIndex];
            out << " width=" << desc.width;
            out << " height=" << desc.height;
            out << " format=" << static_cast<std::uint32_t>(desc.format);
            out << " lifetime=" << static_cast<std::uint32_t>(desc.lifetime);
        }
        else
        {
            const auto& desc = m_buffers[resource.descIndex];
            out << " sizeBytes=" << desc.sizeBytes;
            out << " usage=" << static_cast<std::uint32_t>(desc.usage);
            out << " lifetime=" << static_cast<std::uint32_t>(desc.lifetime);
        }
        out << "\n";
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
