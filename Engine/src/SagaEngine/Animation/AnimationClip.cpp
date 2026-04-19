/// @file AnimationClip.cpp
/// @brief Keyframe sampling (lerp for Vec3, slerp for Quat).

#include "SagaEngine/Animation/AnimationClip.h"

#include <algorithm>

namespace SagaEngine::Animation
{

// ─── Vec3 linear interpolation ───────────────────────────────────────────────

static Math::Vec3 LerpVec3(const Math::Vec3& a, const Math::Vec3& b, float t) noexcept
{
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
    };
}

Math::Vec3 SampleVec3(const std::vector<KeyframeVec3>& keys, float time) noexcept
{
    if (keys.empty()) return {};
    if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;

    // Binary search for the interval [i, i+1] containing `time`.
    auto it = std::upper_bound(keys.begin(), keys.end(), time,
        [](float t, const KeyframeVec3& kf) { return t < kf.time; });

    const auto idx = static_cast<std::size_t>(it - keys.begin());
    const auto& k0 = keys[idx - 1];
    const auto& k1 = keys[idx];

    const float span = k1.time - k0.time;
    const float t = (span > 0.0f) ? (time - k0.time) / span : 0.0f;

    return LerpVec3(k0.value, k1.value, t);
}

// ─── Quat slerp sampling ────────────────────────────────────────────────────

Math::Quat SampleQuat(const std::vector<KeyframeQuat>& keys, float time) noexcept
{
    if (keys.empty()) return Math::Quat::Identity();
    if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;

    auto it = std::upper_bound(keys.begin(), keys.end(), time,
        [](float t, const KeyframeQuat& kf) { return t < kf.time; });

    const auto idx = static_cast<std::size_t>(it - keys.begin());
    const auto& k0 = keys[idx - 1];
    const auto& k1 = keys[idx];

    const float span = k1.time - k0.time;
    const float t = (span > 0.0f) ? (time - k0.time) / span : 0.0f;

    return k0.value.Slerp(k1.value, t);
}

} // namespace SagaEngine::Animation
