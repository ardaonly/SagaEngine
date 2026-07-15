/// @file ServerPacketNormalizer.h
/// @brief Normalizes ServerConnection receive frames into typed packet views.

#pragma once

#include "SagaEngine/Networking/NetworkTypes.h"
#include "SagaEngine/Networking/Packet.h"

#include <cstddef>
#include <cstdint>

namespace SagaEngine::Networking
{

using SagaEngine::Networking::ClientId;
using SagaEngine::Networking::PacketType;

/// Wire layout used by ServerConnection receive callbacks:
/// [magic:2][type:1][flags:1][bodyLength:4][payload...].
inline constexpr std::size_t  kServerPacketFrameHeaderSize   = 8;
inline constexpr std::uint16_t kServerPacketFrameMagic        = 0x5347;
inline constexpr std::uint32_t kServerPacketFrameMaxBodyBytes = 65000;

/// Non-owning packet view after the server receive frame has been validated.
struct NormalizedServerPacketView
{
    ClientId            clientId{0};
    PacketType          packetType{PacketType::Invalid};
    const std::uint8_t* payload{nullptr};
    std::size_t         payloadSize{0};
};

enum class ServerPacketNormalizationStatus : std::uint8_t
{
    Success = 0,
    NullData,
    ShortFrame,
    BadMagic,
    BodyTooLarge,
    BodyLengthMismatch,
    InvalidPacketType,
};

struct ServerPacketNormalizationResult
{
    ServerPacketNormalizationStatus status{
        ServerPacketNormalizationStatus::ShortFrame};
    NormalizedServerPacketView packet{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return status == ServerPacketNormalizationStatus::Success;
    }
};

/// Validate a ServerConnection frame and expose its session id, packet type,
/// and payload without copying. The returned payload pointer is valid only as
/// long as the raw frame buffer remains alive and unmoved.
[[nodiscard]] ServerPacketNormalizationResult NormalizeServerPacketFrame(
    ClientId clientId, const std::uint8_t* data, std::size_t size) noexcept;

[[nodiscard]] bool IsKnownServerPacketType(PacketType packetType) noexcept;

} // namespace SagaEngine::Networking
