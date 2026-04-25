#pragma once
#include "SagaEditor/Collaboration/Permissions/CollaboratorRole.h"
#include "SagaEditor/Collaboration/Permissions/PermissionPolicy.h"
#include <cstdint>
namespace SagaEditor::Collaboration {
enum class PermissionDeniedReason : uint8_t;
class IPermissionManager {
public:
    virtual ~IPermissionManager() = default;
    virtual CollaboratorRole GetRole(uint64_t userId)        const noexcept = 0;
    virtual void             SetRole(uint64_t userId, CollaboratorRole role)  = 0;
    virtual bool             CanPerform(uint64_t userId, EditorAction action) const noexcept = 0;
};
} // namespace SagaEditor::Collaboration
