/// @file Mat4.cpp
/// @brief Scalar reference implementation of Mat4.
///
/// All expressions are written out long-hand on purpose: this file is the
/// authoritative reference the SIMD specialisations in MathSimd.h will be
/// checked against in tests.  No tricks, no `#pragma unroll`, no glm — the
/// compiler is free to vectorise but the source remains readable.

#include "SagaEngine/Math/Mat4.h"
#include "SagaEngine/Math/MathConstants.h"

#include <cmath>
#include <cstdio>

namespace SagaEngine::Math
{

// ─── Named builders ──────────────────────────────────────────────────────────

Mat4 Mat4::FromTranslation(const Vec3& t) noexcept
{
    Mat4 m;
    m.At(0, 3) = t.x;
    m.At(1, 3) = t.y;
    m.At(2, 3) = t.z;
    return m;
}

Mat4 Mat4::FromQuat(const Quat& q) noexcept
{
    // Standard quaternion-to-matrix conversion.  Assumes a unit quaternion;
    // the caller is responsible for normalisation.
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;

    Mat4 m;
    m.At(0, 0) = 1.0f - 2.0f * (yy + zz);
    m.At(1, 0) = 2.0f * (xy + wz);
    m.At(2, 0) = 2.0f * (xz - wy);

    m.At(0, 1) = 2.0f * (xy - wz);
    m.At(1, 1) = 1.0f - 2.0f * (xx + zz);
    m.At(2, 1) = 2.0f * (yz + wx);

    m.At(0, 2) = 2.0f * (xz + wy);
    m.At(1, 2) = 2.0f * (yz - wx);
    m.At(2, 2) = 1.0f - 2.0f * (xx + yy);

    return m;
}

Mat4 Mat4::FromScale(const Vec3& s) noexcept
{
    Mat4 m;
    m.At(0, 0) = s.x;
    m.At(1, 1) = s.y;
    m.At(2, 2) = s.z;
    return m;
}

Mat4 Mat4::FromTransform(const Transform& xf) noexcept
{
    // Build R * S first, then inject translation into the last column —
    // avoids a full 4x4 multiply for the common TRS case.
    Mat4 m = FromQuat(xf.rotation);

    m.At(0, 0) *= xf.scale.x; m.At(1, 0) *= xf.scale.x; m.At(2, 0) *= xf.scale.x;
    m.At(0, 1) *= xf.scale.y; m.At(1, 1) *= xf.scale.y; m.At(2, 1) *= xf.scale.y;
    m.At(0, 2) *= xf.scale.z; m.At(1, 2) *= xf.scale.z; m.At(2, 2) *= xf.scale.z;

    m.At(0, 3) = xf.position.x;
    m.At(1, 3) = xf.position.y;
    m.At(2, 3) = xf.position.z;
    return m;
}

Mat4 Mat4::LookAtRH(const Vec3& eye,
                    const Vec3& center,
                    const Vec3& up) noexcept
{
    // f = forward (from eye to target), s = side (right), u = re-orthogonalised up.
    const Vec3 f = (center - eye).Normalized();
    const Vec3 s = f.Cross(up).Normalized();
    const Vec3 u = s.Cross(f);

    Mat4 m;
    m.At(0, 0) =  s.x; m.At(0, 1) =  s.y; m.At(0, 2) =  s.z;
    m.At(1, 0) =  u.x; m.At(1, 1) =  u.y; m.At(1, 2) =  u.z;
    m.At(2, 0) = -f.x; m.At(2, 1) = -f.y; m.At(2, 2) = -f.z;

    m.At(0, 3) = -s.Dot(eye);
    m.At(1, 3) = -u.Dot(eye);
    m.At(2, 3) =  f.Dot(eye);
    return m;
}

Mat4 Mat4::PerspectiveRH_ZO(float fovYRadians,
                            float aspect,
                            float zNear,
                            float zFar) noexcept
{
    // Vulkan / D3D12 convention: depth range [0, 1], y flipped is handled
    // by the render pipeline, not here.
    const float f   = 1.0f / std::tan(fovYRadians * 0.5f);
    const float nf  = 1.0f / (zNear - zFar);

    Mat4 m;
    // Zero out identity diagonal entries we are about to overwrite.
    for (int i = 0; i < 16; ++i) m.data[i] = 0.0f;

    m.At(0, 0) = f / aspect;
    m.At(1, 1) = f;
    m.At(2, 2) = zFar * nf;
    m.At(2, 3) = zFar * zNear * nf;
    m.At(3, 2) = -1.0f;
    return m;
}

Mat4 Mat4::OrthoRH_ZO(float left,   float right,
                      float bottom, float top,
                      float zNear,  float zFar) noexcept
{
    Mat4 m;
    m.At(0, 0) =  2.0f / (right - left);
    m.At(1, 1) =  2.0f / (top - bottom);
    m.At(2, 2) = -1.0f / (zFar - zNear);

    m.At(0, 3) = -(right + left)   / (right - left);
    m.At(1, 3) = -(top   + bottom) / (top   - bottom);
    m.At(2, 3) = -zNear / (zFar - zNear);
    return m;
}

// ─── Matrix multiply ─────────────────────────────────────────────────────────

Mat4 Mat4::operator*(const Mat4& o) const noexcept
{
    Mat4 r;
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            r.At(row, col) = At(row, 0) * o.At(0, col) +
                             At(row, 1) * o.At(1, col) +
                             At(row, 2) * o.At(2, col) +
                             At(row, 3) * o.At(3, col);
        }
    }
    return r;
}

