/// @file WorkspaceCustomizationOverlayStore.cpp
/// @brief Workspace customization overlay store implementation.

#include "SagaEditor/Customization/WorkspaceCustomizationOverlayStore.h"

#include "SagaEditor/Composition/EditorCompositionDiagnostics.h"
#include "SagaEditor/Customization/EditorCustomizationOverlayLoader.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace SagaEditor
{
namespace
{

using Json = nlohmann::json;

[[nodiscard]] EditorCustomizationDiagnosticSeverity ConvertSeverity(
    EditorCompositionDiagnosticSeverity severity)
{
    switch (severity)
    {
    case EditorCompositionDiagnosticSeverity::Info:
        return EditorCustomizationDiagnosticSeverity::Info;
    case EditorCompositionDiagnosticSeverity::Warning:
        return EditorCustomizationDiagnosticSeverity::Warning;
    case EditorCompositionDiagnosticSeverity::Error:
        return EditorCustomizationDiagnosticSeverity::Error;
    case EditorCompositionDiagnosticSeverity::Blocker:
        return EditorCustomizationDiagnosticSeverity::Blocker;
    }
    return EditorCustomizationDiagnosticSeverity::Error;
}

void AppendCompositionDiagnostics(
    std::vector<EditorCustomizationDiagnostic>& output,
    const std::vector<EditorCompositionDiagnostic>& input)
{
    for (const EditorCompositionDiagnostic& diagnostic : input)
    {
        AddCustomizationDiagnostic(output,
                                   ConvertSeverity(diagnostic.severity),
                                   diagnostic.code,
                                   diagnostic.message,
                                   diagnostic.referencedId,
                                   diagnostic.documentPath);
    }
}

[[nodiscard]] Json ToJson(const EditorCustomizationOverlay& overlay)
{
    Json root;
    root["schemaVersion"] = overlay.schemaVersion;
    root["baseCompositionId"] = overlay.baseCompositionId;
    root["baseArtifactHash"] = overlay.baseArtifactHash;
    root["overlayId"] = overlay.overlayId;

    root["layoutOverrides"] = Json::array();
    for (const LayoutCustomizationOverride& value : overlay.layoutOverrides)
    {
        root["layoutOverrides"].push_back({
            { "workspaceId", value.workspaceId },
            { "panelId", value.panelId },
            { "placement", value.placement },
            { "visible", value.visible },
        });
    }

    root["shortcutOverrides"] = Json::array();
    for (const ShortcutCustomizationOverride& value :
         overlay.shortcutOverrides)
    {
        root["shortcutOverrides"].push_back({
            { "actionId", value.actionId },
            { "chord", value.chord },
        });
    }

    root["visibilityOverrides"] = Json::array();
    for (const VisibilityCustomizationOverride& value :
         overlay.visibilityOverrides)
    {
        root["visibilityOverrides"].push_back({
            { "panelId", value.panelId },
            { "visible", value.visible },
        });
    }

    root["toolbarOverrides"] = Json::array();
    for (const ToolbarCustomizationOverride& value : overlay.toolbarOverrides)
    {
        root["toolbarOverrides"].push_back({
            { "toolbarId", value.toolbarId },
            { "hiddenActionIds", value.hiddenActionIds },
        });
    }

    return root;
}

} // namespace

WorkspaceCustomizationOverlayLoadResult WorkspaceCustomizationOverlayStore::Load(
    const std::filesystem::path& path) const
{
    WorkspaceCustomizationOverlayLoadResult result;
    if (!std::filesystem::exists(path))
    {
        result.fileMissing = true;
        return result;
    }

    EditorCustomizationOverlayLoader loader;
    EditorCustomizationOverlayLoadResult loadResult = loader.LoadFile(path);
    result.overlay = std::move(loadResult.overlay);
    AppendCompositionDiagnostics(result.diagnostics, loadResult.diagnostics);
    return result;
}

WorkspaceCustomizationOverlayStoreResult WorkspaceCustomizationOverlayStore::Save(
    const std::filesystem::path& path,
    const EditorCustomizationOverlay& overlay) const
{
    WorkspaceCustomizationOverlayStoreResult result;
    std::error_code error;
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            AddCustomizationDiagnostic(result.diagnostics,
                                       EditorCustomizationDiagnosticSeverity::Error,
                                       "Customization.WriteFailed",
                                       "Could not create customization overlay directory.",
                                       {},
                                       path.parent_path().string());
            return result;
        }
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output)
    {
        AddCustomizationDiagnostic(result.diagnostics,
                                   EditorCustomizationDiagnosticSeverity::Error,
                                   "Customization.WriteFailed",
                                   "Could not open customization overlay for writing.",
                                   {},
                                   path.string());
        return result;
    }

    output << ToJson(overlay).dump(2) << '\n';
    if (!output)
    {
        AddCustomizationDiagnostic(result.diagnostics,
                                   EditorCustomizationDiagnosticSeverity::Error,
                                   "Customization.WriteFailed",
                                   "Could not write customization overlay.",
                                   {},
                                   path.string());
        return result;
    }

    result.succeeded = true;
    return result;
}

WorkspaceCustomizationOverlayStoreResult WorkspaceCustomizationOverlayStore::Reset(
    const std::filesystem::path& path) const
{
    WorkspaceCustomizationOverlayStoreResult result;
    if (!std::filesystem::exists(path))
    {
        result.succeeded = true;
        return result;
    }

    std::error_code error;
    std::filesystem::remove(path, error);
    if (error)
    {
        AddCustomizationDiagnostic(result.diagnostics,
                                   EditorCustomizationDiagnosticSeverity::Error,
                                   "Customization.WriteFailed",
                                   "Could not delete customization overlay.",
                                   {},
                                   path.string());
        return result;
    }

    result.succeeded = true;
    return result;
}

} // namespace SagaEditor
