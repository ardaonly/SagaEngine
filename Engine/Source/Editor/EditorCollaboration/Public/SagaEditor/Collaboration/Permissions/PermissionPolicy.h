#pragma once
#include "SagaEditor/Collaboration/Permissions/CollaboratorRole.h"
#include <cstdint>
namespace SagaEditor::Collaboration {
enum class EditorAction : uint8_t { ReadScene, ModifyScene, AddAsset, DeleteAsset, ManagePeers };
class PermissionPolicy {
public:
    static bool IsAllowed(CollaboratorRole role, EditorAction action) noexcept;
};
} // namespace SagaEditor::Collaboration
