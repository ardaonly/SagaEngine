#pragma once
#include "SagaEditor/Collaboration/Client/ICollaborationClient.h"
#include <memory>
namespace SagaEditor::Collaboration {
class CollaborationClient final : public ICollaborationClient {
public:
    CollaborationClient();
    ~CollaborationClient() override;
    bool Connect(const std::string& address, uint16_t port) override;
    void Disconnect() override;
    bool IsConnected() const noexcept override;
    void SendMessage(const std::string& msg) override;
    void SetOnMessage(std::function<void(const std::string&)> cb) override;
    void SetOnPeerJoined(std::function<void(const PeerInfo&)> cb) override;
    void SetOnPeerLeft(std::function<void(uint64_t)> cb) override;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
