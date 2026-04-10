/// @file MathGLMBridge.cpp
/// @brief Out-of-line helpers for the MathGLMBridge header.
///
/// Only the Transform → glm::mat4 convenience lives here; everything else
/// in MathGLMBridge.h is inline.  Keeping this .cpp around gives the
/// Math module a single translation unit where GLM actually compiles,
/// which is the canonical place to run a static_assert sanity check that
/// the two sides agree on layout.

#include "SagaEngine/Math/MathGLMBridge.h"

#include <type_traits>

namespace SagaEngine::Math::GlmBridge
{

// ─── Layout sanity ───────────────────────────────────────────────────────────
//
// If these ever fail, a GLM upgrade has changed the struct layout out from
// under us and every ToGlm/FromGlm call becomes a subtle bug.  Better to
// break the build loudly than to ship a silent corruption.

static_assert(sizeof(glm::vec3) == sizeof(Vec3),
              "glm::vec3 / Vec3 size mismatch — GLM layout changed");

static_assert(sizeof(glm::quat) == sizeof(Quat),
              "glm::quat / Quat size mismatch — GLM layout changed");

static_assert(sizeof(glm::mat4) == sizeof(Mat4),
              "glm::mat4 / Mat4 size mismatch — GLM layout changed");

static_assert(std::is_standard_layout_v<Vec3>,
              "Vec3 must be standard-layout for GLM bridge to be zero-cost");
static_assert(std::is_standard_layout_v<Quat>,
              "Quat must be standard-layout for GLM bridge to be zero-cost");
static_assert(std::is_standard_layout_v<Mat4>,
              "Mat4 must be standard-layout for GLM bridge to be zero-cost");

// ─── Transform → glm::mat4 fast path ─────────────────────────────────────────

glm::mat4 ToGlm(const Transform& xf) noexcept
{
    // We could go through Mat4::FromTransform first but building the glm
    // matrix directly saves 16 float copies.  The body mirrors the scalar
    // reference in Mat4.cpp — any future tweak must update both.
    const Quat& q = xf.rotation;
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;

    glm::mat4 out{1.0f};

    // Rotation × scale injected into the upper-left 3×3 block.
    out[0][0] = (1.0f - 2.0f * (yy + zz)) * xf.scale.x;
    out[0][1] = (       2.0f * (xy + wz)) * xf.scale.x;
    out[0][2] = (       2.0f * (xz - wy)) * xf.scale.x;
    out[0][3] = 0.0f;

    out[1][0] = (       2.0f * (xy - wz)) * xf.scale.y;
    out[1][1] = (1.0f - 2.0f * (xx + zz)) * xf.scale.y;
    out[1][2] = (       2.0f * (yz + wx)) * xf.scale.y;
    out[1][3] = 0.0f;

    out[2][0] = (       2.0f * (xz + wy)) * xf.scale.z;
    out[2][1] = (       2.0f * (yz - wx)) * xf.scale.z;
    out[2][2] = (1.0f - 2.0f * (xx + yy)) * xf.scale.z;
    out[2][3] = 0.0f;

    // Translation.
    out[3][0] = xf.position.x;
    out[3][1] = xf.position.y;
    out[3][2] = xf.position.z;
    out[3][3] = 1.0f;

    return out;
}

} // namespace SagaEngine::Math::GlmBridge
