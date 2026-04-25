#pragma once
#include <functional>
#include <string>
namespace SagaEditor::Collaboration {
struct PeerInfo;
class ICollaborationClient {
public:
    virtual ~ICollaborationClient() = default;
    virtual bool Connect(const std::string& address, uint16_t port) = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const noexcept = 0;
    virtual void SendMessage(const std::string& msg) = 0;
    virtual void SetOnMessage(std::function<void(const std::string&)> cb) = 0;
    virtual void SetOnPeerJoined(std::function<void(const PeerInfo&)> cb) = 0;
    virtual void SetOnPeerLeft(std::function<void(uint64_t)> cb) = 0;
};
} // namespace SagaEditor::Collaboration
