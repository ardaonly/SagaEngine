#pragma once

/// @file InputCommandSerializer.h
/// @brief Deterministic, version-aware binary serialization for InputCommand.
///
/// Layer  : Input / Commands
/// Purpose: Converts InputCommand to/from a byte buffer for network transport.
///          The network layer calls this. It knows nothing about input semantics.
///
/// Wire format (40 bytes, little-endian):
///   Offset  Size  Field
///   0       1     version
///   1       4     sequence
///   5       4     simulationTick
///   9       8     clientTimeUs
///   17      4     buttons
///   21      4     moveX  (Q16.16 fixed)
///   25      4     moveY
///   29      4     lookX
///   33      4     lookY
///   37      1     flags
///   38      2     reserved
///
/// Design notes:
///   - All multi-byte fields are little-endian.
///   - Serialization never allocates. Output goes into a caller-provided buffer.
///   - kWireSize is a compile-time constant — network layer can pre-size packets.
///   - Versioned: if version != kCurrentVersion, Deserialize returns an error.
///     Old clients talking to new servers must renegotiate version on connect.

#include "SagaEngine/Input/Commands/InputCommand.h"
#include <array>
#include <cstdint>
#include <span>

namespace SagaEngine::Input
{

class InputCommandSerializer
{
public:
    /// Wire size in bytes. Never changes for a given version.
    static constexpr size_t kWireSize = sizeof(InputCommand);  // 40

    using WireBuffer = std::array<uint8_t, kWireSize>;

    // Serialize

    /// Serialize cmd into a fixed-size buffer.
    /// Does NOT allocate. Returns the buffer by value (40 bytes).
    [[nodiscard]] static WireBuffer Serialize(const InputCommand& cmd) noexcept;

    /// Serialize into a caller-provided span (must be >= kWireSize).
    /// Returns number of bytes written, or 0 on size error.
    static size_t SerializeInto(
        const InputCommand& cmd,
        std::span<uint8_t>  out
    ) noexcept;

    // Deserialize

    enum class DeserializeResult : uint8_t
    {
        Ok              = 0,
        BufferTooSmall  = 1,
        VersionMismatch = 2,
        Corrupt         = 3,  // reserved for checksum failure if added later
    };

    /// Deserialize from a byte span into outCmd.
    /// Returns Ok on success, error code otherwise.
    [[nodiscard]] static DeserializeResult Deserialize(
        std::span<const uint8_t> data,
        InputCommand&            outCmd
    ) noexcept;

    // Batch helpers

    /// Serialize a span of commands into a contiguous output buffer.
    /// outBuffer must have capacity >= commands.size() * kWireSize.
    /// Returns total bytes written.
    static size_t SerializeBatch(
        std::span<const InputCommand> commands,
        std::span<uint8_t>            outBuffer
    ) noexcept;

    /// Deserialize multiple commands from a contiguous buffer.
    /// Stops on first error. Returns number of commands successfully deserialized.
    static size_t DeserializeBatch(
        std::span<const uint8_t>      data,
        std::span<InputCommand>       outCommands
    ) noexcept;

private:
    // Little-endian write helpers
    static void WriteU8 (uint8_t* dst, size_t& off, uint8_t  v) noexcept;
    static void WriteU32(uint8_t* dst, size_t& off, uint32_t v) noexcept;
    static void WriteU64(uint8_t* dst, size_t& off, uint64_t v) noexcept;
    static void WriteI32(uint8_t* dst, size_t& off, int32_t  v) noexcept;

    static uint8_t  ReadU8 (const uint8_t* src, size_t& off) noexcept;
    static uint32_t ReadU32(const uint8_t* src, size_t& off) noexcept;
    static uint64_t ReadU64(const uint8_t* src, size_t& off) noexcept;
    static int32_t  ReadI32(const uint8_t* src, size_t& off) noexcept;
};

} // namespace SagaEngine::Input