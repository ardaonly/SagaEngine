/// @file CommandCodec.h
/// @brief Field-by-field little-endian reader/writer for gameplay command payloads.
///
/// Layer  : SagaEngine / Gameplay / Commands
/// Purpose: Provides ByteWriter and ByteReader, the primitives every command
///          struct uses to serialise itself. Deliberately does NOT memcpy
///          whole structs — alignment, padding and cross-compiler layout
///          bugs are exactly the long-tail nightmares we are avoiding.
///
/// Design rules:
///   - Wire format is little-endian, byte-exact.
///   - Writers append to a std::vector<uint8_t>; readers walk a bounded span.
///   - Every read validates remaining bytes; underflow returns false and
///     leaves the reader in a failed state (subsequent reads no-op).
///   - No exceptions; no allocations beyond the backing vector's growth.

#pragma once

#include "SagaEngine/Gameplay/Commands/OpCode.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace SagaEngine::Gameplay::Commands
{

// ─── Writer ───────────────────────────────────────────────────────────

/// Little-endian byte writer that appends to a user-owned buffer.
/// The writer never throws; callers bound total size via kMaxGameplayCommandPayload.
class ByteWriter
{
public:
    explicit ByteWriter(std::vector<std::uint8_t>& buffer) noexcept
        : m_Buffer(buffer) {}

    void WriteU8(std::uint8_t value)
    {
        m_Buffer.push_back(value);
    }

    void WriteU16(std::uint16_t value)
    {
        m_Buffer.push_back(static_cast<std::uint8_t>(value & 0xFFu));
        m_Buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    }

    void WriteU32(std::uint32_t value)
    {
        m_Buffer.push_back(static_cast<std::uint8_t>(value & 0xFFu));
        m_Buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
        m_Buffer.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
        m_Buffer.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
    }

    void WriteU64(std::uint64_t value)
    {
        for (int i = 0; i < 8; ++i)
            m_Buffer.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFu));
    }

    void WriteI8 (std::int8_t  v) { WriteU8 (static_cast<std::uint8_t >(v)); }
    void WriteI16(std::int16_t v) { WriteU16(static_cast<std::uint16_t>(v)); }
    void WriteI32(std::int32_t v) { WriteU32(static_cast<std::uint32_t>(v)); }
    void WriteI64(std::int64_t v) { WriteU64(static_cast<std::uint64_t>(v)); }

    void WriteBool(bool v) { WriteU8(v ? 1u : 0u); }

    void WriteFloat(float value)
    {
        // Bit-cast via memcpy is the only portable way.
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        WriteU32(bits);
    }

    void WriteDouble(double value)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        WriteU64(bits);
    }

    /// Write a length-prefixed UTF-8 string. Length is uint16 (max 65535 bytes).
    /// Overlong strings are truncated silently; caller should validate upstream.
    void WriteString(std::string_view text, std::uint16_t maxBytes = 1024)
    {
        std::uint16_t len = static_cast<std::uint16_t>(
            text.size() > maxBytes ? maxBytes : text.size());
        WriteU16(len);
        m_Buffer.insert(m_Buffer.end(), text.data(), text.data() + len);
    }

    /// Write the 4-byte GameplayCommandHeader (opcode + schema version).
    void WriteHeader(OpCode op, std::uint16_t schemaVersion)
    {
        WriteU16(static_cast<std::uint16_t>(op));
        WriteU16(schemaVersion);
    }

    [[nodiscard]] std::size_t Size() const noexcept { return m_Buffer.size(); }

private:
    std::vector<std::uint8_t>& m_Buffer;
};

// ─── Reader ───────────────────────────────────────────────────────────

/// Little-endian bounded byte reader. Every read checks remaining bytes;
/// on underflow the reader becomes "failed" and subsequent reads return
/// default values while leaving Ok() == false.
class ByteReader
{
public:
    ByteReader(const std::uint8_t* data, std::size_t size) noexcept
        : m_Data(data), m_Size(size), m_Offset(0), m_Ok(data != nullptr) {}

