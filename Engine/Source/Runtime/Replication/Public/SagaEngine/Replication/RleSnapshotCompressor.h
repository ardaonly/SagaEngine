/// @file RleSnapshotCompressor.h
/// @brief Dependency-free byte-oriented run-length encoder for snapshots.
///
/// Purpose: Ship a working compression backend *today*, without pulling in
///          LZ4 or zstd.  RLE is useless on random data but WorldSnapshots
///          have massive runs of zero bytes (unused component slots,
///          padded quaternions, idle dirty-masks) so the compression ratio
///          on real traffic sits around 2–4×.  Good enough to de-risk the
///          pipeline while the external-dependency work is pending.
///
/// Frame format (little-endian):
///   byte 0       : codec id (SnapshotCodecId::Rle = 1)
///   bytes 1..4   : uint32 original uncompressed size
///   bytes 5..N   : stream of runs
///
/// Run encoding:
///   Bit 7 of the control byte selects the mode:
///     0xxxxxxx  : literal run — copy the next (xxxxxxx + 1) bytes verbatim.
///     1xxxxxxx  : repeat  run — repeat the next byte (xxxxxxx + 1) times.
///   So one control byte covers runs of 1..128.  Longer runs are split
///   across multiple control bytes by the encoder.
///
/// Why not varint or LEB128:
///   Simpler encoder, simpler decoder, smaller constant-time decompress
///   loop.  We never need runs > 128 B to dominate — the point is to
///   collapse the zero-fill in packed snapshots, which we can do with a
///   fixed-width control byte just fine.

#pragma once

#include "SagaServer/Networking/Replication/ISnapshotCompressor.h"

namespace SagaEngine::Networking {

// ─── RleSnapshotCompressor ──────────────────────────────────────────────────

/// Stateless RLE codec.  Construct one per process (or use the provided
/// singleton helper) and share the shared_ptr across ReplicationManagers.
class RleSnapshotCompressor final : public ISnapshotCompressor
{
public:
    RleSnapshotCompressor() = default;

    // ── ISnapshotCompressor ─────────────────────────────────────────────────

    [[nodiscard]] SnapshotCodecId CodecId() const noexcept override
    {
        return SnapshotCodecId::Rle;
    }

    [[nodiscard]] const char* BackendName() const noexcept override
    {
        return "RLE";
    }

    [[nodiscard]] std::size_t MaxCompressedSize(std::size_t inputSize) const noexcept override
    {
        // Worst case: every input byte becomes a 1-literal run (1 control + 1 data).
        // Plus the 5-byte frame header.  +1 for integer-division rounding safety.
        return 5 + 2 * inputSize + 1;
    }

    [[nodiscard]] CompressionResult Compress(
        const std::uint8_t*        input,
        std::size_t                inputSize,
        std::vector<std::uint8_t>& output) const override;

    [[nodiscard]] CompressionResult Decompress(
        const std::uint8_t*        input,
        std::size_t                inputSize,
        std::vector<std::uint8_t>& output) const override;

    // ── Shared singleton accessor ──────────────────────────────────────────

    /// Returns a process-wide shared instance.  The codec has no state, so
    /// all ReplicationManagers can safely share the same object.
    [[nodiscard]] static SnapshotCompressorPtr Shared();

private:
    /// Maximum bytes that fit in a single run control byte (7-bit + 1).
    static constexpr std::size_t kMaxRunLength  = 128;

    /// 1-byte codec id + 4-byte original size.
    static constexpr std::size_t kFrameHeaderSize = 5;
};

} // namespace SagaEngine::Networking
