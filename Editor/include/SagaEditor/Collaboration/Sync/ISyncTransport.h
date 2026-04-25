#pragma once
#include "SagaEditor/Collaboration/Sync/EditOperation.h"
#include <functional>
namespace SagaEditor::Collaboration {
class ISyncTransport {
public:
    virtual ~ISyncTransport() = default;
    virtual void Send(const EditOperation& op) = 0;
    virtual void SetOnReceive(std::function<void(const EditOperation&)> cb) = 0;
    virtual bool IsConnected() const noexcept = 0;
};
} // namespace SagaEditor::Collaboration
