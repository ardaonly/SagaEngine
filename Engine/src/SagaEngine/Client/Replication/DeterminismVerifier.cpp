/// @file DeterminismVerifier.cpp
/// @brief Cross-platform determinism enforcement for replication apply.

#include "SagaEngine/Client/Replication/DeterminismVerifier.h"

#include <cfenv>
#include <cstring>

namespace SagaEngine::Client::Replication {

namespace {

/// Bit pattern for a quiet NaN (positive, payload = 1).
inline constexpr std::uint32_t kNaNBits = 0x7FC00001u;

/// Bit pattern for the largest finite float32 (~3.4028e+38).
inline constexpr std::uint32_t kMaxFiniteBits = 0x7F7FFFFFu;

/// Bit mask for the exponent field of an IEEE 754 float32.
inline constexpr std::uint32_t kExponentMask = 0x7F800000u;

/// Bit mask for the mantissa field of an IEEE 754 float32.
inline constexpr std::uint32_t kMantissaMask = 0x007FFFFFu;

/// Bit mask for the sign bit of an IEEE 754 float32.
inline constexpr std::uint32_t kSignMask = 0x80000000u;

} // namespace

// ─── FPU mode ───────────────────────────────────────────────────────────────

void EnforceDeterministicFPMode() noexcept
{
    // Lock rounding mode to nearest-even (the default, but external
    // libraries or the deterministic math layer may have changed it).
    std::fesetround(FE_TONEAREST);

    // On x86, clear the FTZ (flush-to-zero) and DAZ (denormals-are-zero)
    // bits in the MXCSR register so denormals are consistently handled.
    // On other platforms this is a no-op — denormals are handled by
    // the CanonicaliseFloat32 gate instead.
#if defined(__SSE__)
    unsigned int mxcsr = _mm_getcsr();
    mxcsr |= (1u << 15);  // FTZ: flush results to zero.
    mxcsr |= (1u << 6);   // DAZ: treat denormal inputs as zero.
    _mm_setcsr(mxcsr);
#endif
}

// ─── Canonicalise float32 ───────────────────────────────────────────────────

float CanonicaliseFloat32(float value) noexcept
{
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(std::uint32_t));

    // NaN detection: exponent all 1s, mantissa non-zero.
    if ((bits & kExponentMask) == kExponentMask && (bits & kMantissaMask) != 0)
        return 0.0f;

    // Infinity detection: exponent all 1s, mantissa zero.
    if ((bits & kExponentMask) == kExponentMask)
    {
        // Clamp to largest finite value, preserving sign.
        bits = (bits & kSignMask) | kMaxFiniteBits;
        float result;
        std::memcpy(&result, &bits, sizeof(std::uint32_t));
        return result;
    }

    // Denormal detection: exponent all 0s, mantissa non-zero.
    if ((bits & kExponentMask) == 0 && (bits & kMantissaMask) != 0)
        return 0.0f;

    return value;
}

void CanonicaliseVec3(float* x, float* y, float* z) noexcept
{
    *x = CanonicaliseFloat32(*x);
    *y = CanonicaliseFloat32(*y);
    *z = CanonicaliseFloat32(*z);
}

void CanonicaliseQuat(float* x, float* y, float* z, float* w) noexcept
{
    *x = CanonicaliseFloat32(*x);
    *y = CanonicaliseFloat32(*y);
    *z = CanonicaliseFloat32(*z);
    *w = CanonicaliseFloat32(*w);
}

// ─── Canonicalise component blob ────────────────────────────────────────────

void CanonicaliseComponentBlob(void* data, std::size_t size) noexcept
{
    if (!data || size < sizeof(std::uint32_t))
        return;

    auto* bytes = static_cast<std::uint8_t*>(data);

    // Scan for float32-sized words (4 bytes aligned to 4).
    std::size_t floatCount = size / sizeof(std::uint32_t);
    for (std::size_t i = 0; i < floatCount; ++i)
    {
        std::uint32_t bits;
        std::memcpy(&bits, bytes + i * sizeof(std::uint32_t), sizeof(std::uint32_t));

        // Quick check: if this looks like a float that needs fixing.
        std::uint32_t exp = bits & kExponentMask;
        std::uint32_t mantissa = bits & kMantissaMask;

        if (exp == kExponentMask || (exp == 0 && mantissa != 0))
        {
            float f;
            std::memcpy(&f, &bits, sizeof(std::uint32_t));
            f = CanonicaliseFloat32(f);
            std::memcpy(bytes + i * sizeof(std::uint32_t), &f, sizeof(std::uint32_t));
        }
    }
}

// ─── Bit-exact comparison ───────────────────────────────────────────────────

bool ComponentsMatch(const void* a, const void* b, std::size_t size) noexcept
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    return std::memcmp(a, b, size) == 0;
}

} // namespace SagaEngine::Client::Replication
