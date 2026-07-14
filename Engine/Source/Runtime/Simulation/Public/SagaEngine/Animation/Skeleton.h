/// @file Skeleton.h
/// @brief Joint hierarchy and rest-pose data for skeletal animation.
///
/// Layer  : SagaEngine / Animation
/// Purpose: Defines the static skeleton topology. A Skeleton is a tree
///          of joints (bones), each carrying a local-space rest transform
///          (TRS) and a parent index. The inverse bind matrix converts
///          from model space to bone-local space and is precomputed once
///          at import time.
///
/// Design rules:
///   - Pure data.  No GPU handles, no backend references.
///   - Joints are stored in a flat array sorted parent-before-child so
///     a single forward pass computes all world-space matrices.
///   - Math types come from the engine's own library (Vec3, Quat, Mat4).
///   - Maximum 128 bones per skeleton.  This matches the constant buffer
///     budget (128 * 64 bytes = 8 KB) and covers every humanoid rig.

#pragma once

#include "SagaEngine/Math/Mat4.h"
#include "SagaEngine/Math/Quat.h"
#include "SagaEngine/Math/Vec3.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Animation
{

// ─── Limits ──────────────────────────────────────────────────────────────────

inline constexpr std::uint8_t kMaxBones = 128;

/// Sentinel parent index meaning "this joint is a root".
inline constexpr std::uint8_t kNoParent = 0xFF;

// ─── Joint ───────────────────────────────────────────────────────────────────

/// One joint (bone) in the skeleton hierarchy.
struct Joint
{
    std::string   name;

    /// Index into the parent array.  kNoParent for root joints.
    std::uint8_t  parent = kNoParent;

    /// Rest-pose local transform (relative to parent).
    Math::Vec3    restPosition{ 0.0f, 0.0f, 0.0f };
    Math::Quat    restRotation = Math::Quat::Identity();
    Math::Vec3    restScale{ 1.0f, 1.0f, 1.0f };

    /// Precomputed inverse bind matrix.  Transforms a model-space
    /// vertex into this bone's local space so the skinning shader can
    /// apply the animated bone transform and get a correct result.
    /// Computed once at import: inverseBindMatrix = inverse(worldBindPose).
    Math::Mat4    inverseBindMatrix = Math::Mat4::Identity();
};

// ─── Skeleton ────────────────────────────────────────────────────────────────

/// Immutable skeleton definition.  Shared by all instances of the same
/// character model — only the pose (animated transforms) varies per
/// instance; the topology and inverse bind matrices are constant.
struct Skeleton
{
    std::string            name;
    std::vector<Joint>     joints;

    /// Number of active joints (== joints.size(), capped at kMaxBones).
    [[nodiscard]] std::uint8_t JointCount() const noexcept
    {
        return static_cast<std::uint8_t>(
            joints.size() > kMaxBones ? kMaxBones : joints.size());
    }

    /// Compute the world-space rest-pose matrices for all joints.
    /// Output array must have at least JointCount() entries.
    void ComputeRestPose(Math::Mat4* outWorldMatrices) const noexcept;
};

// ─── Pose buffer ─────────────────────────────────────────────────────────────

/// Per-joint local-space animated transform.  The animation runtime
/// writes these; the pose evaluator multiplies through the hierarchy
/// to produce the final bone matrices uploaded to the GPU.
struct LocalPose
{
    Math::Vec3 position{ 0.0f, 0.0f, 0.0f };
    Math::Quat rotation = Math::Quat::Identity();
    Math::Vec3 scale{ 1.0f, 1.0f, 1.0f };
};

/// Working buffer for one skeleton instance's current pose.
struct PoseBuffer
{
    std::vector<LocalPose> localPoses;   ///< Per-joint animated TRS.
    std::vector<Math::Mat4> skinMatrices; ///< Final bone matrices for GPU upload.

    void Resize(std::uint8_t jointCount)
    {
        localPoses.resize(jointCount);
        skinMatrices.resize(jointCount, Math::Mat4::Identity());
    }

    [[nodiscard]] std::uint8_t JointCount() const noexcept
    {
        return static_cast<std::uint8_t>(localPoses.size());
    }
};

} // namespace SagaEngine::Animation
