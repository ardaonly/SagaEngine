#pragma once
#include "InputCommand.h"
#include "InputBuffer.h"
#include <vector>
#include <mutex>
#include <functional>
#include <cstdint>

namespace Saga::Input::Network
{
class NetworkInputLayer
{
public:
    using Packet = std::vector<uint8_t>;
    NetworkInputLayer(uint64_t clientId);
    uint32_t NextSeq();
    void EnqueueLocal(const InputCommand& cmd);
    Packet BuildOutgoingPacket(uint32_t maxBytes);
    void ReceiveIncomingPacket(const uint8_t* data, size_t size);
    void ApplyAck(uint32_t ackSeq);
    std::vector<InputCommand> ConsumeRemoteCommands();
    void SetSendCallback(std::function<void(const Packet&)> cb);
private:
    std::mutex m;
    uint64_t m_clientId;
    uint32_t m_nextSeq;
    InputBuffer m_localBuffer;
    InputBuffer m_remoteBuffer;
    std::function<void(const Packet&)> m_sendCb;
    void AppendCommandToPacket(const InputCommand& c, Packet& p);
};
}