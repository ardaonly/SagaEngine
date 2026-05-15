/// @file PoseEvaluator.cpp
/// @brief Animation sampling and skin matrix computation.

#include "SagaEngine/Animation/PoseEvaluator.h"

#include <cmath>

namespace SagaEngine::Animation
{

// ─── AnimationState ──────────────────────────────────────────────────────────

void AnimationState::Tick(float dt) noexcept
{
    if (!playing || !clip || clip->duration <= 0.0f) return;

    time += dt * speed;

    if (clip->looping)
    {
        time = std::fmod(time, clip->duration);
        if (time < 0.0f) time += clip->duration;
    }
    else
    {
        if (time >= clip->duration)
        {
            time    = clip->duration;
            playing = false;
        }
        else if (time < 0.0f)
        {
            time    = 0.0f;
            playing = false;
        }
    }
}

// ─── Pose evaluation ─────────────────────────────────────────────────────────

void EvaluatePose(const Skeleton&       skeleton,
                  const AnimationState& state,
                  PoseBuffer&           outBuffer) noexcept
{
    const auto jointCount = skeleton.JointCount();
    outBuffer.Resize(jointCount);

    // 1. Initialize local poses from rest pose.
    for (std::uint8_t i = 0; i < jointCount; ++i)
    {
        outBuffer.localPoses[i].position = skeleton.joints[i].restPosition;
        outBuffer.localPoses[i].rotation = skeleton.joints[i].restRotation;
        outBuffer.localPoses[i].scale    = skeleton.joints[i].restScale;
    }

    // 2. Sample animation channels and overwrite affected joints.
    if (state.clip)
    {
        const float t = state.time;

        for (const auto& ch : state.clip->channels)
        {
            if (ch.jointIndex >= jointCount) continue;

            auto& pose = outBuffer.localPoses[ch.jointIndex];

            switch (ch.target)
            {
            case ChannelTarget::Position:
                pose.position = SampleVec3(ch.vec3Keys, t);
                break;
            case ChannelTarget::Rotation:
                pose.rotation = SampleQuat(ch.quatKeys, t);
                break;
            case ChannelTarget::Scale:
                pose.scale = SampleVec3(ch.vec3Keys, t);
                break;
            }
        }
    }

    // 3. Forward-pass: accumulate local → world matrices.
    //    Joints are sorted parent-before-child, so a single pass suffices.
    Math::Mat4 worldMatrices[kMaxBones];

    for (std::uint8_t i = 0; i < jointCount; ++i)
    {
        const auto& lp = outBuffer.localPoses[i];

        Math::Mat4 local = Math::Mat4::FromTranslation(lp.position)
                         * Math::Mat4::FromQuat(lp.rotation)
                         * Math::Mat4::FromScale(lp.scale);

        const auto parent = skeleton.joints[i].parent;
        if (parent != kNoParent && parent < i)
            worldMatrices[i] = worldMatrices[parent] * local;
        else
            worldMatrices[i] = local;
    }

    // 4. Multiply by inverse bind matrix → final skin matrices.
    //    skinMatrix[i] = worldAnimated[i] * inverseBindPose[i]
    for (std::uint8_t i = 0; i < jointCount; ++i)
    {
        outBuffer.skinMatrices[i] = worldMatrices[i]
                                  * skeleton.joints[i].inverseBindMatrix;
    }
}

} // namespace SagaEngine::Animation