// ─── Point and direction transform ──────────────────────────────────────────

Vec3 Mat4::TransformPoint(const Vec3& p) const noexcept
{
    // Implicit w = 1; the last row is assumed to be (0, 0, 0, 1) for affine
    // transforms, so we skip the perspective divide.
    return {
        At(0,0)*p.x + At(0,1)*p.y + At(0,2)*p.z + At(0,3),
        At(1,0)*p.x + At(1,1)*p.y + At(1,2)*p.z + At(1,3),
        At(2,0)*p.x + At(2,1)*p.y + At(2,2)*p.z + At(2,3)
    };
}

Vec3 Mat4::TransformDirection(const Vec3& d) const noexcept
{
    // Implicit w = 0 — ignore the translation column.
    return {
        At(0,0)*d.x + At(0,1)*d.y + At(0,2)*d.z,
        At(1,0)*d.x + At(1,1)*d.y + At(1,2)*d.z,
        At(2,0)*d.x + At(2,1)*d.y + At(2,2)*d.z
    };
}

// ─── Transpose ───────────────────────────────────────────────────────────────

Mat4 Mat4::Transposed() const noexcept
{
    Mat4 r;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            r.At(row, col) = At(col, row);
    return r;
}

// ─── Inversion ───────────────────────────────────────────────────────────────

bool Mat4::InverseChecked(Mat4& out) const noexcept
{
    // Cofactor expansion — O(1) for a fixed 4x4 and branch-free.
    // This implementation mirrors the well-known GLM / MESA "invert 4x4"
    // formulation, written against our column-major layout.

    const float* m = data;
    float inv[16];

    inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
             + m[9]*m[7]*m[14]  + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
             - m[8]*m[7]*m[14]  - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
             + m[8]*m[7]*m[13]  + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
             - m[8]*m[6]*m[13]  - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];

    inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
             - m[9]*m[3]*m[14]  - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
             + m[8]*m[3]*m[14]  + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
             - m[8]*m[3]*m[13]  - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
             + m[8]*m[2]*m[13]  + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];

    inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]  - m[5]*m[2]*m[15]
             + m[5]*m[3]*m[14]  + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
    inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]  + m[4]*m[2]*m[15]
             - m[4]*m[3]*m[14]  - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]  - m[4]*m[1]*m[15]
             + m[4]*m[3]*m[13]  + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]  + m[4]*m[1]*m[14]
             - m[4]*m[2]*m[13]  - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];

    inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]  + m[5]*m[2]*m[11]
             - m[5]*m[3]*m[10]  - m[9]*m[2]*m[7]   + m[9]*m[3]*m[6];
    inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]  - m[4]*m[2]*m[11]
             + m[4]*m[3]*m[10]  + m[8]*m[2]*m[7]   - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]   + m[4]*m[1]*m[11]
             - m[4]*m[3]*m[9]   - m[8]*m[1]*m[7]   + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]   - m[4]*m[1]*m[10]
             + m[4]*m[2]*m[9]   + m[8]*m[1]*m[6]   - m[8]*m[2]*m[5];

    const float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (std::fabs(det) < kEpsilon)
        return false;

    const float invDet = 1.0f / det;
    for (int i = 0; i < 16; ++i)
        out.data[i] = inv[i] * invDet;
    return true;
}

Mat4 Mat4::Inverse() const noexcept
{
    Mat4 r;
    if (!InverseChecked(r))
        return Mat4::Identity();
    return r;
}

// ─── Diagnostics ─────────────────────────────────────────────────────────────

std::string Mat4::ToString() const
{
    char buf[384];
    // Print as rows so the layout matches mathematical notation even though
    // storage is column-major.
    std::snprintf(buf, sizeof(buf),
        "Mat4{\n"
        "  [%.4f %.4f %.4f %.4f]\n"
        "  [%.4f %.4f %.4f %.4f]\n"
        "  [%.4f %.4f %.4f %.4f]\n"
        "  [%.4f %.4f %.4f %.4f]\n"
        "}",
        At(0,0), At(0,1), At(0,2), At(0,3),
        At(1,0), At(1,1), At(1,2), At(1,3),
        At(2,0), At(2,1), At(2,2), At(2,3),
        At(3,0), At(3,1), At(3,2), At(3,3));
    return buf;
}

} // namespace SagaEngine::Math
