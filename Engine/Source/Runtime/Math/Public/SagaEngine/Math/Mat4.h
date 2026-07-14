/// @file Mat4.h
/// @brief Column-major 4x4 single-precision matrix for GPU upload and
///        affine transform composition.
///
/// Purpose: The RHI / render pipeline needs a concrete matrix type to hand
///          to shaders (model, view, projection) and to compose TRS chains
///          for scene hierarchy evaluation.  Transform.h holds pos/rot/scale
///          as a value type but nothing in the engine turns that into a raw
///          float[16] GPU buffer — this header fills that gap.
///
/// Convention:
///   - Column-major storage, matching GLSL / Vulkan / Metal defaults.
///     `m[column][row]` in code, `m.data[col*4 + row]` in memory.
///   - Right-handed coordinate system, same as Transform's forward = -Z.
///   - Multiplication order is mathematical: `parent * child` transforms a
///     child-space vector into parent space.
///
/// Design:
///   - 64-byte POD (exactly one cache line).
///   - No hidden SIMD — the straight scalar implementation is the reference;
///     a SIMD specialisation can be plugged in later via MathSimd.
///   - No GLM dependency — composed from Vec3, Quat, and literals only.

#pragma once

#include "SagaEngine/Math/Vec3.h"
#include "SagaEngine/Math/Quat.h"
#include "SagaEngine/Math/Transform.h"

#include <cstddef>
#include <string>

namespace SagaEngine::Math
{

// ─── Mat4 ─────────────────────────────────────────────────────────────────────

/// Column-major 4x4 float matrix.  Identity on default construction.
struct Mat4
{
    /// Linear storage, column-major.  Indexed as data[col*4 + row].
    float data[16]{
        1.0f, 0.0f, 0.0f, 0.0f,  // column 0
        0.0f, 1.0f, 0.0f, 0.0f,  // column 1
        0.0f, 0.0f, 1.0f, 0.0f,  // column 2
        0.0f, 0.0f, 0.0f, 1.0f   // column 3
    };

    // ── Construction ──────────────────────────────────────────────────────────

    constexpr Mat4() noexcept = default;

    /// Construct from 16 scalars in column-major order.  Use this only in
    /// tests or when porting pre-laid-out data; prefer the named builders.
    constexpr Mat4(float m00, float m10, float m20, float m30,
                   float m01, float m11, float m21, float m31,
                   float m02, float m12, float m22, float m32,
                   float m03, float m13, float m23, float m33) noexcept
        : data{m00, m10, m20, m30,
               m01, m11, m21, m31,
               m02, m12, m22, m32,
               m03, m13, m23, m33}
    {}

    // ── Named builders ────────────────────────────────────────────────────────

    /// Identity matrix — no transform applied.
    [[nodiscard]] static constexpr Mat4 Identity() noexcept { return Mat4{}; }

    /// Pure translation.
    [[nodiscard]] static Mat4 FromTranslation(const Vec3& t) noexcept;

    /// Pure rotation from a unit quaternion.
    [[nodiscard]] static Mat4 FromQuat(const Quat& q) noexcept;

    /// Non-uniform scale (diagonal).
    [[nodiscard]] static Mat4 FromScale(const Vec3& s) noexcept;

    /// Compose T * R * S from an engine Transform.  Applied to a point as
    /// M * p: first scale, then rotate, then translate.
    [[nodiscard]] static Mat4 FromTransform(const Transform& xf) noexcept;

    /// Right-handed look-at view matrix — camera at `eye` looking at `center`.
    [[nodiscard]] static Mat4 LookAtRH(const Vec3& eye,
                                       const Vec3& center,
                                       const Vec3& up) noexcept;

    /// Right-handed perspective projection with depth clip range [0, 1]
    /// (Vulkan / DirectX convention).  `fovYRadians` is the vertical FOV.
    [[nodiscard]] static Mat4 PerspectiveRH_ZO(float fovYRadians,
                                               float aspect,
                                               float zNear,
                                               float zFar) noexcept;

    /// Right-handed orthographic projection with clip range [0, 1].
    [[nodiscard]] static Mat4 OrthoRH_ZO(float left,  float right,
                                         float bottom, float top,
                                         float zNear,  float zFar) noexcept;

    // ── Element access ────────────────────────────────────────────────────────

    /// Element access — (row, col) convention like mathematical notation.
    [[nodiscard]] constexpr float  At(int row, int col) const noexcept
    {
        return data[col * 4 + row];
    }
    [[nodiscard]] constexpr float& At(int row, int col) noexcept
    {
        return data[col * 4 + row];
    }

    /// Raw pointer to the 16 contiguous floats — use this when uploading to
    /// a GPU buffer.  The layout matches `layout(std140) mat4` in GLSL.
    [[nodiscard]] constexpr const float* Data() const noexcept { return data; }
    [[nodiscard]] constexpr       float* Data()       noexcept { return data; }

    // ── Arithmetic ────────────────────────────────────────────────────────────

    /// Matrix multiply — returns `this * other` (this applied after other
    /// when transforming column vectors).
    [[nodiscard]] Mat4 operator*(const Mat4& other) const noexcept;

    /// Transform a point (implicit w = 1).  Result is also a point.
    [[nodiscard]] Vec3 TransformPoint(const Vec3& p) const noexcept;

    /// Transform a direction (implicit w = 0).  Ignores the translation column.
    [[nodiscard]] Vec3 TransformDirection(const Vec3& d) const noexcept;

    // ── Inversion / transpose ─────────────────────────────────────────────────

    /// Return the transpose.
    [[nodiscard]] Mat4 Transposed() const noexcept;

    /// Return the inverse.  Falls back to identity if the matrix is singular;
    /// callers that care should use InverseChecked instead.
    [[nodiscard]] Mat4 Inverse() const noexcept;

    /// Same as Inverse but reports whether the inversion succeeded.  `out` is
    /// unchanged when the return value is false.
    [[nodiscard]] bool InverseChecked(Mat4& out) const noexcept;

    // ── Diagnostics ───────────────────────────────────────────────────────────

    /// Return a human-readable four-line representation for log messages.
    [[nodiscard]] std::string ToString() const;
};

// ─── Size guarantees ─────────────────────────────────────────────────────────

static_assert(sizeof(Mat4) == 64, "Mat4 must be 16 floats with no padding");

} // namespace SagaEngine::Math
