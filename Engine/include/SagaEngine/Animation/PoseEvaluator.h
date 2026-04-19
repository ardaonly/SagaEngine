/// @file PoseEvaluator.h
/// @brief Samples an animation clip and computes the final bone matrices.
///
/// Layer  : SagaEngine / Animation
/// Purpose: The PoseEvaluator is the bridge between animation data and
///          the GPU skinning pipeline.  Given a Skeleton, a Clip, and a
///          playback time, it:
///            1. Samples each channel to produce per-joint local TRS.
///            2. Walks the joint hierarchy to accumulate world matrices.
///            3. Multiplies by inverse bind matrices to produce the final
///               skin matrix palette ready for GPU upload.

#pragma once

#include "SagaEngine/Animation/AnimationClip.h"
#include "SagaEngine/Animation/Skeleton.h"

namespace SagaEngine::Animation
{

/// Animation playback state for one skeleton instance.
struct AnimationState
{
    const AnimationClip* clip     = nullptr;
    float                time     = 0.0f;   ///< Current playback position (seconds).
    float                speed    = 1.0f;   ///< Playback speed multiplier.
    bool                 playing  = false;

    /// Advance playback by dt seconds.  Handles looping.
    void Tick(float dt) noexcept;
};

/// Sample the clip at the current time, compute hierarchy, produce
/// the final skin matrices (world * inverseBind) in outBuffer.skinMatrices.
/// Call after AnimationState::Tick().
void EvaluatePose(const Skeleton&      skeleton,
                  const AnimationState& state,
                  PoseBuffer&           outBuffer) noexcept;

} // namespace SagaEngine::Animation
