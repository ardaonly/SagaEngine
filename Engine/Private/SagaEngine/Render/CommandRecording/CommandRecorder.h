/// @file CommandRecorder.h
/// @brief Utility that builds a CommandBuffer from high-level draw requests.

#pragma once

#include "SagaEngine/Render/CommandRecording/CommandBuffer.h"

namespace SagaEngine::Render::CommandRecording
{

/// Builds a CommandBuffer by translating high-level draw requests into
/// low-level GPU commands.  Extensible — subclass or add helpers as
/// the render pipeline grows.
class CommandRecorder
{
public:
    CommandRecorder() = default;

    /// Begin recording into a fresh buffer.
    void Begin(const std::string& debugName = {})
    {
        m_buffer.Clear();
        m_buffer.SetDebugName(debugName);
    }

    void SetPipeline(RHI::PipelineHandle pso)             { m_buffer.Push(CmdSetPipeline{pso}); }
    void SetVertexBuffer(RHI::BufferHandle vb, std::uint32_t slot = 0)
                                                            { m_buffer.Push(CmdSetVertexBuffer{vb, slot}); }
    void SetIndexBuffer(RHI::BufferHandle ib)              { m_buffer.Push(CmdSetIndexBuffer{ib}); }
    void Draw(std::uint32_t verts, std::uint32_t first = 0){ m_buffer.Push(CmdDraw{verts, first}); }
    void DrawIndexed(std::uint32_t indices, std::uint32_t firstIdx = 0, std::int32_t vtxOff = 0)
                                                            { m_buffer.Push(CmdDrawIndexed{indices, firstIdx, vtxOff}); }
    void Dispatch(std::uint32_t x, std::uint32_t y = 1, std::uint32_t z = 1)
                                                            { m_buffer.Push(CmdDispatch{x, y, z}); }

    /// Finish recording and return the buffer (moved out).
    [[nodiscard]] CommandBuffer Finish() { return std::move(m_buffer); }

private:
    CommandBuffer m_buffer;
};

} // namespace SagaEngine::Render::CommandRecording
