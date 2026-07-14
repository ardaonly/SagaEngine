/// @file MathSimd.h
/// @brief Vector-width abstraction layer for SIMD-accelerated math paths.
///
/// Purpose: Batch-oriented systems — replication distance culling, particle
///          integration, animation sampling — benefit hugely from processing
///          four or eight entities per instruction.  This header hides the
///          intrinsic dialect behind a small `Simd4f` type so callers write
///          portable code and the backend compiles to SSE, AVX, NEON, or a
///          scalar fallback depending on the target.
///
/// Design:
///   - Architecture detection via predefined macros; the fallback path is
///     plain scalar arithmetic so unsupported targets still compile.
///   - `Simd4f` is a 4-wide single-precision value type.  It is trivially
///     copyable and must stay aligned to 16 bytes — the type itself enforces
///     this via `alignas`.
///   - Only the primitives the engine actually needs are exposed (load,
///     store, add, mul, mad, min, max, sqrt, horizontal add).  Everything
///     else can be built on top in module-local headers.
///   - No dependency on Vec3 / Quat: this is a lower-level layer those types
///     will opt into later once the SIMD paths are benchmarked.
///
/// Usage (batch distance-squared for replication graph):
///   auto dx = Sub(Load(&xs[i]), Load(&px));
///   auto dy = Sub(Load(&ys[i]), Load(&py));
///   auto dz = Sub(Load(&zs[i]), Load(&pz));
///   auto d2 = Mad(dx, dx, Mad(dy, dy, Mul(dz, dz)));
///   Store(&out[i], d2);

#pragma once

#include <cstddef>
#include <cstdint>

// ─── Architecture detection ──────────────────────────────────────────────────

#if defined(__AVX2__)
    #define SAGA_SIMD_AVX2    1
    #include <immintrin.h>
#elif defined(__SSE4_1__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64)
    #define SAGA_SIMD_SSE     1
    #include <emmintrin.h>
    #include <smmintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define SAGA_SIMD_NEON    1
    #include <arm_neon.h>
#else
    #define SAGA_SIMD_SCALAR  1
#endif

namespace SagaEngine::Math::Simd
{

// ─── Simd4f ──────────────────────────────────────────────────────────────────

/// Portable 4-wide float vector.  Storage differs per backend but the public
/// surface is identical so call sites remain portable.
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    using Simd4f = __m128;
#elif defined(SAGA_SIMD_NEON)
    using Simd4f = float32x4_t;
#else
    struct alignas(16) Simd4f
    {
        float v[4]{};
    };
#endif

// ─── Load / store ────────────────────────────────────────────────────────────

/// Load 4 aligned floats into a Simd4f register.  Pointer must be 16-byte
/// aligned.  Use LoadU for unaligned input.
[[nodiscard]] inline Simd4f Load(const float* aligned16) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    return _mm_load_ps(aligned16);
#elif defined(SAGA_SIMD_NEON)
    return vld1q_f32(aligned16);
#else
    return Simd4f{{aligned16[0], aligned16[1], aligned16[2], aligned16[3]}};
#endif
}

/// Unaligned load — slightly slower on SSE pre-Nehalem, free on modern x86
/// and NEON.  Prefer Load when alignment is already guaranteed.
[[nodiscard]] inline Simd4f LoadU(const float* ptr) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    return _mm_loadu_ps(ptr);
#elif defined(SAGA_SIMD_NEON)
    return vld1q_f32(ptr);
#else
    return Simd4f{{ptr[0], ptr[1], ptr[2], ptr[3]}};
#endif
}

/// Broadcast a scalar into all four lanes.
[[nodiscard]] inline Simd4f Splat(float scalar) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    return _mm_set1_ps(scalar);
#elif defined(SAGA_SIMD_NEON)
    return vdupq_n_f32(scalar);
#else
    return Simd4f{{scalar, scalar, scalar, scalar}};
#endif
}

/// Store 4 floats into aligned memory.  Pointer must be 16-byte aligned.
inline void Store(float* aligned16, Simd4f value) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    _mm_store_ps(aligned16, value);
#elif defined(SAGA_SIMD_NEON)
    vst1q_f32(aligned16, value);
#else
    aligned16[0] = value.v[0]; aligned16[1] = value.v[1];
    aligned16[2] = value.v[2]; aligned16[3] = value.v[3];
#endif
}

// ─── Arithmetic ──────────────────────────────────────────────────────────────

[[nodiscard]] inline Simd4f Add(Simd4f a, Simd4f b) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    return _mm_add_ps(a, b);
#elif defined(SAGA_SIMD_NEON)
    return vaddq_f32(a, b);
#else
    return Simd4f{{a.v[0]+b.v[0], a.v[1]+b.v[1], a.v[2]+b.v[2], a.v[3]+b.v[3]}};
#endif
}

[[nodiscard]] inline Simd4f Sub(Simd4f a, Simd4f b) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    return _mm_sub_ps(a, b);
#elif defined(SAGA_SIMD_NEON)
    return vsubq_f32(a, b);
