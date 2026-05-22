/// @file EditorCompositionDiff.h
/// @brief Computes structural diffs between compiled editor composition artifacts.

#pragma once

#include "SagaEditor/Composition/EditorCompositionArtifact.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEditor
{

/// Change kind for a compiled composition definition.
enum class EditorCompositionDiffKind : uint8_t
{
    AddedDefinition,
    RemovedDefinition,
    ChangedDefinition,
};

/// One structural difference between two compiled composition artifacts.
struct EditorCompositionDiffEntry
{
    EditorCompositionDiffKind kind = EditorCompositionDiffKind::ChangedDefinition;
    std::string definitionType;
    std::string id;
    std::string path;
};

/// Complete diff between two composition artifacts.
struct EditorCompositionDiff
{
    std::vector<EditorCompositionDiffEntry> entries;
};

/// Compute a deterministic top-level definition diff.
[[nodiscard]] EditorCompositionDiff DiffEditorCompositionArtifacts(
    const EditorCompositionArtifact& oldArtifact,
    const EditorCompositionArtifact& newArtifact);

} // namespace SagaEditor
