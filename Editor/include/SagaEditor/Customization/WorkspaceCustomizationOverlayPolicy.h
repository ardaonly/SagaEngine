/// @file WorkspaceCustomizationOverlayPolicy.h
/// @brief Resolves safe workspace customization overlay storage paths.

#pragma once

#include <filesystem>

namespace SagaEditor
{

/// Path policy for user-private workspace customization overlays.
class WorkspaceCustomizationOverlayPolicy
{
public:
    /// Return the default overlay file under a workspace root.
    [[nodiscard]] std::filesystem::path GetDefaultOverlayPath(
        const std::filesystem::path& workspaceRoot) const;
};

} // namespace SagaEditor
