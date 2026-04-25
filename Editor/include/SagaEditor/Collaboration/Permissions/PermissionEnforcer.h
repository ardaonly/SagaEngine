#pragma once
#include "SagaEditor/Collaboration/Permissions/IPermissionManager.h"
namespace SagaEditor::Collaboration {
class PermissionEnforcer {
public:
    explicit PermissionEnforcer(IPermissionManager& mgr);
    bool Enforce(uint64_t userId, EditorAction action) const noexcept;
private:
    IPermissionManager& m_mgr;
};
} // namespace SagaEditor::Collaboration
