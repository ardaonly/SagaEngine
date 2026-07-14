/// @file EditorCompositionResolver.h
/// @brief Resolves compiled editor composition artifacts with safe user overlays.

#pragma once

#include "SagaEditor/Composition/EditorCompositionArtifact.h"
#include "SagaEditor/Composition/EditorCompositionDiagnostics.h"
#include "SagaEditor/Composition/EditorCompositionManifest.h"
#include "SagaEditor/Composition/ResolvedEditorCompositionSnapshot.h"
#include "SagaEditor/Customization/EditorCustomizationOverlay.h"

#include <span>

namespace SagaEditor
{

/// Resolver for producing immutable editor snapshots from compiled artifacts.
class EditorCompositionResolver
{
public:
    [[nodiscard]] ResolvedEditorCompositionSnapshot Resolve(
        const EditorCompositionManifest& manifest,
        const EditorCompositionArtifact& artifact,
        std::span<const EditorCustomizationOverlay> overlays) const;
};

} // namespace SagaEditor
