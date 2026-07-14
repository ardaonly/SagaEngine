/// @file WorkspaceCustomizationOverlayStore.h
/// @brief Loads, saves, and resets safe workspace customization overlays.

#pragma once

#include "SagaEditor/Customization/EditorCustomizationDiagnostics.h"
#include "SagaEditor/Customization/EditorCustomizationOverlay.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace SagaEditor
{

/// Result of loading an optional workspace customization overlay.
struct WorkspaceCustomizationOverlayLoadResult
{
    std::optional<EditorCustomizationOverlay> overlay;
    bool fileMissing = false;
    std::vector<EditorCustomizationDiagnostic> diagnostics;
};

/// Result of writing or resetting an overlay file.
struct WorkspaceCustomizationOverlayStoreResult
{
    bool succeeded = false;
    std::vector<EditorCustomizationDiagnostic> diagnostics;
};

/// File-backed store for safe workspace customization overlays.
class WorkspaceCustomizationOverlayStore
{
public:
    [[nodiscard]] WorkspaceCustomizationOverlayLoadResult Load(
        const std::filesystem::path& path) const;

    [[nodiscard]] WorkspaceCustomizationOverlayStoreResult Save(
        const std::filesystem::path& path,
        const EditorCustomizationOverlay& overlay) const;

    [[nodiscard]] WorkspaceCustomizationOverlayStoreResult Reset(
        const std::filesystem::path& path) const;
};

} // namespace SagaEditor
