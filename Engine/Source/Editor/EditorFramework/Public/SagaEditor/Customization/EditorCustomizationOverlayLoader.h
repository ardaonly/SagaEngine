/// @file EditorCustomizationOverlayLoader.h
/// @brief Loads safe user customization overlays for editor composition snapshots.

#pragma once

#include "SagaEditor/Composition/EditorCompositionDiagnostics.h"
#include "SagaEditor/Customization/EditorCustomizationOverlay.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

/// Result of loading a safe editor customization overlay.
struct EditorCustomizationOverlayLoadResult
{
    std::optional<EditorCustomizationOverlay> overlay;
    std::vector<EditorCompositionDiagnostic> diagnostics;
};

/// Loader for user-authored safe customization overlay JSON files.
class EditorCustomizationOverlayLoader
{
public:
    [[nodiscard]] EditorCustomizationOverlayLoadResult LoadFile(
        const std::filesystem::path& path) const;

    [[nodiscard]] EditorCustomizationOverlayLoadResult LoadText(
        const std::string& text,
        std::string documentPath = {}) const;
};

} // namespace SagaEditor
