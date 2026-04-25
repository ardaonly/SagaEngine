#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor::Collaboration {
class ICollaborationServer {
public:
    virtual ~ICollaborationServer() = default;
    virtual bool Start(uint16_t port) = 0;
    virtual void Stop()               = 0;
    virtual bool IsRunning() const noexcept = 0;
    virtual void BroadcastMessage(const std::string& msg) = 0;
    virtual uint32_t GetConnectedClientCount() const noexcept = 0;
};
} // namespace SagaEditor::Collaboration
