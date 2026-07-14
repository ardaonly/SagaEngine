/// @file FrameContext.h
/// @brief Per-frame invariants every render camera sees during one ExecuteFrame.
///
/// Layer  : SagaEngine / Render / Scene
/// Purpose: The render subsystem hands each camera the same FrameContext
///          so per-camera cullers and (later) per-camera effects agree on
///          time, frame index, and delta. Anything global that the backend
///          might need goes here rather than being threaded as separate
///          arguments.
///
/// Design rules:
///   - Pure data. No pointers into game state, no callbacks.
///   - Frame index is authoritative — backends may key ring buffers,
///     per-frame allocators, and GPU query pools on this value.

#pragma once

#include <cstdint>

namespace SagaEngine::Render::Scene
{

// ─── FrameContext ─────────────────────────────────────────────────────

/// Immutable per-frame context. Constructed once per ExecuteFrame() and
/// passed by const-reference to every sub-stage.
struct FrameContext
{
    std::uint64_t frameIndex      = 0;     ///< Monotonically increasing.
    float         deltaTimeSec    = 0.0f;  ///< Interval since last ExecuteFrame.
    double        absoluteTimeSec = 0.0;   ///< Application-defined "wall" time.
};

} // namespace SagaEngine::Render::Scene
