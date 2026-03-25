#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <cstring>

namespace Saga::Input::Network
{

struct InputCommand
{
    uint32_t seq;
    uint64_t clientId;
    uint64_t timestamp;
    uint16_t type;
    std::array<float,4> payload;
};

inline void write_u16_be(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<uint8_t>(v & 0xFF));
}
inline void write_u32_be(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<uint8_t>(v & 0xFF));
}
inline void write_u64_be(std::vector<uint8_t>& b, uint64_t v) {
    for (int i = 7; i >= 0; --i) b.push_back(static_cast<uint8_t>((v >> (i*8)) & 0xFF));
}
inline uint32_t float_to_u32(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return u;
}

inline std::vector<uint8_t> SerializeCommand(const InputCommand& c)
{
    std::vector<uint8_t> b;
    b.reserve(40);
    write_u32_be(b, c.seq);
    write_u64_be(b, c.clientId);
    write_u64_be(b, c.timestamp);
    write_u16_be(b, c.type);
    for (int i = 0; i < 4; ++i) {
        write_u32_be(b, float_to_u32(c.payload[i]));
    }
    return b;
}

inline bool DeserializeCommand(const uint8_t* data, size_t size, InputCommand& out)
{
    const size_t expected = sizeof(out.seq) + sizeof(out.clientId) + sizeof(out.timestamp) + sizeof(out.type) + sizeof(uint32_t)*4;
    if (size < expected) return false;
    size_t off = 0;
    auto read_u16_be = [&](uint16_t &v) {
        v = (static_cast<uint16_t>(data[off]) << 8) | static_cast<uint16_t>(data[off+1]);
        off += 2;
    };
    auto read_u32_be = [&](uint32_t &v) {
        v = (static_cast<uint32_t>(data[off]) << 24) |
            (static_cast<uint32_t>(data[off+1]) << 16) |
            (static_cast<uint32_t>(data[off+2]) << 8) |
            (static_cast<uint32_t>(data[off+3]));
        off += 4;
    };
    auto read_u64_be = [&](uint64_t &v) {
        v = 0;
        for (int i = 0; i < 8; ++i) {
            v = (v << 8) | static_cast<uint64_t>(data[off + i]);
        }
        off += 8;
    };

    uint32_t seq;
    uint64_t clientId;
    uint64_t timestamp;
    uint16_t type;
    uint32_t pvals[4];

    read_u32_be(seq);
    read_u64_be(clientId);
    read_u64_be(timestamp);
    read_u16_be(type);
    for (int i = 0; i < 4; ++i) read_u32_be(pvals[i]);

    out.seq = seq;
    out.clientId = clientId;
    out.timestamp = timestamp;
    out.type = type;
    for (int i = 0; i < 4; ++i) {
        float f;
        std::memcpy(&f, &pvals[i], sizeof(f));
        out.payload[i] = f;
    }
    return true;
}

} // namespace