    [[nodiscard]] bool        Ok()        const noexcept { return m_Ok; }
    [[nodiscard]] std::size_t Remaining() const noexcept { return m_Ok ? (m_Size - m_Offset) : 0; }
    [[nodiscard]] std::size_t Offset()    const noexcept { return m_Offset; }

    [[nodiscard]] std::uint8_t ReadU8()
    {
        if (!Require(1)) return 0;
        return m_Data[m_Offset++];
    }

    [[nodiscard]] std::uint16_t ReadU16()
    {
        if (!Require(2)) return 0;
        std::uint16_t v = static_cast<std::uint16_t>(m_Data[m_Offset])
                        | static_cast<std::uint16_t>(m_Data[m_Offset + 1] << 8);
        m_Offset += 2;
        return v;
    }

    [[nodiscard]] std::uint32_t ReadU32()
    {
        if (!Require(4)) return 0;
        std::uint32_t v =
              static_cast<std::uint32_t>(m_Data[m_Offset])
            | (static_cast<std::uint32_t>(m_Data[m_Offset + 1]) << 8)
            | (static_cast<std::uint32_t>(m_Data[m_Offset + 2]) << 16)
            | (static_cast<std::uint32_t>(m_Data[m_Offset + 3]) << 24);
        m_Offset += 4;
        return v;
    }

    [[nodiscard]] std::uint64_t ReadU64()
    {
        if (!Require(8)) return 0;
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v |= static_cast<std::uint64_t>(m_Data[m_Offset + i]) << (i * 8);
        m_Offset += 8;
        return v;
    }

    [[nodiscard]] std::int8_t  ReadI8 () { return static_cast<std::int8_t >(ReadU8 ()); }
    [[nodiscard]] std::int16_t ReadI16() { return static_cast<std::int16_t>(ReadU16()); }
    [[nodiscard]] std::int32_t ReadI32() { return static_cast<std::int32_t>(ReadU32()); }
    [[nodiscard]] std::int64_t ReadI64() { return static_cast<std::int64_t>(ReadU64()); }

    [[nodiscard]] bool ReadBool() { return ReadU8() != 0u; }

    [[nodiscard]] float ReadFloat()
    {
        std::uint32_t bits = ReadU32();
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    [[nodiscard]] double ReadDouble()
    {
        std::uint64_t bits = ReadU64();
        double value = 0.0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    /// Read a uint16-length-prefixed UTF-8 string. If maxBytes is exceeded
    /// the reader fails and an empty string is returned.
    [[nodiscard]] std::string ReadString(std::uint16_t maxBytes = 1024)
    {
        std::uint16_t len = ReadU16();
        if (!m_Ok || len > maxBytes || !Require(len))
        {
            m_Ok = false;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(m_Data + m_Offset), len);
        m_Offset += len;
        return s;
    }

    /// Read and validate a GameplayCommandHeader. Returns false if the opcode
    /// does not match the caller's expectation.
    [[nodiscard]] bool ReadHeader(OpCode expected, std::uint16_t& outSchemaVersion)
    {
        const std::uint16_t op = ReadU16();
        outSchemaVersion       = ReadU16();
        if (!m_Ok) return false;
        if (op != static_cast<std::uint16_t>(expected))
        {
            m_Ok = false;
            return false;
        }
        return true;
    }

    /// Peek at the opcode without consuming it (cheap dispatcher fast path).
    [[nodiscard]] static bool PeekOpCode(const std::uint8_t* data, std::size_t size,
                                          OpCode& outOp) noexcept
    {
        if (data == nullptr || size < kGameplayCommandHeaderSize)
            return false;
        const std::uint16_t op = static_cast<std::uint16_t>(data[0])
                               | static_cast<std::uint16_t>(data[1] << 8);
        outOp = static_cast<OpCode>(op);
        return true;
    }

private:
    [[nodiscard]] bool Require(std::size_t n) noexcept
    {
        if (!m_Ok || (m_Size - m_Offset) < n)
        {
            m_Ok = false;
            return false;
        }
        return true;
    }

    const std::uint8_t* m_Data   = nullptr;
    std::size_t         m_Size   = 0;
    std::size_t         m_Offset = 0;
    bool                m_Ok     = false;
};

} // namespace SagaEngine::Gameplay::Commands
