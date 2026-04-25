#pragma once
#include "SagaEditor/Collaboration/Sync/ISyncTransport.h"
#include <memory>
namespace SagaEditor::Collaboration {
class SyncTransportImpl final : public ISyncTransport {
public:
    SyncTransportImpl();
    ~SyncTransportImpl() override;
    void Send(const EditOperation& op) override;
    void SetOnReceive(std::function<void(const EditOperation&)> cb) override;
    bool IsConnected() const noexcept override;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
