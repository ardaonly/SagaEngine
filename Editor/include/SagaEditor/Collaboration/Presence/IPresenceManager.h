#pragma once
#include "SagaEditor/Collaboration/Presence/PresenceInfo.h"
#include <functional>
#include <vector>
namespace SagaEditor::Collaboration {
struct PresenceEvent;
class IPresenceManager {
public:
    virtual ~IPresenceManager() = default;
    virtual void UpdateLocalPresence(const PresenceInfo& info) = 0;
    virtual std::vector<PresenceInfo> GetAllPresence() const   = 0;
    virtual void SetOnPresenceChanged(std::function<void(const PresenceEvent&)> cb) = 0;
};
} // namespace SagaEditor::Collaboration
