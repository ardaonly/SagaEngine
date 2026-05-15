/// @file InputCommandSerializer.cpp
/// @brief Deterministic little-endian binary serialization for InputCommand.

#include "SagaEngine/Input/Commands/InputCommandSerializer.h"
#include <cstring>
#include <algorithm>

namespace SagaEngine::Input
{

// ─── Little-endian write helpers ─────────────────────────────────────────────

void InputCommandSerializer::WriteU8(uint8_t* dst, size_t& off, uint8_t v) noexcept
{
    dst[off++] = v;
}

void InputCommandSerializer::WriteU32(uint8_t* dst, size_t& off, uint32_t v) noexcept
{
    dst[off++] = static_cast<uint8_t>(v);
    dst[off++] = static_cast<uint8_t>(v >> 8);
    dst[off++] = static_cast<uint8_t>(v >> 16);
    dst[off++] = static_cast<uint8_t>(v >> 24);
}

void InputCommandSerializer::WriteU64(uint8_t* dst, size_t& off, uint64_t v) noexcept
{
    dst[off++] = static_cast<uint8_t>(v);
    dst[off++] = static_cast<uint8_t>(v >> 8);
    dst[off++] = static_cast<uint8_t>(v >> 16);
    dst[off++] = static_cast<uint8_t>(v >> 24);
    dst[off++] = static_cast<uint8_t>(v >> 32);
    dst[off++] = static_cast<uint8_t>(v >> 40);
    dst[off++] = static_cast<uint8_t>(v >> 48);
    dst[off++] = static_cast<uint8_t>(v >> 56);
}

void InputCommandSerializer::WriteI32(uint8_t* dst, size_t& off, int32_t v) noexcept
{
    WriteU32(dst, off, static_cast<uint32_t>(v));
}

// ─── Little-endian read helpers ───────────────────────────────────────────────

uint8_t InputCommandSerializer::ReadU8(const uint8_t* src, size_t& off) noexcept
{
    return src[off++];
}

uint32_t InputCommandSerializer::ReadU32(const uint8_t* src, size_t& off) noexcept
{
    uint32_t v  = static_cast<uint32_t>(src[off]);
             v |= static_cast<uint32_t>(src[off+1]) << 8;
             v |= static_cast<uint32_t>(src[off+2]) << 16;
             v |= static_cast<uint32_t>(src[off+3]) << 24;
    off += 4;
    return v;
}

uint64_t InputCommandSerializer::ReadU64(const uint8_t* src, size_t& off) noexcept
{
    uint64_t v  = static_cast<uint64_t>(src[off]);
             v |= static_cast<uint64_t>(src[off+1]) << 8;
             v |= static_cast<uint64_t>(src[off+2]) << 16;
             v |= static_cast<uint64_t>(src[off+3]) << 24;
             v |= static_cast<uint64_t>(src[off+4]) << 32;
             v |= static_cast<uint64_t>(src[off+5]) << 40;
             v |= static_cast<uint64_t>(src[off+6]) << 48;
             v |= static_cast<uint64_t>(src[off+7]) << 56;
    off += 8;
    return v;
}

int32_t InputCommandSerializer::ReadI32(const uint8_t* src, size_t& off) noexcept
{
    return static_cast<int32_t>(ReadU32(src, off));
}

// ─── Serialize ────────────────────────────────────────────────────────────────

InputCommandSerializer::WireBuffer
InputCommandSerializer::Serialize(const InputCommand& cmd) noexcept
{
    WireBuffer buf{};
    size_t off = 0;
    WriteU8 (buf.data(), off, cmd.version);
    WriteU32(buf.data(), off, cmd.sequence);
    WriteU32(buf.data(), off, cmd.simulationTick);
    WriteU64(buf.data(), off, cmd.clientTimeUs);
    WriteU32(buf.data(), off, cmd.buttons);
    WriteI32(buf.data(), off, cmd.moveX);
    WriteI32(buf.data(), off, cmd.moveY);
    WriteI32(buf.data(), off, cmd.lookX);
    WriteI32(buf.data(), off, cmd.lookY);
    WriteU8 (buf.data(), off, cmd.flags);
    WriteU8 (buf.data(), off, 0);  // reserved[0]
    WriteU8 (buf.data(), off, 0);  // reserved[1]
    // off == 40 == kWireSize
    return buf;
}

size_t InputCommandSerializer::SerializeInto(
    const InputCommand& cmd,
    std::span<uint8_t>  out) noexcept
{
    if (out.size() < kWireSize) return 0;
    const auto buf = Serialize(cmd);
    std::memcpy(out.data(), buf.data(), kWireSize);
    return kWireSize;
}

// ─── Deserialize ─────────────────────────────────────────────────────────────

InputCommandSerializer::DeserializeResult
InputCommandSerializer::Deserialize(
    std::span<const uint8_t> data,
    InputCommand&            outCmd) noexcept
{
    if (data.size() < kWireSize)
        return DeserializeResult::BufferTooSmall;

    size_t off = 0;
    const uint8_t version = ReadU8(data.data(), off);

    if (version != InputCommand::kCurrentVersion)
        return DeserializeResult::VersionMismatch;

    outCmd.version        = version;
    outCmd.sequence       = ReadU32(data.data(), off);
    outCmd.simulationTick = ReadU32(data.data(), off);
    outCmd.clientTimeUs   = ReadU64(data.data(), off);
    outCmd.buttons        = ReadU32(data.data(), off);
    outCmd.moveX          = ReadI32(data.data(), off);
    outCmd.moveY          = ReadI32(data.data(), off);
    outCmd.lookX          = ReadI32(data.data(), off);
    outCmd.lookY          = ReadI32(data.data(), off);
    outCmd.flags          = ReadU8 (data.data(), off);
    // reserved bytes: skip
    return DeserializeResult::Ok;
}

// ─── Batch ────────────────────────────────────────────────────────────────────

size_t InputCommandSerializer::SerializeBatch(
    std::span<const InputCommand> commands,
    std::span<uint8_t>            outBuffer) noexcept
{
    if (outBuffer.size() < commands.size() * kWireSize) return 0;

    size_t written = 0;
    for (const auto& cmd : commands)
    {
        const auto buf = Serialize(cmd);
        std::memcpy(outBuffer.data() + written, buf.data(), kWireSize);
        written += kWireSize;
    }
    return written;
}

size_t InputCommandSerializer::DeserializeBatch(
    std::span<const uint8_t> data,
    std::span<InputCommand>  outCommands) noexcept
{
    size_t offset = 0;
    size_t count  = 0;

    while (offset + kWireSize <= data.size() && count < outCommands.size())
    {
        const auto result = Deserialize(
            data.subspan(offset, kWireSize),
            outCommands[count]
        );
        if (result != DeserializeResult::Ok) break;
        offset += kWireSize;
        ++count;
    }
    return count;
}

} // namespace SagaEngine::Input