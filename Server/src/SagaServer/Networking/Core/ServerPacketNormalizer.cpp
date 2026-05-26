/// @file ServerPacketNormalizer.cpp
/// @brief Server packet frame normalization implementation.

#include "SagaServer/Networking/Core/ServerPacketNormalizer.h"

#include <cstring>

namespace SagaServer::Networking
{

namespace
{

PacketType PacketTypeFromFrameByte(std::uint8_t value) noexcept
{
    return static_cast<PacketType>(static_cast<std::uint16_t>(value));
}

} // namespace

bool IsKnownServerPacketType(PacketType packetType) noexcept
{
    switch (packetType)
    {
        case PacketType::HandshakeRequest:
        case PacketType::HandshakeResponse:
        case PacketType::Disconnect:
        case PacketType::KeepAlive:
        case PacketType::Ack:
        case PacketType::Nack:
        case PacketType::EntitySpawn:
        case PacketType::EntityDespawn:
        case PacketType::ComponentUpdate:
        case PacketType::ComponentRemove:
        case PacketType::Snapshot:
        case PacketType::DeltaSnapshot:
        case PacketType::RPCRequest:
        case PacketType::RPCResponse:
        case PacketType::RPCEvent:
        case PacketType::InputCommand:
        case PacketType::InputAck:
        case PacketType::AuthorityTransfer:
        case PacketType::AuthorityRequest:
        case PacketType::Custom:
            return true;

        case PacketType::Invalid:
        default:
            return false;
    }
}

ServerPacketNormalizationResult NormalizeServerPacketFrame(
    ClientId clientId, const std::uint8_t* data, std::size_t size) noexcept
{
    ServerPacketNormalizationResult result;

    if (data == nullptr)
    {
        result.status = ServerPacketNormalizationStatus::NullData;
        return result;
    }

    if (size < kServerPacketFrameHeaderSize)
    {
        result.status = ServerPacketNormalizationStatus::ShortFrame;
        return result;
    }

    std::uint16_t magic = 0;
    std::memcpy(&magic, data, sizeof(magic));
    if (magic != kServerPacketFrameMagic)
    {
        result.status = ServerPacketNormalizationStatus::BadMagic;
        return result;
    }

    std::uint32_t bodyLength = 0;
    std::memcpy(&bodyLength, data + 4, sizeof(bodyLength));
    if (bodyLength > kServerPacketFrameMaxBodyBytes)
    {
        result.status = ServerPacketNormalizationStatus::BodyTooLarge;
        return result;
    }

    const std::size_t expectedSize =
        kServerPacketFrameHeaderSize + static_cast<std::size_t>(bodyLength);
    if (size != expectedSize)
    {
        result.status = ServerPacketNormalizationStatus::BodyLengthMismatch;
        return result;
    }

    const PacketType packetType = PacketTypeFromFrameByte(data[2]);
    if (!IsKnownServerPacketType(packetType))
    {
        result.status = ServerPacketNormalizationStatus::InvalidPacketType;
        return result;
    }

    result.status = ServerPacketNormalizationStatus::Success;
    result.packet.clientId = clientId;
    result.packet.packetType = packetType;
    result.packet.payload = data + kServerPacketFrameHeaderSize;
    result.packet.payloadSize = bodyLength;
    return result;
}

} // namespace SagaServer::Networking
