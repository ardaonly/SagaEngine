/// @file RenderGraph.h
/// @brief Front-end API for building and executing a render graph each frame.

#pragma once

#include "SagaEngine/Render/RenderGraph/RGTypes.h"
#include "SagaEngine/Render/RenderGraph/RGPass.h"

#include <functional>
#include <string>
#include <vector>

namespace SagaEngine::RHI { class IRHI; }

namespace SagaEngine::Render::RG
{

enum class RGDiagnosticCode : std::uint8_t
{
    None = 0,
    EmptyPassName,
    InvalidResourceId,
    ReadBeforeWrite,
    DuplicateWriter,
    MissingOutputProducer,
    CycleDetected,
    EmptyResourceName,
    InvalidTextureDesc,
    InvalidBufferDesc,
    DuplicatePassName,
    DuplicatePassResourceUsage,
    PassReadsAndWritesSameResource,
    CompiledStateInvalidated,
};

struct RGDiagnostic
{
    RGDiagnosticCode code = RGDiagnosticCode::None;
    std::uint32_t passIndex = 0;
    std::string passName;
    RGResourceId resource = RGResourceId::kInvalid;
};

struct RGCompileSnapshot
{
    bool valid = false;
    std::uint32_t passCount = 0;
    std::uint32_t resourceCount = 0;
    std::uint32_t compiledPassCount = 0;
    std::vector<RGDiagnostic> diagnostics;
    std::string deterministicDump;
};

enum class RGExecutionSkipReason : std::uint8_t
{
    None = 0,
    GraphNotCompiled,
    InvalidCompile,
    CompiledStateInvalidated,
};

struct RGExecutionSnapshot
{
    bool attempted = false;
    bool executed = false;
    bool validCompile = false;
    RGExecutionSkipReason skipReason = RGExecutionSkipReason::GraphNotCompiled;
    std::uint32_t executedPassCount = 0;
    std::vector<std::string> passOrder;
};

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

    /// Add a dependency-free pass. This is a convenience for tests and
    /// graph-level utility passes; resource-producing passes should use the
    /// full overload with explicit inputs and outputs.
    void AddPass(const std::string& name,
                 std::function<void(RHI::IRHI&)> execute);

    /// Compile the graph into execution order.
    [[nodiscard]] bool Compile();

    /// Execute the compiled graph against the given RHI.
    void Execute(RHI::IRHI& rhi);

    [[nodiscard]] const RGCompileSnapshot& GetLastCompileSnapshot()
        const noexcept
    {
        return m_lastCompileSnapshot;
    }

    [[nodiscard]] const RGExecutionSnapshot& GetLastExecutionSnapshot()
        const noexcept
    {
        return m_lastExecutionSnapshot;
    }

    [[nodiscard]] std::string DumpLastCompile() const
    {
        return m_lastCompileSnapshot.deterministicDump;
    }

    [[nodiscard]] bool HasCompiledState() const noexcept
    {
        return m_compileAttempted && !m_compiledStateDirty;
    }

    [[nodiscard]] bool IsCompiledStateValid() const noexcept
    {
        return HasCompiledState() && m_compiledValid;
    }

    [[nodiscard]] bool IsCompiledStateDirty() const noexcept
    {
        return m_compiledStateDirty;
    }

    [[nodiscard]] std::uint32_t PassCount() const noexcept
    {
        return static_cast<std::uint32_t>(m_passes.size());
    }

    [[nodiscard]] std::uint32_t ResourceCount() const noexcept
    {
        return static_cast<std::uint32_t>(m_resourceRecords.size());
    }

private:
    enum class ResourceRecordKind : std::uint8_t
    {
        Texture = 0,
        Buffer,
    };

    struct ResourceRecord
    {
        RGResourceId id = RGResourceId::kInvalid;
        ResourceRecordKind kind = ResourceRecordKind::Texture;
        std::uint32_t descIndex = 0;
        std::string debugName;
    };

    [[nodiscard]] std::vector<RGDiagnostic> ValidateGraph() const;
    [[nodiscard]] bool ResourceExists(RGResourceId id) const noexcept;
    [[nodiscard]] std::string BuildDeterministicDump() const;
    void InvalidateCompiledStateForMutation();

    std::vector<RGPass>        m_passes;
    std::vector<RGPass*>       m_compiledPasses;
    std::vector<RGTextureDesc> m_textures;
    std::vector<RGBufferDesc>  m_buffers;
    std::vector<ResourceRecord> m_resourceRecords;
    std::uint32_t              m_nextResourceId = 1;
    bool                       m_compiledValid = false;
    bool                       m_compileAttempted = false;
    bool                       m_compiledStateDirty = false;
    RGCompileSnapshot          m_lastCompileSnapshot{};
    RGExecutionSnapshot        m_lastExecutionSnapshot{};
};

} // namespace SagaEngine::Render::RG
