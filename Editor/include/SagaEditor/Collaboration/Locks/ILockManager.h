#pragma once
#include "SagaEditor/Collaboration/Locks/LockToken.h"
#include "SagaEditor/Collaboration/Locks/LockPolicy.h"
#include <functional>
#include <string>
namespace SagaEditor::Collaboration {
struct LockEvent;
class ILockManager {
public:
    virtual ~ILockManager() = default;
    virtual bool      TryAcquire(const std::string& objectId, LockPolicy policy, LockToken& out) = 0;
    virtual void      Release(uint64_t lockId) = 0;
    virtual bool      IsLocked(const std::string& objectId) const noexcept = 0;
    virtual void      SetOnLockEvent(std::function<void(const LockEvent&)> cb) = 0;
};
} // namespace SagaEditor::Collaboration
