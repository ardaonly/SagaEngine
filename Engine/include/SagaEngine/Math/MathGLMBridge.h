/// @file MathGLMBridge.h
/// @brief Opt-in conversion helpers between SagaEngine::Math types and GLM.
///
/// Purpose: Third-party libraries the engine ships with (Diligent Engine for
///          rendering, some physics backends, certain editor tools) expose
///          their APIs in `glm::vec3` / `glm::quat` / `glm::mat4` terms.
///          Engine core stays GLM-free by routing through the thin wrappers
///          in Vec3/Quat/Mat4; this header is the one and only place GLM
///          types re-enter the public surface, and only for callers that
///          explicitly include it.
///
/// Contract:
///   - This header includes `<glm/*>` — therefore it MUST NOT be included
///     by any module that wants to stay GLM-free.  Anything in `Engine/ECS`,
///     `Engine/Replication` (server), `Engine/Simulation`, `Engine/Client`,
///     and most `Server/Networking/*` headers must NOT include this file.
///   - The only acceptable consumers are:
///       * `Engine/RHI` and `Engine/Render` .cpp files that bridge to the
///         GPU backend.
///       * Editor / tooling TUs.
///       * The Math module's own tests.
///   - Conversions are zero-cost at runtime (same memory layout) but an
///     explicit call makes the boundary visible in code review.
///
/// Design:
///   - Two flavours per conversion: `ToGlm` / `FromGlm`.  Both are inline
///     and constexpr-friendly where GLM permits.
///   - For Mat4 the layout conversion is identity: SagaEngine::Math::Mat4
///     is already column-major with 16 contiguous floats matching
///     `glm::mat4` memory.  We still wrap the cast in a function so the
///     UB of `reinterpret_cast<const glm::mat4&>` does not leak into call
///     sites.

#pragma once

#include "SagaEngine/Math/Vec3.h"
#include "SagaEngine/Math/Quat.h"
#include "SagaEngine/Math/Mat4.h"
#include "SagaEngine/Math/Transform.h"

// NOTE: including <glm/*> here is intentional.  This file is the GLM
// boundary — other headers must not include it.
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

namespace SagaEngine::Math::GlmBridge
{

// ─── Vec3 ↔ glm::vec3 ────────────────────────────────────────────────────────

/// Convert an engine Vec3 to a `glm::vec3`.  Memory layouts are identical;
/// the explicit function call is purely a code-review marker that a GLM
/// boundary is being crossed.
[[nodiscard]] inline glm::vec3 ToGlm(const Vec3& v) noexcept
{
    return glm::vec3{v.x, v.y, v.z};
}

/// Convert a `glm::vec3` into an engine Vec3.
[[nodiscard]] inline Vec3 FromGlm(const glm::vec3& v) noexcept
{
    return Vec3{v.x, v.y, v.z};
}

// ─── Quat ↔ glm::quat ────────────────────────────────────────────────────────
//
// GLM stores quaternions as (w, x, y, z) while SagaEngine stores them as
// (x, y, z, w) in memory — matches the wire format used in replication and
// matches most GPU-facing conventions.  The conversions below swap order
// explicitly so there is no "silent reordering" when switching sides.

[[nodiscard]] inline glm::quat ToGlm(const Quat& q) noexcept
{
    // glm::quat ctor is (w, x, y, z).
    return glm::quat{q.w, q.x, q.y, q.z};
}

[[nodiscard]] inline Quat FromGlm(const glm::quat& q) noexcept
{
    return Quat{q.x, q.y, q.z, q.w};
}

// ─── Mat4 ↔ glm::mat4 ────────────────────────────────────────────────────────
//
// SagaEngine::Math::Mat4 is column-major with 16 contiguous floats, exactly
// matching `glm::mat4`'s internal layout.  A constructive copy costs 16
// float assignments — cheap enough that we do NOT expose a dangerous
// reinterpret_cast helper.

[[nodiscard]] inline glm::mat4 ToGlm(const Mat4& m) noexcept
{
    glm::mat4 out{};
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            out[col][row] = m.At(row, col);
    return out;
}

[[nodiscard]] inline Mat4 FromGlm(const glm::mat4& m) noexcept
{
    Mat4 out;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            out.At(row, col) = m[col][row];
    return out;
}

// ─── Transform → glm::mat4 convenience ───────────────────────────────────────

/// Build a `glm::mat4` directly from an engine Transform — equivalent to
/// `ToGlm(Mat4::FromTransform(xf))` but avoids the double copy.
[[nodiscard]] glm::mat4 ToGlm(const Transform& xf) noexcept;

} // namespace SagaEngine::Math::GlmBridge
