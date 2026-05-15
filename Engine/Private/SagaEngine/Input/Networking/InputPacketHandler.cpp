/// @file InputPacketHandler.cpp
/// @brief Bridges Packet parsing and InputCommandInbox routing.

#include "SagaEngine/Input/Networking/InputPacketHandler.h"

#include <array>
#include <span>
#include <utility>

namespace SagaEngine::Input
{

using SagaEngine::Networking::ConnectionId;
using SagaEngine::Networking::Packet;
using SagaEngine::Networking::PacketType;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

InputPacketHandler::InputPacketHandler(ServerInputProcessor& processor)
    : m_processor(processor)
{
}

// ─── Receive path ────────────────────────────────────────────────────────────

void InputPacketHandler::OnPacketReceived(
    ConnectionId connectionId,
    const Packet& packet)
{
    if (packet.GetType() != PacketType::InputCommand)
    {
        return;
    }

    if (!packet.IsValid())
    {
        ++m_packetsRejected;
        return;
    }

    ++m_packetsReceived;

    size_t offset = 0;
    std::array<uint8_t, InputCommandSerializer::kWireSize> raw{};

    if (!packet.ReadBytes(raw.data(), raw.size(), offset))
    {
        ++m_packetsRejected;
        return;
    }

    InputCommand cmd{};
    const auto result = InputCommandSerializer::Deserialize(
        std::span<const uint8_t>(raw.data(), raw.size()),
        cmd);

    if (result != InputCommandSerializer::DeserializeResult::Ok)
    {
        ++m_packetsRejected;
        return;
    }

    InputCommandInbox* inbox = m_processor.GetInbox(connectionId);
    if (!inbox)
    {
        ++m_packetsRejected;
        return;
    }

    const auto pushResult = inbox->PushReceived(cmd);
    if (pushResult != InputCommandInbox::PushResult::Accepted)
    {
        ++m_packetsRejected;
    }
}

bool InputPacketHandler::OnRawBytesReceived(
    ConnectionId connectionId,
    const uint8_t* data,
    size_t size)
{
    Packet packet;
    if (!Packet::Deserialize(data, size, packet))
    {
        return false;
    }

    OnPacketReceived(connectionId, packet);
    return true;
}

// ─── Send path ───────────────────────────────────────────────────────────────

std::vector<InputPacketHandler::AckPacket>
InputPacketHandler::BuildAckPackets(
    const std::vector<AckEntry>& acks,
    uint32_t serverTick) const
{
    std::vector<AckPacket> out;
    out.reserve(acks.size());

    for (const auto& ack : acks)
    {
        InputAckPayload payload{
            .lastProcessedSequence = ack.sequence,
            .serverTick            = serverTick
        };

        Packet packet(PacketType::InputAck);
        packet.Write(payload);
        packet.SetTimestamp(serverTick);

        out.push_back(AckPacket{
            .connectionId = ack.clientId,
            .packet       = std::move(packet)
        });

        ++m_acksSent;
    }

    return out;
}

} // namespace SagaEngine::Input