#else
    return Simd4f{{a.v[0]-b.v[0], a.v[1]-b.v[1], a.v[2]-b.v[2], a.v[3]-b.v[3]}};
#endif
}

[[nodiscard]] inline Simd4f Mul(Simd4f a, Simd4f b) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    return _mm_mul_ps(a, b);
#elif defined(SAGA_SIMD_NEON)
    return vmulq_f32(a, b);
#else
    return Simd4f{{a.v[0]*b.v[0], a.v[1]*b.v[1], a.v[2]*b.v[2], a.v[3]*b.v[3]}};
#endif
}

/// Fused multiply-add: (a * b) + c.  On AVX2 / NEON this compiles to a single
/// instruction; on older SSE it decays to separate mul + add.
[[nodiscard]] inline Simd4f Mad(Simd4f a, Simd4f b, Simd4f c) noexcept
{
#if defined(SAGA_SIMD_AVX2) && defined(__FMA__)
    return _mm_fmadd_ps(a, b, c);
#elif defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    return _mm_add_ps(_mm_mul_ps(a, b), c);
#elif defined(SAGA_SIMD_NEON)
    return vmlaq_f32(c, a, b);
#else
    return Simd4f{{a.v[0]*b.v[0]+c.v[0], a.v[1]*b.v[1]+c.v[1],
                   a.v[2]*b.v[2]+c.v[2], a.v[3]*b.v[3]+c.v[3]}};
#endif
}

// ─── Reductions ──────────────────────────────────────────────────────────────

[[nodiscard]] inline Simd4f Min(Simd4f a, Simd4f b) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    return _mm_min_ps(a, b);
#elif defined(SAGA_SIMD_NEON)
    return vminq_f32(a, b);
#else
    return Simd4f{{a.v[0]<b.v[0]?a.v[0]:b.v[0], a.v[1]<b.v[1]?a.v[1]:b.v[1],
                   a.v[2]<b.v[2]?a.v[2]:b.v[2], a.v[3]<b.v[3]?a.v[3]:b.v[3]}};
#endif
}

[[nodiscard]] inline Simd4f Max(Simd4f a, Simd4f b) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    return _mm_max_ps(a, b);
#elif defined(SAGA_SIMD_NEON)
    return vmaxq_f32(a, b);
#else
    return Simd4f{{a.v[0]>b.v[0]?a.v[0]:b.v[0], a.v[1]>b.v[1]?a.v[1]:b.v[1],
                   a.v[2]>b.v[2]?a.v[2]:b.v[2], a.v[3]>b.v[3]?a.v[3]:b.v[3]}};
#endif
}

[[nodiscard]] inline Simd4f Sqrt(Simd4f a) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    return _mm_sqrt_ps(a);
#elif defined(SAGA_SIMD_NEON)
    return vsqrtq_f32(a);
#else
    // Include <cmath> would pull extra headers into every TU — inline the
    // scalar approximation via std::sqrt only when we actually need it.
    Simd4f r{};
    for (std::size_t i = 0; i < 4; ++i)
        r.v[i] = a.v[i] > 0.0f ? __builtin_sqrtf(a.v[i]) : 0.0f;
    return r;
#endif
}

/// Horizontal add — sum of the 4 lanes as a scalar.  Used to close out dot
/// products and batched reductions.
[[nodiscard]] inline float HorizontalAdd(Simd4f v) noexcept
{
#if defined(SAGA_SIMD_SSE) || defined(SAGA_SIMD_AVX2)
    __m128 shuf = _mm_movehdup_ps(v);            // [v1, v1, v3, v3]
    __m128 sums = _mm_add_ps(v, shuf);
    shuf        = _mm_movehl_ps(shuf, sums);     // [v2+v3, v3, ...]
    sums        = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
#elif defined(SAGA_SIMD_NEON)
    float32x2_t lo = vget_low_f32(v);
    float32x2_t hi = vget_high_f32(v);
    float32x2_t s  = vadd_f32(lo, hi);
    return vget_lane_f32(vpadd_f32(s, s), 0);
#else
    return v.v[0] + v.v[1] + v.v[2] + v.v[3];
#endif
}

// ─── Capability query ────────────────────────────────────────────────────────

/// Return true if the current build is using a hardware SIMD backend rather
/// than the scalar fallback — handy for logging at engine boot.
[[nodiscard]] inline constexpr bool HasHardwareSimd() noexcept
{
#if defined(SAGA_SIMD_SCALAR)
    return false;
#else
    return true;
#endif
}

/// Short name of the active backend for diagnostics.
[[nodiscard]] inline constexpr const char* BackendName() noexcept
{
#if defined(SAGA_SIMD_AVX2)
    return "AVX2";
#elif defined(SAGA_SIMD_SSE)
    return "SSE4.1";
#elif defined(SAGA_SIMD_NEON)
    return "NEON";
#else
    return "Scalar";
#endif
}

} // namespace SagaEngine::Math::Simd
