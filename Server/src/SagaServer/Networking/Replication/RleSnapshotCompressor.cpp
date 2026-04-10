/// @file RleSnapshotCompressor.cpp
/// @brief Implementation of the byte-oriented RLE snapshot codec.

#include "SagaServer/Networking/Replication/RleSnapshotCompressor.h"

#include <cstring>
#include <memory>

namespace SagaEngine::Networking {

namespace
{
constexpr std::uint8_t kRepeatFlag  = 0x80; ///< Bit-7 set → repeat run.
constexpr std::uint8_t kLengthMask  = 0x7F; ///< Low 7 bits hold (length - 1).
} // namespace

// ─── Compress ───────────────────────────────────────────────────────────────

CompressionResult RleSnapshotCompressor::Compress(
    const std::uint8_t*        input,
    std::size_t                inputSize,
    std::vector<std::uint8_t>& output) const
{
    // Short-circuit the degenerate "empty payload" case.  We still emit
    // the 5-byte frame header so the receiver can decode zero-length
    // payloads without a separate code path.
    output.clear();
    output.reserve(MaxCompressedSize(inputSize));

    // ── Frame header: codec id + original size ─────────────────────────────
    output.push_back(static_cast<std::uint8_t>(SnapshotCodecId::Rle));
    const auto origSize = static_cast<std::uint32_t>(inputSize);
    output.push_back(static_cast<std::uint8_t>((origSize >> 0)  & 0xFF));
    output.push_back(static_cast<std::uint8_t>((origSize >> 8)  & 0xFF));
    output.push_back(static_cast<std::uint8_t>((origSize >> 16) & 0xFF));
    output.push_back(static_cast<std::uint8_t>((origSize >> 24) & 0xFF));

    if (inputSize == 0)
        return CompressionResult::Ok;
    if (input == nullptr)
        return CompressionResult::Corrupt;

    // Main loop walks `i` forward, collapsing each run (literal or repeat)
    // into at most one control byte plus its payload.  The two cases are
    // mutually exclusive: if the current byte equals the next, we emit a
    // repeat run; otherwise we accumulate a literal run until we either
    // see a repeat start or hit the 128-byte cap.
    std::size_t i = 0;
    while (i < inputSize)
    {
        // Detect repeat run length (>=2 identical bytes starting at i).
        std::size_t runEnd = i + 1;
        while (runEnd < inputSize && input[runEnd] == input[i] &&
               (runEnd - i) < kMaxRunLength)
        {
            ++runEnd;
        }
        const std::size_t runLen = runEnd - i;

        if (runLen >= 3)
        {
            // Worth a repeat run — saves at least 1 byte over literal.
            output.push_back(static_cast<std::uint8_t>(kRepeatFlag | (runLen - 1)));
            output.push_back(input[i]);
            i = runEnd;
            continue;
        }

        // Literal run: gather bytes until either a repeat of >=3 starts
        // or we hit the 128-byte cap.
        const std::size_t litStart = i;
        while (i < inputSize && (i - litStart) < kMaxRunLength)
        {
            // Peek ahead for a 3-byte repeat; if found, stop the literal.
            if (i + 2 < inputSize && input[i] == input[i + 1] && input[i + 1] == input[i + 2])
                break;
            ++i;
        }
        const std::size_t litLen = i - litStart;
        output.push_back(static_cast<std::uint8_t>((litLen - 1) & kLengthMask));
        output.insert(output.end(), input + litStart, input + litStart + litLen);
    }

    return CompressionResult::Ok;
}

// ─── Decompress ─────────────────────────────────────────────────────────────

CompressionResult RleSnapshotCompressor::Decompress(
    const std::uint8_t*        input,
    std::size_t                inputSize,
    std::vector<std::uint8_t>& output) const
{
    output.clear();

    // Header sanity: must be long enough for the 5-byte frame header.
    if (input == nullptr || inputSize < kFrameHeaderSize)
        return CompressionResult::Truncated;

    if (input[0] != static_cast<std::uint8_t>(SnapshotCodecId::Rle))
        return CompressionResult::CodecMismatch;

    // Read original-size field (little-endian uint32).
    const std::uint32_t originalSize =
        static_cast<std::uint32_t>(input[1]) |
        (static_cast<std::uint32_t>(input[2]) << 8) |
        (static_cast<std::uint32_t>(input[3]) << 16) |
        (static_cast<std::uint32_t>(input[4]) << 24);

    // Pre-reserve the output so decompression never reallocates.
    output.reserve(originalSize);

    std::size_t ip = kFrameHeaderSize;
    while (ip < inputSize)
    {
        const std::uint8_t control = input[ip++];
        const std::size_t  runLen  = static_cast<std::size_t>(control & kLengthMask) + 1;

        if (control & kRepeatFlag)
        {
            // Repeat run: one data byte expanded runLen times.
            if (ip + 1 > inputSize)
                return CompressionResult::Truncated;
            if (output.size() + runLen > originalSize)
                return CompressionResult::Corrupt;
            output.insert(output.end(), runLen, input[ip]);
            ip += 1;
        }
        else
        {
            // Literal run: copy runLen bytes straight from input.
            if (ip + runLen > inputSize)
                return CompressionResult::Truncated;
            if (output.size() + runLen > originalSize)
                return CompressionResult::Corrupt;
            output.insert(output.end(), input + ip, input + ip + runLen);
            ip += runLen;
        }
    }

    // Final size must match the header's original-size field — otherwise
    // either the stream was truncated on the wire or the header is lying.
    if (output.size() != originalSize)
        return CompressionResult::Corrupt;

    return CompressionResult::Ok;
}

// ─── Shared singleton ───────────────────────────────────────────────────────

SnapshotCompressorPtr RleSnapshotCompressor::Shared()
{
    // Meyers singleton — thread-safe init since C++11.  Returning a
    // shared_ptr with an empty deleter keeps the global alive for the
    // program's lifetime while still fitting the shared_ptr API.
    static const auto s_instance = std::make_shared<RleSnapshotCompressor>();
    return s_instance;
}

} // namespace SagaEngine::Networking
