/// @file WorkspaceCustomizationOverlayPolicy.cpp
/// @brief Workspace customization overlay path policy implementation.

#include "SagaEditor/Customization/WorkspaceCustomizationOverlayPolicy.h"

namespace SagaEditor
{

std::filesystem::path WorkspaceCustomizationOverlayPolicy::GetDefaultOverlayPath(
    const std::filesystem::path& workspaceRoot) const
{
    return workspaceRoot / ".saga" / "editor" / "customization" /
           "workspace.overlay.json";
}

} // namespace SagaEditor
