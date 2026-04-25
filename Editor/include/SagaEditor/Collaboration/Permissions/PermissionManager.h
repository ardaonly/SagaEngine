#pragma once
#include "SagaEditor/Collaboration/Permissions/IPermissionManager.h"
#include <memory>
namespace SagaEditor::Collaboration {
class PermissionManager final : public IPermissionManager {
public:
    PermissionManager();
    ~PermissionManager() override;
    CollaboratorRole GetRole(uint64_t userId)        const noexcept override;
    void             SetRole(uint64_t userId, CollaboratorRole role) override;
    bool             CanPerform(uint64_t userId, EditorAction action) const noexcept override;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
