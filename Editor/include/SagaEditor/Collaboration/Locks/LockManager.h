#pragma once
#include "SagaEditor/Collaboration/Locks/ILockManager.h"
#include <memory>
namespace SagaEditor::Collaboration {
class LockManager final : public ILockManager {
public:
    LockManager();
    ~LockManager() override;
    bool TryAcquire(const std::string& objectId, LockPolicy policy, LockToken& out) override;
    void Release(uint64_t lockId) override;
    bool IsLocked(const std::string& objectId) const noexcept override;
    void SetOnLockEvent(std::function<void(const LockEvent&)> cb) override;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
