/// @file VarInt.h
/// @brief Variable-length integer encoding (LEB128 + zigzag for signed).
///
/// Layer  : SagaEngine / Core / Encoding
/// Purpose: Fixed-width integer encodings are wasteful on the wire.  A
///          64-bit entity id whose practical range is 0…20,000,000 spends
///          38 bits every time it is transmitted.  LEB128 (the Protocol
///          Buffers varint scheme) collapses small positive integers into
///          one byte, medium ones into two, and so on — for the typical
///          distribution of ECS ids and component sizes this cuts 3-5×.
///
///          Signed integers are routed through zigzag encoding so that
///          small negatives (think velocity deltas) are as cheap as
///          small positives.  The canonical references are
///          Protocol Buffers / Cap'n Proto; the algorithm is stable and
///          will never change.
///
/// Design rules:
///   - Header-only.  The encoders are a handful of inline functions and
///     every call site gets them inlined; there is no reason to pay a
///     call-site indirection.
///   - No heap allocation.  Every encoder writes into a caller-owned
///     buffer; every decoder reads from a caller-owned buffer.
///     Out-of-range reads return `false` rather than throwing.
///   - `std::size_t` is used for sizes so this code compiles cleanly on
///     both 32-bit and 64-bit platforms.
///   - `constexpr` where the compiler is happy — some decoders mutate
///     their input cursor and cannot be constexpr before C++20.
///
/// What this header is NOT:
///   - Not a full serialisation framework.  There is no schema, no field
///     ids, no versioning.  Callers layer those concerns on top.

#pragma once

#include <cstddef>
#include <cstdint>

