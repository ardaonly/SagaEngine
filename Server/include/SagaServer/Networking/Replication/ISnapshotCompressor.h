/// @file ISnapshotCompressor.h
/// @brief Pluggable compression interface for WorldSnapshots and DeltaSnapshots.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: The replication layer must not care *how* snapshot bytes are
///          compressed — it only cares that the bytes get smaller for the
///          wire and can be round-tripped losslessly.  A pluggable interface
///          lets us start with a tiny RLE backend today (zero external
///          deps) and swap in LZ4 / zstd later without touching any
///          ReplicationManager code.
///
/// Design:
///   - Pure-virtual interface, ownership via shared_ptr.
///   - Every backend advertises its `CodecId` so a decompressing peer can
///     reject streams from unknown backends instead of silently corrupting.
///   - Streams are *frame-oriented*: one Compress() call produces one
///     independently decompressable chunk.  Streaming state is NOT kept
///     between calls — that would couple the codec to connection lifetime.
///   - All methods are thread-safe by contract.  Backends that cannot be
///     called concurrently must take an internal mutex.
///
/// Why not just "use zstd directly":
///   We will eventually use zstd (or LZ4) but that brings an external
///   dependency and a build-system change.  Shipping the interface now lets
///   every replication call site be compression-ready *today*, and lets
///   tests run against the dependency-free RLE backend.

#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

namespace SagaEngine::Networking {

// ─── Codec identification ───────────────────────────────────────────────────

/// Codec identifier written into every compressed frame.  The first byte
/// of a compressed frame is always the CodecId so the receiver can look up
/// the right backend.  Values are stable on the wire — never renumber.
enum class SnapshotCodecId : std::uint8_t
{
    None = 0,   ///< Identity codec — used for tests and tiny payloads.
    Rle  = 1,   ///< Run-length encoding — minimal, dependency-free, byte-oriented.
    Lz4  = 2,   ///< Reserved for future LZ4 backend.
    Zstd = 3,   ///< Reserved for future zstd backend.
};

/// Result codes.  An enum, not bool, because "decompress failed because
/// frame was truncated" and "decompress failed because codec mismatch"
/// have very different treatment upstream.
enum class CompressionResult : std::uint8_t
{
    Ok = 0,
    BufferTooSmall,
    CodecMismatch,
    Truncated,
    Corrupt,
    Unsupported,
};

// ─── Interface ──────────────────────────────────────────────────────────────

/// Abstract snapshot compressor.  Implementations must be stateless between
/// calls (or protect internal state with a mutex) — the replication layer
/// may call into the same instance from multiple ticks concurrently.
class ISnapshotCompressor
{
public:
    virtual ~ISnapshotCompressor() = default;

    /// Which codec this backend produces.  Burned into every frame header.
    [[nodiscard]] virtual SnapshotCodecId CodecId() const noexcept = 0;

    /// Human-readable backend name for logs / diagnostics.  Must be stable.
    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;

    /// Worst-case output size for a payload of `inputSize` bytes.  Callers
    /// use this to size their output vector before calling `Compress()`
    /// so the codec never has to reallocate under them.
    [[nodiscard]] virtual std::size_t MaxCompressedSize(std::size_t inputSize) const noexcept = 0;

    /// Compress `input` into `output`.  `output` is *replaced*, not
    /// appended to.  Returns `Ok` and fills `outBytesWritten` on success.
    [[nodiscard]] virtual CompressionResult Compress(
        const std::uint8_t*        input,
        std::size_t                inputSize,
        std::vector<std::uint8_t>& output) const = 0;

    /// Decompress a frame previously produced by `Compress()`.  The frame
    /// header's codec id is verified against `CodecId()`; a mismatch yields
    /// `CodecMismatch` and `output` is left untouched.
    [[nodiscard]] virtual CompressionResult Decompress(
        const std::uint8_t*        input,
        std::size_t                inputSize,
        std::vector<std::uint8_t>& output) const = 0;
};

using SnapshotCompressorPtr = std::shared_ptr<ISnapshotCompressor>;

} // namespace SagaEngine::Networking
