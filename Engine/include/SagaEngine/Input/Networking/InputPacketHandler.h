#pragma once

/// @file InputPacketHandler.h
/// @brief Bridges Packet (network layer) ↔ InputCommandInbox (input layer).
///
/// Path: Engine/include/SagaEngine/Input/Networking/InputPacketHandler.h
///
/// This is the ONLY file that knows both SagaEngine::Networking::Packet
/// and SagaEngine::Input::InputCommand. Every other class in the input
/// system knows only InputCommand. Every other class in the networking
/// system knows only Packet. This handler lives at the seam.
///
/// Packet wire format for PacketType::InputCommand:
///   [PacketHeader 20 bytes][InputCommand 40 bytes] = 60 bytes total.
///
/// Packet wire format for PacketType::InputAck:
///   [PacketHeader 20 bytes][InputAckPayload 8 bytes] = 28 bytes total.

#include "SagaEngine/Input/Networking/ServerInputProcessor.h"
#include "SagaServer/Networking/Core/Packet.h"
#include "SagaServer/Networking/Core/NetworkTypes.h"
#include "SagaEngine/Input/Commands/InputCommandSerializer.h"
#include <vector>
#include <cstdint>

namespace SagaEngine::Input
{

// ─── Ack payload ─────────────────────────────────────────────────────────────

#pragma pack(push, 1)
struct InputAckPayload
{
    uint32_t lastProcessedSequence;
    uint32_t serverTick;
};
#pragma pack(pop)
static_assert(sizeof(InputAckPayload) == 8, "InputAckPayload size mismatch");

// ─── Handler ──────────────────────────────────────────────────────────────────

class InputPacketHandler
{
public:
    explicit InputPacketHandler(ServerInputProcessor& processor);
    ~InputPacketHandler() = default;

    InputPacketHandler(const InputPacketHandler&) = delete;
    InputPacketHandler& operator=(const InputPacketHandler&) = delete;

    // ── Receive path (network recv thread) ───────────────────────────────────

    /// Decode a Packet and route to the correct InputCommandInbox.
    /// Ignores non-InputCommand packets silently.
    void OnPacketReceived(
        SagaEngine::Networking::ConnectionId    connectionId,
        const SagaEngine::Networking::Packet&   packet
    );

    /// Convenience: deserialize raw bytes first, then route.
    bool OnRawBytesReceived(
        SagaEngine::Networking::ConnectionId connectionId,
        const uint8_t*                       data,
        size_t                               size
    );

    // ── Send path (simulation thread, after ProcessTick) ─────────────────────

    struct AckPacket
    {
        SagaEngine::Networking::ConnectionId connectionId;
        SagaEngine::Networking::Packet       packet;
    };

    /// Build InputAck packets from the ack records produced by ProcessTick.
    [[nodiscard]] std::vector<AckPacket> BuildAckPackets(
        const std::vector<AckEntry>& acks,
        uint32_t                     serverTick
    ) const;

    // ── Diagnostics ──────────────────────────────────────────────────────────

    [[nodiscard]] uint64_t GetTotalPacketsReceived() const noexcept { return m_packetsReceived; }
    [[nodiscard]] uint64_t GetTotalPacketsRejected() const noexcept { return m_packetsRejected; }
    [[nodiscard]] uint64_t GetTotalAcksSent()        const noexcept { return m_acksSent; }

private:
    ServerInputProcessor& m_processor;
    mutable uint64_t      m_packetsReceived = 0;
    mutable uint64_t      m_packetsRejected = 0;
    mutable uint64_t      m_acksSent        = 0;
};

} // namespace SagaEngine::Input