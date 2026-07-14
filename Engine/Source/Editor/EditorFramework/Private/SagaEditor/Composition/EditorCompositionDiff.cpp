/// @file EditorCompositionDiff.cpp
/// @brief Structural diff implementation for compiled editor composition artifacts.

#include "SagaEditor/Composition/EditorCompositionDiff.h"

#include <algorithm>
#include <map>
#include <sstream>

namespace SagaEditor
{
namespace
{

template <typename T>
[[nodiscard]] std::string FingerprintDefinition(const T& definition)
{
    std::ostringstream output;
    output << definition.id << '\n' << definition.displayName;
    return output.str();
}

[[nodiscard]] std::string FingerprintDefinition(const PanelDefinition& definition)
{
    std::ostringstream output;
    output << definition.id << '\n'
           << definition.displayName << '\n'
           << definition.kind << '\n'
           << definition.defaultVisible << '\n'
           << definition.internalOnly;
    for (const std::string& profile : definition.allowedProfiles)
    {
        output << '\n' << profile;
    }
    return output.str();
}

[[nodiscard]] std::string FingerprintDefinition(const ActionDefinition& definition)
{
    std::ostringstream output;
    output << definition.id << '\n'
           << definition.displayName << '\n'
           << definition.category << '\n'
           << definition.safeInProduct << '\n'
           << definition.internalOnly;
    return output.str();
}

[[nodiscard]] std::string FingerprintDefinition(const ToolbarDefinition& definition)
{
    std::ostringstream output;
    output << definition.id << '\n' << definition.displayName;
    for (const std::string& action : definition.actions)
    {
        output << '\n' << action;
    }
    return output.str();
}

[[nodiscard]] std::string FingerprintDefinition(const WorkspaceProfileDefinition& definition)
{
    std::ostringstream output;
    output << definition.id << '\n' << definition.displayName << '\n' << definition.layoutId;
    for (const std::string& panel : definition.visiblePanels)
    {
        output << '\n' << panel;
    }
    return output.str();
}

[[nodiscard]] std::string FingerprintDefinition(const EditorModeDefinition& definition)
{
    std::ostringstream output;
    output << definition.id << '\n' << definition.displayName << '\n' << definition.workspaceId;
    for (const std::string& panel : definition.requiredPanels)
    {
        output << '\n' << panel;
    }
    return output.str();
}

template <typename T>
void DiffDefinitions(const std::vector<T>& oldDefinitions,
                     const std::vector<T>& newDefinitions,
                     const std::string& definitionType,
                     const std::string& path,
                     std::vector<EditorCompositionDiffEntry>& entries)
{
    std::map<std::string, std::string> oldById;
    std::map<std::string, std::string> newById;

    for (const T& definition : oldDefinitions)
    {
        oldById[definition.id] = FingerprintDefinition(definition);
    }
    for (const T& definition : newDefinitions)
    {
        newById[definition.id] = FingerprintDefinition(definition);
    }

    for (const auto& [id, oldFingerprint] : oldById)
    {
        auto newIt = newById.find(id);
        if (newIt == newById.end())
        {
            entries.push_back({ EditorCompositionDiffKind::RemovedDefinition,
                                definitionType,
                                id,
                                path });
        }
        else if (newIt->second != oldFingerprint)
        {
            entries.push_back({ EditorCompositionDiffKind::ChangedDefinition,
                                definitionType,
                                id,
                                path });
        }
    }

    for (const auto& [id, _] : newById)
    {
        if (!oldById.contains(id))
        {
            entries.push_back({ EditorCompositionDiffKind::AddedDefinition,
                                definitionType,
                                id,
                                path });
        }
    }
}

} // namespace

EditorCompositionDiff DiffEditorCompositionArtifacts(
    const EditorCompositionArtifact& oldArtifact,
    const EditorCompositionArtifact& newArtifact)
{
    EditorCompositionDiff diff;
    DiffDefinitions(oldArtifact.panels, newArtifact.panels, "panel", "/panels", diff.entries);
    DiffDefinitions(oldArtifact.actions, newArtifact.actions, "action", "/actions", diff.entries);
    DiffDefinitions(oldArtifact.toolbars, newArtifact.toolbars, "toolbar", "/toolbars", diff.entries);
    DiffDefinitions(oldArtifact.workspaces, newArtifact.workspaces, "workspace", "/workspaces", diff.entries);
    DiffDefinitions(oldArtifact.editorModes, newArtifact.editorModes, "editorMode", "/editorModes", diff.entries);

    std::sort(diff.entries.begin(),
              diff.entries.end(),
              [](const EditorCompositionDiffEntry& left,
                 const EditorCompositionDiffEntry& right)
              {
                  if (left.path != right.path)
                  {
                      return left.path < right.path;
                  }
                  return left.id < right.id;
              });

    return diff;
}

} // namespace SagaEditor
