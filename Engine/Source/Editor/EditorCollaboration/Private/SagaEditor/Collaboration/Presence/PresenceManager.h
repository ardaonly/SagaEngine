#pragma once
#include "SagaEditor/Collaboration/Presence/IPresenceManager.h"
#include <memory>
namespace SagaEditor::Collaboration {
class PresenceManager final : public IPresenceManager {
public:
    PresenceManager();
    ~PresenceManager() override;
    void UpdateLocalPresence(const PresenceInfo& info) override;
    std::vector<PresenceInfo> GetAllPresence() const override;
    void SetOnPresenceChanged(std::function<void(const PresenceEvent&)> cb) override;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
