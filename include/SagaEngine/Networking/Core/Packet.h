#pragma once
#include "NetworkTypes.h"
#include <vector>
#include <cstring>
#include <cstdint>
#include <SagaEngine/Core/Log/Log.h>

namespace SagaEngine::Networking {

// Packet Header - Fixed Size (20 bytes)
#pragma pack(push, 1)
struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t sequence;
    uint32_t timestamp;
    uint16_t payloadSize;
    uint16_t checksum;
};
#pragma pack(pop)

// Compile-time constant (struct defined above)
static constexpr size_t PACKET_HEADER_SIZE = sizeof(PacketHeader);
static_assert(PACKET_HEADER_SIZE == 20, "PacketHeader must be 20 bytes");

static constexpr uint32_t PACKET_MAGIC = 0x53414741;
static constexpr uint16_t PROTOCOL_VERSION = 1;
static constexpr size_t MAX_PACKET_SIZE = 1400;
static constexpr size_t MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - PACKET_HEADER_SIZE;

// CRC16 Implementation (CCITT)
class CRC16 {
public:
    static uint16_t Calculate(const uint8_t* data, size_t length) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < length; ++i) {
            crc ^= static_cast<uint16_t>(data[i]) << 8;
            for (int j = 0; j < 8; ++j) {
                crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
            }
        }
        return crc;
    }
    
    static bool Verify(const uint8_t* data, size_t length, uint16_t expected) {
        return Calculate(data, length) == expected;
    }
};

// Packet Class
class Packet {
public:
    Packet() = default;
    explicit Packet(PacketType type);
    ~Packet() = default;
    
    Packet(const Packet& other);
    Packet& operator=(const Packet& other);
    Packet(Packet&& other) noexcept;
    Packet& operator=(Packet&& other) noexcept;
    
    bool IsValid() const;
    bool IsHeaderValid() const;
    bool IsChecksumValid() const;
    
    PacketType GetType() const { return static_cast<PacketType>(m_Header.type); }
    uint32_t GetSequence() const { return m_Header.sequence; }
    uint32_t GetTimestamp() const { return m_Header.timestamp; }
    uint16_t GetPayloadSize() const { return m_Header.payloadSize; }
    size_t GetTotalSize() const { return PACKET_HEADER_SIZE + m_Payload.size(); }
    
    void SetSequence(uint32_t seq) { m_Header.sequence = seq; }
    void SetTimestamp(uint32_t ts) { m_Header.timestamp = ts; }
    
    template<typename T>
    bool Write(const T& value) {
        if (m_Payload.size() + sizeof(T) > MAX_PAYLOAD_SIZE) {
            LOG_WARN("Packet", "Payload size exceeded max limit");
            return false;
        }
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        m_Payload.insert(m_Payload.end(), bytes, bytes + sizeof(T));
        m_Header.payloadSize = static_cast<uint16_t>(m_Payload.size());
        UpdateChecksum();
        return true;
    }
    
    bool WriteBytes(const uint8_t* data, size_t size) {
        if (m_Payload.size() + size > MAX_PAYLOAD_SIZE) {
            LOG_WARN("Packet", "Payload size exceeded max limit");
            return false;
        }
        m_Payload.insert(m_Payload.end(), data, data + size);
        m_Header.payloadSize = static_cast<uint16_t>(m_Payload.size());
        UpdateChecksum();
        return true;
    }
    
    template<typename T>
    bool Read(T& value, size_t& offset) const {
        if (offset + sizeof(T) > m_Payload.size()) {
            return false;
        }
        std::memcpy(&value, m_Payload.data() + offset, sizeof(T));
        offset += sizeof(T);
        return true;
    }
    
    bool ReadBytes(uint8_t* buffer, size_t size, size_t& offset) const {
        if (offset + size > m_Payload.size()) {
            return false;
        }
        std::memcpy(buffer, m_Payload.data() + offset, size);
        offset += size;
        return true;
    }
    
    const uint8_t* GetData() const;
    std::vector<uint8_t> GetSerializedData() const;
    static bool Deserialize(const uint8_t* data, size_t size, Packet& outPacket);
    
    static constexpr size_t GetMaxPayloadSize() {
        return MAX_PAYLOAD_SIZE;
    }
    
    std::string ToString() const;
    
private:
    PacketHeader m_Header{};
    std::vector<uint8_t> m_Payload;
    mutable std::vector<uint8_t> m_SerializedBuffer;
    
    void UpdateChecksum();
    void UpdateMagic();
    void UpdateVersion();
    
    static uint16_t HostToNet16(uint16_t value);
    static uint32_t HostToNet32(uint32_t value);
    static uint16_t NetToHost16(uint16_t value);
    static uint32_t NetToHost32(uint32_t value);
};

inline Packet::Packet(PacketType type) {
    m_Header.magic = PACKET_MAGIC;
    m_Header.version = PROTOCOL_VERSION;
    m_Header.type = static_cast<uint16_t>(type);
    m_Header.sequence = 0;
    m_Header.timestamp = 0;
    m_Header.payloadSize = 0;
    m_Header.checksum = 0;
    m_Payload.reserve(256);
    UpdateChecksum();
}

