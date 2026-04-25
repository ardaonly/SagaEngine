/// @file CommandBuffer.h
/// @brief Recorded GPU command buffer — a list of draw / dispatch / barrier ops.

#pragma once

#include "SagaEngine/Render/Scene/RenderView.h"
#include "SagaEngine/RHI/Types/PipelineTypes.h"
#include "SagaEngine/RHI/Types/BufferTypes.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace SagaEngine::Render::CommandRecording
{

// ── Command variants ─────────────────────────────────────────────────

struct CmdSetPipeline  { RHI::PipelineHandle pipeline = RHI::PipelineHandle::kInvalid; };
struct CmdSetVertexBuffer { RHI::BufferHandle buffer = RHI::BufferHandle::kInvalid; std::uint32_t slot = 0; };
struct CmdSetIndexBuffer  { RHI::BufferHandle buffer = RHI::BufferHandle::kInvalid; };
struct CmdDraw         { std::uint32_t vertexCount = 0; std::uint32_t firstVertex = 0; };
struct CmdDrawIndexed  { std::uint32_t indexCount = 0;  std::uint32_t firstIndex = 0; std::int32_t vertexOffset = 0; };
struct CmdDispatch     { std::uint32_t groupsX = 1; std::uint32_t groupsY = 1; std::uint32_t groupsZ = 1; };

using RenderCommand = std::variant<
    CmdSetPipeline,
    CmdSetVertexBuffer,
    CmdSetIndexBuffer,
    CmdDraw,
    CmdDrawIndexed,
    CmdDispatch
>;

// ── CommandBuffer ────────────────────────────────────────────────────

/// A recorded list of GPU commands. Built by CommandRecorder, executed by
/// the backend or FrameGraphExecutor.
class CommandBuffer
{
public:
    void Clear() { m_commands.clear(); m_debugName.clear(); }

    void SetDebugName(const std::string& name) { m_debugName = name; }
    [[nodiscard]] const std::string& GetDebugName() const noexcept { return m_debugName; }

    void Push(const RenderCommand& cmd) { m_commands.push_back(cmd); }

    [[nodiscard]] const std::vector<RenderCommand>& GetCommands() const noexcept { return m_commands; }
    [[nodiscard]] std::size_t Size() const noexcept { return m_commands.size(); }
    [[nodiscard]] bool Empty() const noexcept { return m_commands.empty(); }

private:
    std::vector<RenderCommand> m_commands;
    std::string                m_debugName;
};

} // namespace SagaEngine::Render::CommandRecording
