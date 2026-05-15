/// @file Quat.cpp
/// @brief Quaternion non-inline method implementations.
///
/// All math is done in plain scalar arithmetic — no GLM dependency in this
/// translation unit.  The GLM conversion helpers live in a separate file
/// (MathGLMBridge.cpp) so engine code never pulls in GLM transitively.

#include "SagaEngine/Math/Quat.h"
#include "SagaEngine/Math/MathConstants.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace SagaEngine::Math
{

// ─── Named constructors ───────────────────────────────────────────────────────

Quat Quat::FromAxisAngle(const Vec3& axis, float angleRadians) noexcept
{
    const float half  = angleRadians * 0.5f;
    const float sinH  = std::sin(half);
    const float cosH  = std::cos(half);
    return { axis.x * sinH, axis.y * sinH, axis.z * sinH, cosH };
}

Quat Quat::FromEuler(float pitchRad, float yawRad, float rollRad) noexcept
{
    // Applied order: Yaw (Y) → Pitch (X) → Roll (Z)
    const float cy = std::cos(yawRad   * 0.5f);
    const float sy = std::sin(yawRad   * 0.5f);
    const float cp = std::cos(pitchRad * 0.5f);
    const float sp = std::sin(pitchRad * 0.5f);
    const float cr = std::cos(rollRad  * 0.5f);
    const float sr = std::sin(rollRad  * 0.5f);

    return {
        cy * sp * cr + sy * cp * sr,
        sy * cp * cr - cy * sp * sr,
        cy * cp * sr - sy * sp * cr,
        cy * cp * cr + sy * sp * sr
    };
}

// ─── Normalisation ────────────────────────────────────────────────────────────

Quat Quat::Normalized() const noexcept
{
    const float n = Norm();
    if (n * n < kEpsilonNormSq)
        return Identity();
    return { x / n, y / n, z / n, w / n };
}

// ─── Inverse ──────────────────────────────────────────────────────────────────

Quat Quat::Inverse() const noexcept
{
    const float nsq = NormSq();
    if (nsq < kEpsilonNormSq)
        return Identity();
    const Quat conj = Conjugate();
    return { conj.x / nsq, conj.y / nsq, conj.z / nsq, conj.w / nsq };
}

// ─── Rotation application ─────────────────────────────────────────────────────

Vec3 Quat::Rotate(const Vec3& v) const noexcept
{
    // Rodrigues formula via two cross products — branchless, no temporary Quat.
    // t = 2 * cross(q.xyz, v)
    // result = v + q.w * t + cross(q.xyz, t)
    const float tx = 2.0f * (y * v.z - z * v.y);
    const float ty = 2.0f * (z * v.x - x * v.z);
    const float tz = 2.0f * (x * v.y - y * v.x);

    return {
        v.x + w * tx + (y * tz - z * ty),
        v.y + w * ty + (z * tx - x * tz),
        v.z + w * tz + (x * ty - y * tx)
    };
}

// ─── Decomposition ────────────────────────────────────────────────────────────

Vec3 Quat::ToEulerAngles() const noexcept
{
    // Returns pitch (X), yaw (Y), roll (Z) in radians.

    // Pitch (X-axis rotation)
    const float sinrCosp =  2.0f * (w * x + y * z);
    const float cosrCosp =  1.0f - 2.0f * (x * x + y * y);
    const float pitch     = std::atan2(sinrCosp, cosrCosp);

    // Yaw (Y-axis rotation)
    const float sinp = 2.0f * (w * y - z * x);
    float yaw;
    if (std::fabs(sinp) >= 1.0f)
        yaw = std::copysign(kHalfPi, sinp); // clamp to ±90°
    else
        yaw = std::asin(sinp);

    // Roll (Z-axis rotation)
    const float sinyCosp =  2.0f * (w * z + x * y);
    const float cosyCosp =  1.0f - 2.0f * (y * y + z * z);
    const float roll      = std::atan2(sinyCosp, cosyCosp);

    return { pitch, yaw, roll };
}

void Quat::ToAxisAngle(Vec3& outAxis, float& outAngleRad) const noexcept
{
    // Clamp w to avoid NaN in acos due to floating-point drift past ±1.
    const float wClamped = std::max(-1.0f, std::min(1.0f, w));
    outAngleRad = 2.0f * std::acos(wClamped);

    const float sinHalf = std::sqrt(1.0f - wClamped * wClamped);
    if (sinHalf < 1e-6f)
    {
        // Angle near zero — axis direction undefined, return canonical up.
        outAxis = Vec3::Up();
    }
    else
    {
        outAxis = { x / sinHalf, y / sinHalf, z / sinHalf };
    }
}

// ─── Interpolation ────────────────────────────────────────────────────────────

Quat Quat::Nlerp(const Quat& target, float t) const noexcept
{
    // Ensure shortest-arc by flipping target if dot < 0.
    const float d  = Dot(target);
    const Quat  t2 = (d < 0.0f) ? Quat{-target.x, -target.y, -target.z, -target.w}
                                 : target;

    const float invT = 1.0f - t;
    return Quat{
        invT * x + t * t2.x,
        invT * y + t * t2.y,
        invT * z + t * t2.z,
        invT * w + t * t2.w
    }.Normalized();
}

Quat Quat::Slerp(const Quat& target, float t) const noexcept
{
    float d = Dot(target);

    // Ensure shortest path.
    Quat end = target;
    if (d < 0.0f)
    {
        end = {-target.x, -target.y, -target.z, -target.w};
        d   = -d;
    }

    // Fall back to Nlerp when quaternions are very close to avoid sin(0) division.
    if (d > kSlerpLinearThreshold)
        return Nlerp(end, t);

    const float theta0   = std::acos(d);
    const float theta    = theta0 * t;
    const float sinTheta = std::sin(theta);
    const float sinTheta0 = std::sin(theta0);

    const float s0 = std::cos(theta) - d * sinTheta / sinTheta0;
    const float s1 = sinTheta / sinTheta0;

    return Quat{
        s0 * x + s1 * end.x,
        s0 * y + s1 * end.y,
        s0 * z + s1 * end.z,
        s0 * w + s1 * end.w
    };
}

// ─── Diagnostics ──────────────────────────────────────────────────────────────

std::string Quat::ToString() const
{
    char buf[96];
    std::snprintf(buf, sizeof(buf),
                  "Quat(x=%.4f, y=%.4f, z=%.4f, w=%.4f)", x, y, z, w);
    return buf;
}

} // namespace SagaEngine::Math