namespace SagaEngine::Core::Encoding {

// ─── Limits ───────────────────────────────────────────────────────────────

/// Maximum bytes a 64-bit LEB128 varint can produce.  10 bytes covers
/// the full 64-bit range (7 bits per byte × 10 = 70 ≥ 64).  Callers
/// use this to size their scratch buffers.
inline constexpr std::size_t kMaxVarInt64Bytes = 10;

/// Maximum bytes a 32-bit LEB128 varint can produce.  5 bytes covers
/// the full 32-bit range (7 × 5 = 35 ≥ 32).
inline constexpr std::size_t kMaxVarInt32Bytes = 5;

// ─── Zigzag encoding ──────────────────────────────────────────────────────

/// Fold a signed 32-bit value into an unsigned 32-bit value in which
/// small magnitudes produce small results:  0→0, -1→1, 1→2, -2→3, 2→4
/// … and so on.  The inverse is `ZigZagDecode32`.
[[nodiscard]] inline constexpr std::uint32_t ZigZagEncode32(std::int32_t value) noexcept
{
    // Arithmetic right shift replicates the sign bit, producing 0 for
    // non-negative values and ~0 for negatives; XOR with (value << 1)
    // then folds the magnitude into the low bits.
    return static_cast<std::uint32_t>((value << 1) ^ (value >> 31));
}

[[nodiscard]] inline constexpr std::int32_t ZigZagDecode32(std::uint32_t value) noexcept
{
    return static_cast<std::int32_t>((value >> 1) ^ (~(value & 1) + 1));
}

[[nodiscard]] inline constexpr std::uint64_t ZigZagEncode64(std::int64_t value) noexcept
{
    return static_cast<std::uint64_t>((value << 1) ^ (value >> 63));
}

[[nodiscard]] inline constexpr std::int64_t ZigZagDecode64(std::uint64_t value) noexcept
{
    return static_cast<std::int64_t>((value >> 1) ^ (~(value & 1) + 1));
}

// ─── LEB128 encoders ──────────────────────────────────────────────────────

/// Write an unsigned 64-bit LEB128 varint into `out`.  Returns the
/// number of bytes written (1…10).  `capacity` is the number of
/// writable bytes at `out`; a request that would overflow the buffer
/// returns 0 and leaves `out` untouched.
[[nodiscard]] inline std::size_t WriteVarUInt64(std::uint64_t value,
                                                std::uint8_t* out,
                                                std::size_t   capacity) noexcept
{
    std::size_t written = 0;
    while (value >= 0x80u)
    {
        if (written >= capacity)
            return 0;
        out[written++] = static_cast<std::uint8_t>(value) | 0x80u;
        value >>= 7;
    }
    if (written >= capacity)
        return 0;
    out[written++] = static_cast<std::uint8_t>(value);
    return written;
}

/// Write an unsigned 32-bit LEB128 varint.  Thin wrapper around the
/// 64-bit version so call sites do not have to widen by hand.
[[nodiscard]] inline std::size_t WriteVarUInt32(std::uint32_t value,
                                                std::uint8_t* out,
                                                std::size_t   capacity) noexcept
{
    return WriteVarUInt64(static_cast<std::uint64_t>(value), out, capacity);
}

/// Write a signed 64-bit value.  Internally zigzags and forwards.
[[nodiscard]] inline std::size_t WriteVarInt64(std::int64_t  value,
                                               std::uint8_t* out,
                                               std::size_t   capacity) noexcept
{
    return WriteVarUInt64(ZigZagEncode64(value), out, capacity);
}

/// Write a signed 32-bit value.  Internally zigzags and forwards.
[[nodiscard]] inline std::size_t WriteVarInt32(std::int32_t  value,
                                               std::uint8_t* out,
                                               std::size_t   capacity) noexcept
{
    return WriteVarUInt64(ZigZagEncode32(value), out, capacity);
}

// ─── LEB128 decoders ──────────────────────────────────────────────────────

/// Read an unsigned 64-bit LEB128 varint from `in`.  On success the
/// function writes the decoded value into `outValue`, advances
/// `cursor` by the number of consumed bytes, and returns true.  A
/// truncated or malformed varint returns false and leaves
/// `outValue` / `cursor` unchanged.
[[nodiscard]] inline bool ReadVarUInt64(const std::uint8_t* in,
                                        std::size_t         size,
                                        std::size_t&        cursor,
                                        std::uint64_t&      outValue) noexcept
{
    std::uint64_t result = 0;
    std::size_t   local  = cursor;
    unsigned      shift  = 0;

    while (local < size)
    {
        const std::uint8_t byte = in[local++];
        result |= static_cast<std::uint64_t>(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0u)
        {
            outValue = result;
            cursor   = local;
            return true;
        }
        shift += 7;
        if (shift >= 64)
            return false; // Overlong varint — reject before it overflows.
    }
    return false;
}

/// Read an unsigned 32-bit LEB128 varint.  Rejects values that do
/// not fit in 32 bits so the caller never sees silent truncation.
[[nodiscard]] inline bool ReadVarUInt32(const std::uint8_t* in,
                                        std::size_t         size,
                                        std::size_t&        cursor,
                                        std::uint32_t&      outValue) noexcept
{
    std::uint64_t wide = 0;
    if (!ReadVarUInt64(in, size, cursor, wide))
        return false;
    if (wide > 0xFFFFFFFFu)
        return false;
    outValue = static_cast<std::uint32_t>(wide);
    return true;
}

/// Read a signed 64-bit varint (zigzag-decoded).
[[nodiscard]] inline bool ReadVarInt64(const std::uint8_t* in,
                                       std::size_t         size,
                                       std::size_t&        cursor,
                                       std::int64_t&       outValue) noexcept
{
    std::uint64_t raw = 0;
    if (!ReadVarUInt64(in, size, cursor, raw))
        return false;
    outValue = ZigZagDecode64(raw);
    return true;
}

/// Read a signed 32-bit varint (zigzag-decoded).
[[nodiscard]] inline bool ReadVarInt32(const std::uint8_t* in,
                                       std::size_t         size,
                                       std::size_t&        cursor,
                                       std::int32_t&       outValue) noexcept
{
    std::uint32_t raw = 0;
    if (!ReadVarUInt32(in, size, cursor, raw))
        return false;
    outValue = ZigZagDecode32(static_cast<std::int32_t>(raw));
    return true;
}

// ─── Utility ──────────────────────────────────────────────────────────────

/// Compute the encoded size of an unsigned 64-bit varint without
/// actually writing it.  Useful when callers need to reserve buffer
/// space before serialising a full record.
[[nodiscard]] inline constexpr std::size_t VarUInt64Size(std::uint64_t value) noexcept
{
    std::size_t bytes = 1;
    while (value >= 0x80u)
    {
        value >>= 7;
        ++bytes;
    }
    return bytes;
}

} // namespace SagaEngine::Core::Encoding
