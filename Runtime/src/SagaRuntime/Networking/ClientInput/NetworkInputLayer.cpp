#include "SagaEngine/Input/Network/NetworkInputLayer.h"
#include <cstring>
using namespace Saga::Input::Network;

NetworkInputLayer::NetworkInputLayer(uint64_t clientId)
: m_clientId(clientId)
, m_nextSeq(1)
, m_localBuffer(4096)
, m_remoteBuffer(4096)
{
}

uint32_t NetworkInputLayer::NextSeq()
{
    std::lock_guard<std::mutex> lk(m);
    return m_nextSeq++;
}

void NetworkInputLayer::EnqueueLocal(const InputCommand& cmd)
{
    m_localBuffer.Push(cmd);
}

NetworkInputLayer::Packet NetworkInputLayer::BuildOutgoingPacket(uint32_t maxBytes)
{
    Packet p;
    p.reserve(maxBytes);
    auto maybe = m_localBuffer.GetSince(0);
    for(const auto& c : maybe) {
        auto s = SerializeCommand(c);
        if(p.size() + s.size() + 2 > maxBytes) break;
        AppendCommandToPacket(c, p);
    }
    if(m_sendCb && !p.empty()) m_sendCb(p);
    return p;
}

void NetworkInputLayer::AppendCommandToPacket(const InputCommand& c, Packet& p)
{
    auto s = SerializeCommand(c);
    uint16_t len = static_cast<uint16_t>(s.size());
    uint8_t* plen = reinterpret_cast<uint8_t*>(&len);
    p.insert(p.end(), plen, plen + sizeof(len));
    p.insert(p.end(), s.begin(), s.end());
}

void NetworkInputLayer::ReceiveIncomingPacket(const uint8_t* data, size_t size)
{
    size_t off = 0;
    while(off + 2 <= size) {
        uint16_t len;
        std::memcpy(&len, data + off, sizeof(len));
        off += 2;
        if(off + len > size) break;
        InputCommand c;
        if(DeserializeCommand(data + off, len, c)) m_remoteBuffer.Push(c);
        off += len;
    }
}

void NetworkInputLayer::ApplyAck(uint32_t ackSeq)
{
    m_localBuffer.AckUpTo(ackSeq);
}

std::vector<InputCommand> NetworkInputLayer::ConsumeRemoteCommands()
{
    return m_remoteBuffer.SnapshotAndClear();
}

void NetworkInputLayer::SetSendCallback(std::function<void(const Packet&)> cb)
{
    m_sendCb = std::move(cb);
}