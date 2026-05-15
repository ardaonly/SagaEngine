/// @file CacheLine.h
/// @brief Cache-line size constant + alignment helpers to avoid false sharing.
///
/// Purpose: Hot, frequently-written counters (tick stats, network rx/tx
///          totals, job completion counters) live in shared data that
///          multiple threads touch.  If two unrelated counters land on the
///          same cache line the CPU's coherency protocol ping-pongs the line
///          between cores, silently killing throughput.  This header exposes
///          the knobs needed to pad and align such counters correctly.
///
/// Design:
///   - `kCacheLineSize` is a compile-time constant derived from the target
///     CPU.  x86/x64 and most ARM64 chips use 64 bytes; Apple M1/M2 cores
///     use 128 bytes for some L1D lines — we default to the larger value on
///     Apple Silicon so we never get false sharing, at the cost of a bit of
///     padding.
///   - `SAGA_CACHE_ALIGNED` is the macro to put in front of members or
///     structs that must sit on their own line.
///   - `CacheLinePad<T>` wraps a value and appends padding so `sizeof` of
///     the wrapper is always exactly one cache line.
///
/// Usage:
///   struct ZoneStats {
///       SAGA_CACHE_ALIGNED std::atomic<std::uint64_t> packetsReceived{0};
///       SAGA_CACHE_ALIGNED std::atomic<std::uint64_t> packetsSent{0};
///   };

#pragma once

#include <cstddef>
#include <new>

namespace SagaEngine::Core::Threading
{

// ─── Cache line size constant ────────────────────────────────────────────────

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
    /// Preferred: ask the standard library what the implementation thinks the
    /// L1D line size is.  Works on recent libstdc++ and libc++; silently
    /// absent on older toolchains, which is why we fall through to the
    /// platform detection below.
    inline constexpr std::size_t kCacheLineSize =
        std::hardware_destructive_interference_size;
#elif defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
    /// Apple Silicon L1D uses 128-byte lines on performance cores.
    inline constexpr std::size_t kCacheLineSize = 128;
#elif defined(__powerpc__) || defined(__powerpc64__)
    /// POWER8/POWER9 uses 128-byte lines.
    inline constexpr std::size_t kCacheLineSize = 128;
#else
    /// x86, x64, ARMv7/ARMv8 mainstream — 64 bytes.
    inline constexpr std::size_t kCacheLineSize = 64;
#endif

// ─── Alignment macro ─────────────────────────────────────────────────────────

/// Place in front of a member or type that must not share a cache line with
/// neighbouring fields.  Expands to `alignas(kCacheLineSize)`.
#define SAGA_CACHE_ALIGNED alignas(::SagaEngine::Core::Threading::kCacheLineSize)

// ─── Padded value wrapper ────────────────────────────────────────────────────

/// Wrap a value and pad it out to fill exactly one cache line.  Useful for
/// per-thread slots in arrays where each worker gets its own line.
/// `sizeof(CacheLinePad<T>) == kCacheLineSize` by construction.
template <typename T>
struct alignas(kCacheLineSize) CacheLinePad
{
    T value{};

private:
    // Calculate remaining bytes after the value so the total size matches
    // exactly one cache line.  Guarded so a T that is already bigger than a
    // line still compiles (it will just occupy ceil(sizeof(T)/line) lines).
    static constexpr std::size_t kPaddingBytes =
        (sizeof(T) < kCacheLineSize) ? (kCacheLineSize - sizeof(T)) : 0;

    char padding_[kPaddingBytes == 0 ? 1 : kPaddingBytes]{};

public:
    constexpr CacheLinePad() noexcept = default;

    /// Forward construction so the wrapper is a drop-in replacement for `T`.
    constexpr explicit CacheLinePad(const T& v) noexcept : value(v) {}

    /// Implicit access to the underlying value for call-site ergonomics.
    [[nodiscard]] constexpr T&       Get()       noexcept { return value; }
    [[nodiscard]] constexpr const T& Get() const noexcept { return value; }
};

// Static sanity check: the wrapper sizes to a whole number of cache lines.
static_assert(sizeof(CacheLinePad<int>) % kCacheLineSize == 0,
              "CacheLinePad must be a multiple of kCacheLineSize");

} // namespace SagaEngine::Core::Threading