inline Packet::Packet(const Packet& other)
    : m_Header(other.m_Header)
    , m_Payload(other.m_Payload) {
}

inline Packet& Packet::operator=(const Packet& other) {
    if (this != &other) {
        m_Header = other.m_Header;
        m_Payload = other.m_Payload;
    }
    return *this;
}

inline Packet::Packet(Packet&& other) noexcept
    : m_Header(other.m_Header)
    , m_Payload(std::move(other.m_Payload)) {
}

inline Packet& Packet::operator=(Packet&& other) noexcept {
    if (this != &other) {
        m_Header = other.m_Header;
        m_Payload = std::move(other.m_Payload);
    }
    return *this;
}

inline bool Packet::IsValid() const {
    return IsHeaderValid() && IsChecksumValid();
}

inline bool Packet::IsHeaderValid() const {
    return m_Header.magic == PACKET_MAGIC &&
           m_Header.version == PROTOCOL_VERSION &&
           m_Header.type != static_cast<uint16_t>(PacketType::Invalid);
}

inline bool Packet::IsChecksumValid() const {
    return CRC16::Verify(m_Payload.data(), m_Payload.size(), m_Header.checksum);
}

inline void Packet::UpdateChecksum() {
    m_Header.checksum = CRC16::Calculate(m_Payload.data(), m_Payload.size());
}

inline void Packet::UpdateMagic() {
    m_Header.magic = PACKET_MAGIC;
}

inline void Packet::UpdateVersion() {
    m_Header.version = PROTOCOL_VERSION;
}

inline uint16_t Packet::HostToNet16(uint16_t value) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8);
#else
    return value;
#endif
}

inline uint32_t Packet::HostToNet32(uint32_t value) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((value & 0x000000FF) << 24) |
           ((value & 0x0000FF00) << 8)  |
           ((value & 0x00FF0000) >> 8)  |
           ((value & 0xFF000000) >> 24);
#else
    return value;
#endif
}

inline uint16_t Packet::NetToHost16(uint16_t value) {
    return HostToNet16(value);
}

inline uint32_t Packet::NetToHost32(uint32_t value) {
    return HostToNet32(value);
}

inline const uint8_t* Packet::GetData() const {
    m_SerializedBuffer = GetSerializedData();
    return m_SerializedBuffer.data();
}

inline std::vector<uint8_t> Packet::GetSerializedData() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(PACKET_HEADER_SIZE + m_Payload.size());
    
    PacketHeader netHeader;
    netHeader.magic = HostToNet32(m_Header.magic);
    netHeader.version = HostToNet16(m_Header.version);
    netHeader.type = HostToNet16(m_Header.type);
    netHeader.sequence = HostToNet32(m_Header.sequence);
    netHeader.timestamp = HostToNet32(m_Header.timestamp);
    netHeader.payloadSize = HostToNet16(m_Header.payloadSize);
    netHeader.checksum = HostToNet16(m_Header.checksum);
    
    buffer.insert(buffer.end(),
                  reinterpret_cast<const uint8_t*>(&netHeader),
                  reinterpret_cast<const uint8_t*>(&netHeader) + PACKET_HEADER_SIZE);
    
    buffer.insert(buffer.end(), m_Payload.begin(), m_Payload.end());
    
    return buffer;
}

inline bool Packet::Deserialize(const uint8_t* data, size_t size, Packet& outPacket) {
    if (size < PACKET_HEADER_SIZE) {
        LOG_WARN("Packet", "Data too small for header");
        return false;
    }
    
    PacketHeader netHeader;
    std::memcpy(&netHeader, data, PACKET_HEADER_SIZE);
    
    outPacket.m_Header.magic = NetToHost32(netHeader.magic);
    outPacket.m_Header.version = NetToHost16(netHeader.version);
    outPacket.m_Header.type = NetToHost16(netHeader.type);
    outPacket.m_Header.sequence = NetToHost32(netHeader.sequence);
    outPacket.m_Header.timestamp = NetToHost32(netHeader.timestamp);
    outPacket.m_Header.payloadSize = NetToHost16(netHeader.payloadSize);
    outPacket.m_Header.checksum = NetToHost16(netHeader.checksum);
    
    if (!outPacket.IsHeaderValid()) {
        LOG_WARN("Packet", "Invalid header after deserialization");
        return false;
    }
    
    size_t payloadSize = size - PACKET_HEADER_SIZE;
    if (payloadSize != outPacket.m_Header.payloadSize) {
        LOG_WARN("Packet", "Payload size mismatch");
        return false;
    }
    
    outPacket.m_Payload.resize(payloadSize);
    std::memcpy(outPacket.m_Payload.data(),
                data + PACKET_HEADER_SIZE,
                payloadSize);
    
    if (!outPacket.IsChecksumValid()) {
        LOG_WARN("Packet", "Checksum verification failed");
        return false;
    }
    
    return true;
}

inline std::string Packet::ToString() const {
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
        "Packet[type=%s, seq=%u, ts=%u, size=%zu]",
        PacketTypeToString(GetType()),
        m_Header.sequence,
        m_Header.timestamp,
        GetTotalSize());
    return std::string(buffer);
}

} // namespace SagaEngine::Networking