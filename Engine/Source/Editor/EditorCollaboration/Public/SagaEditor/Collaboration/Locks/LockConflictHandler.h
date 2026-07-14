#pragma once
#include "SagaEditor/Collaboration/Locks/LockEvent.h"
#include <functional>
namespace SagaEditor::Collaboration {
class LockConflictHandler {
public:
    void SetOnConflict(std::function<void(const LockEvent&)> cb);
    void HandleConflict(const LockEvent& event);
private:
    std::function<void(const LockEvent&)> m_cb;
};
} // namespace SagaEditor::Collaboration
