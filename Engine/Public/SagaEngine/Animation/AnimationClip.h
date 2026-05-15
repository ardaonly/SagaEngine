/// @file AnimationClip.h
/// @brief Keyframe animation data — clips, channels, and keyframes.
///
/// Layer  : SagaEngine / Animation
/// Purpose: An AnimationClip is a baked sequence of keyframes for one or
///          more joints.  Each channel targets a single joint and a single
///          property (position, rotation, or scale).  The runtime samples
///          channels at a given time and writes the result into a PoseBuffer.
///
/// Design rules:
///   - Pure data.  No GPU state, no pointers into Skeleton.
///   - Channels reference joints by index (matching Skeleton::joints order).
///   - Keyframes are sorted by time.  The sampler binary-searches.
///   - Rotation keyframes store unit quaternions; position/scale store Vec3.
///   - Interpolation is always linear (lerp for position/scale, slerp for rotation).

#pragma once

#include "SagaEngine/Math/Quat.h"
#include "SagaEngine/Math/Vec3.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Animation
{

// ─── Keyframes ───────────────────────────────────────────────────────────────

struct KeyframeVec3
{
    float           time = 0.0f;   ///< Seconds from clip start.
    Math::Vec3      value{};
};

struct KeyframeQuat
{
    float           time = 0.0f;
    Math::Quat      value = Math::Quat::Identity();
};

// ─── Channel ─────────────────────────────────────────────────────────────────

/// What property a channel drives.
enum class ChannelTarget : std::uint8_t
{
    Position = 0,
    Rotation = 1,
    Scale    = 2,
};

/// One animation channel targeting one joint's one property.
struct AnimationChannel
{
    std::uint8_t   jointIndex = 0;
    ChannelTarget  target     = ChannelTarget::Position;

    /// Keyframe data.  Only the matching vector is populated based on target:
    /// Position/Scale → vec3Keys, Rotation → quatKeys.
    std::vector<KeyframeVec3> vec3Keys;
    std::vector<KeyframeQuat> quatKeys;
};

// ─── Clip ────────────────────────────────────────────────────────────────────

/// A single animation clip (e.g. "walk", "idle", "attack").
struct AnimationClip
{
    std::string                    name;
    float                          duration = 0.0f;   ///< Total length in seconds.
    bool                           looping  = true;
    std::vector<AnimationChannel>  channels;
};

// ─── Sampling helpers ────────────────────────────────────────────────────────

/// Sample a Vec3 channel at the given time.  Clamps at boundaries.
[[nodiscard]] Math::Vec3 SampleVec3(const std::vector<KeyframeVec3>& keys,
                                     float time) noexcept;

/// Sample a Quat channel at the given time.  Clamps at boundaries.
[[nodiscard]] Math::Quat SampleQuat(const std::vector<KeyframeQuat>& keys,
                                     float time) noexcept;

} // namespace SagaEngine::Animation
