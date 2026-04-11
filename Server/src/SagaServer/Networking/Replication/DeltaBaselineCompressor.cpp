/// @file DeltaBaselineCompressor.cpp
/// @brief XOR-baseline delta compressor implementation.

#include "SagaServer/Networking/Replication/DeltaBaselineCompressor.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace SagaEngine::Networking {

namespace {

// ─── RLE run control bytes ───────────────────────────────────────────────
//
// Mirrors the layout in `RleSnapshotCompressor.cpp` so the two codecs
// share a wire-compatible inner stream.  High bit = repeat run,
// low 7 bits = (length − 1).

constexpr std::uint8_t kRepeatFlag   = 0x80;
constexpr std::uint8_t kLengthMask   = 0x7F;
constexpr std::size_t  kMaxRunLength = 128;

/// FNV-1a 32-bit hash used to tag baselines.  Not cryptographically
/// strong, but all we need is "this is almost certainly not the
/// wrong buffer" for a few kilobytes of data.
[[nodiscard]] std::uint32_t Fnv1a32(const std::uint8_t* data,
                                    std::size_t         size) noexcept
{
    std::uint32_t hash = 0x811C9DC5u;
    for (std::size_t i = 0; i < size; ++i)
    {
        hash ^= data[i];
        hash *= 0x01000193u;
    }
    return hash;
}

/// Append a little-endian uint32 to the tail of `buf`.  Shared
/// helper used by both the header-write path and the hash stamp.
void AppendLE32(std::vector<std::uint8_t>& buf, std::uint32_t value)
{
    buf.push_back(static_cast<std::uint8_t>((value >> 0)  & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((value >> 8)  & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    buf.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

/// Read a little-endian uint32 from `in[offset..offset+4]`.  No
/// bounds check — the caller has already verified the frame header
/// is large enough.
[[nodiscard]] std::uint32_t ReadLE32(const std::uint8_t* in, std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(in[offset + 0])       |
           static_cast<std::uint32_t>(in[offset + 1]) << 8  |
           static_cast<std::uint32_t>(in[offset + 2]) << 16 |
           static_cast<std::uint32_t>(in[offset + 3]) << 24;
}

} // namespace

// ─── BaselineHash ─────────────────────────────────────────────────────────

std::uint32_t DeltaBaselineCompressor::BaselineHash(
    const std::uint8_t* data,
    std::size_t         size) noexcept
{
    return Fnv1a32(data, size);
}

// ─── Shared ───────────────────────────────────────────────────────────────

std::shared_ptr<DeltaBaselineCompressor> DeltaBaselineCompressor::Shared()
{
    static auto instance = std::make_shared<DeltaBaselineCompressor>();
    return instance;
}

// ─── EncodeDelta ──────────────────────────────────────────────────────────

CompressionResult DeltaBaselineCompressor::EncodeDelta(
    const std::uint8_t*        current,
    std::size_t                currentSize,
    const std::uint8_t*        baseline,
    std::size_t                baselineSize,
    std::vector<std::uint8_t>& output) const
{
    output.clear();
    // Conservative reserve — assume the worst-case "every byte
    // becomes a one-byte literal run" to avoid reallocating
    // mid-encode.  The header is 9 bytes on top of that.
    output.reserve(kFrameHeaderSize + 2 * currentSize + 1);

    // ── Frame header ─────────────────────────────────────────────────────
    output.push_back(kFrameCodecTag);
    AppendLE32(output, static_cast<std::uint32_t>(currentSize));
    AppendLE32(output, Fnv1a32(baseline, baselineSize));

    if (currentSize == 0)
        return CompressionResult::Ok;

    if (current == nullptr)
        return CompressionResult::Corrupt;

    // ── XOR into a scratch buffer ────────────────────────────────────────
    //
    // The scratch is sized to the *current* snapshot.  Bytes past
    // the end of the baseline are treated as zero, so the XOR
    // simplifies to "copy the tail verbatim" in that case.  Bytes
    // past the end of the current snapshot are ignored — they are
    // reconstructed on the decoder side as baseline-sized zero fill.
    std::vector<std::uint8_t> xored(currentSize, 0);
    const std::size_t overlap = std::min(currentSize, baselineSize);
    for (std::size_t i = 0; i < overlap; ++i)
        xored[i] = current[i] ^ baseline[i];
    for (std::size_t i = overlap; i < currentSize; ++i)
        xored[i] = current[i];

    // ── Run-length encode the XOR stream ─────────────────────────────────
    EncodeRleStream(xored.data(), xored.size(), output);

    return CompressionResult::Ok;
}

// ─── EncodeRleStream ──────────────────────────────────────────────────────

void DeltaBaselineCompressor::EncodeRleStream(
    const std::uint8_t*        xored,
    std::size_t                size,
    std::vector<std::uint8_t>& output) const
{
    std::size_t i = 0;
    while (i < size)
    {
        // Measure the repeat-run length starting at `i`.  Capped at
        // `kMaxRunLength` because the control byte only has 7 bits
        // of length field.
        std::size_t runEnd = i + 1;
        while (runEnd < size &&
               xored[runEnd] == xored[i] &&
               (runEnd - i) < kMaxRunLength)
        {
            ++runEnd;
        }
        const std::size_t runLen = runEnd - i;

        if (runLen >= 3)
        {
            // Worth a repeat run — 2 bytes total vs. N+1 bytes as a
            // literal.  Below three we stay in literal mode because
            // a 3-byte literal is also 2 bytes of overhead but
            // covers heterogenous tails more gracefully.
            output.push_back(
                static_cast<std::uint8_t>(kRepeatFlag | ((runLen - 1) & kLengthMask)));
            output.push_back(xored[i]);
            i = runEnd;
            continue;
        }

        // Accumulate a literal run from `i` up to the next 3-byte
        // repeat or the cap, whichever comes first.
        std::size_t literalEnd = i + 1;
        while (literalEnd < size && (literalEnd - i) < kMaxRunLength)
        {
            // Peek ahead for a 3-byte repeat starting at
            // `literalEnd`.  If we find one we stop the literal
            // right before it so the repeat gets its own run.
            if (literalEnd + 2 < size &&
                xored[literalEnd]     == xored[literalEnd + 1] &&
                xored[literalEnd + 1] == xored[literalEnd + 2])
            {
                break;
            }
            ++literalEnd;
        }

        const std::size_t literalLen = literalEnd - i;
        output.push_back(static_cast<std::uint8_t>((literalLen - 1) & kLengthMask));
        for (std::size_t k = 0; k < literalLen; ++k)
            output.push_back(xored[i + k]);
        i = literalEnd;
    }
}

// ─── DecodeDelta ──────────────────────────────────────────────────────────

CompressionResult DeltaBaselineCompressor::DecodeDelta(
    const std::uint8_t*        input,
    std::size_t                inputSize,
    const std::uint8_t*        baseline,
    std::size_t                baselineSize,
    std::vector<std::uint8_t>& output) const
{
    output.clear();

    if (inputSize < kFrameHeaderSize)
        return CompressionResult::Truncated;
    if (input == nullptr)
        return CompressionResult::Corrupt;

    // ── Validate frame header ────────────────────────────────────────────
    if (input[0] != kFrameCodecTag)
        return CompressionResult::CodecMismatch;

    const std::uint32_t origSize     = ReadLE32(input, 1);
    const std::uint32_t baselineHash = ReadLE32(input, 5);

    const std::uint32_t observedHash = Fnv1a32(baseline, baselineSize);
    if (baselineHash != observedHash)
        return CompressionResult::CodecMismatch;

    // Empty payload — zero-length snapshot is a legitimate "nothing
    // changed" encoding.  Short-circuit to avoid walking an empty
    // run stream.
    if (origSize == 0)
        return CompressionResult::Ok;

    // ── Decode the XOR run stream into a scratch buffer ─────────────────
    std::vector<std::uint8_t> xored;
    xored.reserve(origSize);
    if (!DecodeRleStream(input + kFrameHeaderSize,
                         inputSize - kFrameHeaderSize,
                         origSize,
                         xored))
    {
        return CompressionResult::Corrupt;
    }

    if (xored.size() != origSize)
        return CompressionResult::Corrupt;

    // ── Fold the baseline back in ───────────────────────────────────────
    //
    // Bytes covered by both buffers are xored; bytes past the end
    // of the baseline are the raw payload (the encoder made that
    // choice); bytes past the end of the XOR stream cannot exist
    // because we allocated exactly `origSize`.
    output.resize(origSize);
    const std::size_t overlap = std::min<std::size_t>(origSize, baselineSize);
    for (std::size_t i = 0; i < overlap; ++i)
        output[i] = xored[i] ^ baseline[i];
    for (std::size_t i = overlap; i < origSize; ++i)
        output[i] = xored[i];

    return CompressionResult::Ok;
}

// ─── DecodeRleStream ──────────────────────────────────────────────────────

bool DeltaBaselineCompressor::DecodeRleStream(
    const std::uint8_t*        in,
    std::size_t                size,
    std::size_t                expectedSize,
    std::vector<std::uint8_t>& output) const
{
    std::size_t i = 0;
    while (i < size)
    {
        const std::uint8_t control = in[i++];
        const std::size_t  runLen  = static_cast<std::size_t>(control & kLengthMask) + 1;

        if ((control & kRepeatFlag) != 0u)
        {
            // Repeat run — one payload byte copied runLen times.
            if (i >= size)
                return false;
            const std::uint8_t value = in[i++];
            if (output.size() + runLen > expectedSize)
                return false;
            output.insert(output.end(), runLen, value);
        }
        else
        {
            // Literal run — runLen bytes copied verbatim.
            if (i + runLen > size)
                return false;
            if (output.size() + runLen > expectedSize)
                return false;
            output.insert(output.end(), in + i, in + i + runLen);
            i += runLen;
        }
    }
    return true;
}

} // namespace SagaEngine::Networking
