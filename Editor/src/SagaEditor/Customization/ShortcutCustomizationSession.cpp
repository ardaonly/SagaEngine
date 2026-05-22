/// @file ShortcutCustomizationSession.cpp
/// @brief Host-owned shortcut customization session implementation.

#include "SagaEditor/Customization/ShortcutCustomizationSession.h"

#include "SagaEditor/Composition/ResolvedEditorCompositionSnapshot.h"
#include "SagaEditor/Customization/WorkspaceCustomizationOverlayPolicy.h"
#include "SagaEditor/Customization/WorkspaceCustomizationOverlayStore.h"
#include "SagaEditor/Diagnostics/EditorDiagnostic.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"

#include <utility>

namespace SagaEditor
{
namespace
{

constexpr const char* kShortcutCustomizationDiagnosticSource =
    "editor.customization.shortcuts";

[[nodiscard]] EditorDiagnosticSeverity ToEditorSeverity(
    EditorCustomizationDiagnosticSeverity severity)
{
    switch (severity)
    {
    case EditorCustomizationDiagnosticSeverity::Info:
        return EditorDiagnosticSeverity::Info;
    case EditorCustomizationDiagnosticSeverity::Warning:
        return EditorDiagnosticSeverity::Warning;
    case EditorCustomizationDiagnosticSeverity::Error:
    case EditorCustomizationDiagnosticSeverity::Blocker:
        return EditorDiagnosticSeverity::Error;
    }
    return EditorDiagnosticSeverity::Error;
}

[[nodiscard]] std::vector<EditorDiagnostic> ToEditorDiagnostics(
    const std::vector<EditorCustomizationDiagnostic>& diagnostics)
{
    std::vector<EditorDiagnostic> output;
    output.reserve(diagnostics.size());
    for (const EditorCustomizationDiagnostic& diagnostic : diagnostics)
    {
        EditorDiagnostic editorDiagnostic;
        editorDiagnostic.severity = ToEditorSeverity(diagnostic.severity);
        editorDiagnostic.code = diagnostic.code;
        editorDiagnostic.message = diagnostic.message;
        editorDiagnostic.location.resource =
            !diagnostic.path.empty() ? diagnostic.path : diagnostic.targetId;
        editorDiagnostic.publishBlocking =
            diagnostic.severity ==
            EditorCustomizationDiagnosticSeverity::Blocker;
        output.push_back(std::move(editorDiagnostic));
    }
    return output;
}

[[nodiscard]] bool HasUnsupportedShortcutOverlaySections(
    const EditorCustomizationOverlay& overlay)
{
    return !overlay.layoutOverrides.empty() ||
           !overlay.visibilityOverrides.empty() ||
           !overlay.toolbarOverrides.empty();
}

} // namespace

ShortcutCustomizationSession::ShortcutCustomizationSession() = default;
ShortcutCustomizationSession::~ShortcutCustomizationSession() = default;

bool ShortcutCustomizationSession::Init(
    ShortcutCustomizationSessionConfig config)
{
    Shutdown();
    m_workspaceRoot = std::move(config.workspaceRoot);
    m_snapshot = config.snapshot;
    m_availability = std::move(config.availability);
    m_diagnosticsService = config.diagnosticsService;
    m_overlayId = std::move(config.overlayId);

    if (!m_workspaceRoot.empty())
    {
        WorkspaceCustomizationOverlayPolicy policy;
        m_overlayPath = policy.GetDefaultOverlayPath(m_workspaceRoot);
    }

    LoadDefaultOverlay();
    RebuildController();
    RefreshDiagnosticsSnapshot();
    PublishDiagnostics();
    return true;
}

void ShortcutCustomizationSession::Shutdown()
{
    if (m_diagnosticsService != nullptr)
    {
        m_diagnosticsService->ClearSource(kShortcutCustomizationDiagnosticSource);
    }

    m_workspaceRoot.clear();
    m_overlayPath.clear();
    m_snapshot = nullptr;
    m_diagnosticsService = nullptr;
    m_availability = {};
    m_loadedOverlay.reset();
    m_controller.reset();
    m_emptyModel = {};
    m_sessionDiagnostics.clear();
    m_diagnosticsSnapshot.clear();
    m_overlayId = "user.workspace.default";
}

void ShortcutCustomizationSession::RefreshAvailability(
    EditorCustomizationAvailability availability)
{
    m_availability = std::move(availability);
    RebuildController();
    RefreshDiagnosticsSnapshot();
    PublishDiagnostics();
}

const ShortcutCustomizationModel&
ShortcutCustomizationSession::GetModel() const noexcept
{
    return m_controller != nullptr ? m_controller->GetModel() : m_emptyModel;
}

ShortcutCustomizationController*
ShortcutCustomizationSession::GetController() noexcept
{
    return m_controller.get();
}

const ShortcutCustomizationController*
ShortcutCustomizationSession::GetController() const noexcept
{
    return m_controller.get();
}

const std::filesystem::path&
ShortcutCustomizationSession::OverlayPath() const noexcept
{
    return m_overlayPath;
}

const std::vector<EditorCustomizationDiagnostic>&
ShortcutCustomizationSession::Diagnostics() const noexcept
{
    return m_diagnosticsSnapshot;
}

ShortcutCustomizationSessionResult
ShortcutCustomizationSession::SaveOverlay()
{
    ShortcutCustomizationSessionResult result;
    if (m_controller == nullptr)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Blocker,
                      "Customization.NoCompositionSnapshot",
                      "Cannot save shortcut customization without a controller.");
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    if (m_overlayPath.empty())
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.WriteFailed",
                      "Cannot save shortcut customization without a workspace root.");
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    if (!m_controller->Validate())
    {
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    WorkspaceCustomizationOverlayStore store;
    EditorCustomizationOverlay overlay = m_controller->BuildOverlayDelta();
    WorkspaceCustomizationOverlayStoreResult saveResult =
        store.Save(m_overlayPath, overlay);
    m_sessionDiagnostics.insert(m_sessionDiagnostics.end(),
                                saveResult.diagnostics.begin(),
                                saveResult.diagnostics.end());
    if (saveResult.succeeded)
    {
        m_loadedOverlay = std::move(overlay);
    }

    RefreshDiagnosticsSnapshot();
    PublishDiagnostics();
    result.succeeded = saveResult.succeeded;
    result.diagnostics = m_diagnosticsSnapshot;
    return result;
}

ShortcutCustomizationSessionResult
ShortcutCustomizationSession::ResetShortcuts()
{
    ShortcutCustomizationSessionResult result;
    if (m_overlayPath.empty())
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.WriteFailed",
                      "Cannot reset shortcut customization without a workspace root.");
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    if (m_loadedOverlay)
    {
        m_loadedOverlay->shortcutOverrides.clear();
    }

    WorkspaceCustomizationOverlayStore store;
    WorkspaceCustomizationOverlayStoreResult storeResult;
    if (m_loadedOverlay &&
        (!m_loadedOverlay->layoutOverrides.empty() ||
         !m_loadedOverlay->visibilityOverrides.empty() ||
         !m_loadedOverlay->toolbarOverrides.empty()))
    {
        storeResult = store.Save(m_overlayPath, *m_loadedOverlay);
    }
    else
    {
        storeResult = store.Reset(m_overlayPath);
        if (storeResult.succeeded)
        {
            m_loadedOverlay.reset();
        }
    }

    m_sessionDiagnostics.insert(m_sessionDiagnostics.end(),
                                storeResult.diagnostics.begin(),
                                storeResult.diagnostics.end());
    if (storeResult.succeeded)
    {
        RebuildController();
    }

    RefreshDiagnosticsSnapshot();
    PublishDiagnostics();
    result.succeeded = storeResult.succeeded;
    result.diagnostics = m_diagnosticsSnapshot;
    return result;
}

ShortcutCustomizationSessionResult
ShortcutCustomizationSession::ReloadOverlay()
{
    ShortcutCustomizationSessionResult result;
    m_sessionDiagnostics.clear();
    m_loadedOverlay.reset();
    LoadDefaultOverlay();
    RebuildController();
    RefreshDiagnosticsSnapshot();
    PublishDiagnostics();
    result.succeeded = !HasErrorCustomizationDiagnostic(m_sessionDiagnostics);
    result.diagnostics = m_diagnosticsSnapshot;
    return result;
}

ShortcutCustomizationSessionResult
ShortcutCustomizationSession::ExportOverlay(const std::filesystem::path& path)
{
    ShortcutCustomizationSessionResult result;
    if (m_controller == nullptr)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Blocker,
                      "Customization.NoCompositionSnapshot",
                      "Cannot export shortcut customization without a controller.",
                      {},
                      path.string());
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    if (path.empty())
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.WriteFailed",
                      "Cannot export shortcut customization without a path.");
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    if (!m_controller->Validate())
    {
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    WorkspaceCustomizationOverlayStore store;
    EditorCustomizationOverlay overlay = m_controller->BuildOverlayDelta();
    overlay.layoutOverrides.clear();
    overlay.visibilityOverrides.clear();
    overlay.toolbarOverrides.clear();
    WorkspaceCustomizationOverlayStoreResult saveResult =
        store.Save(path, overlay);
    m_sessionDiagnostics.insert(m_sessionDiagnostics.end(),
                                saveResult.diagnostics.begin(),
                                saveResult.diagnostics.end());

    RefreshDiagnosticsSnapshot();
    PublishDiagnostics();
    result.succeeded = saveResult.succeeded;
    result.diagnostics = m_diagnosticsSnapshot;
    return result;
}

ShortcutCustomizationSessionResult
ShortcutCustomizationSession::ImportOverlay(const std::filesystem::path& path)
{
    ShortcutCustomizationSessionResult result;
    m_sessionDiagnostics.clear();

    if (path.empty())
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.InvalidOverlay",
                      "Cannot import shortcut customization without a path.");
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    WorkspaceCustomizationOverlayStore store;
    WorkspaceCustomizationOverlayLoadResult loadResult = store.Load(path);
    m_sessionDiagnostics.insert(m_sessionDiagnostics.end(),
                                loadResult.diagnostics.begin(),
                                loadResult.diagnostics.end());

    if (loadResult.fileMissing)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.InvalidOverlay",
                      "Could not find shortcut customization overlay to import.",
                      {},
                      path.string());
    }

    if (!loadResult.overlay.has_value() ||
        HasErrorCustomizationDiagnostic(loadResult.diagnostics))
    {
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    EditorCustomizationOverlay imported = std::move(*loadResult.overlay);
    if (HasUnsupportedShortcutOverlaySections(imported))
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.InvalidOverlay",
                      "Shortcut customization import accepts only shortcut overrides.",
                      {},
                      path.string());
    }

    if (m_snapshot != nullptr &&
        !imported.baseArtifactHash.empty() &&
        imported.baseArtifactHash != m_snapshot->artifactHash)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.InvalidOverlay",
                      "Imported shortcut customization targets a different composition artifact hash.",
                      imported.baseArtifactHash,
                      path.string());
    }

    ShortcutCustomizationControllerConfig controllerConfig;
    controllerConfig.snapshot = m_snapshot;
    controllerConfig.availability = m_availability;
    controllerConfig.initialOverlay = imported;
    controllerConfig.overlayId = m_overlayId;
    ShortcutCustomizationController validationController(
        std::move(controllerConfig));
    if (!validationController.Validate())
    {
        const auto& diagnostics = validationController.GetDiagnostics();
        m_sessionDiagnostics.insert(m_sessionDiagnostics.end(),
                                    diagnostics.begin(),
                                    diagnostics.end());
    }

    if (HasErrorCustomizationDiagnostic(m_sessionDiagnostics))
    {
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    if (m_overlayPath.empty())
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.WriteFailed",
                      "Cannot import shortcut customization without a workspace root.",
                      {},
                      path.string());
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    if (m_loadedOverlay)
    {
        m_loadedOverlay->shortcutOverrides = imported.shortcutOverrides;
        imported = *m_loadedOverlay;
    }

    WorkspaceCustomizationOverlayStoreResult saveResult =
        store.Save(m_overlayPath, imported);
    m_sessionDiagnostics.insert(m_sessionDiagnostics.end(),
                                saveResult.diagnostics.begin(),
                                saveResult.diagnostics.end());
    if (saveResult.succeeded)
    {
        m_loadedOverlay = std::move(imported);
        RebuildController();
    }

    RefreshDiagnosticsSnapshot();
    PublishDiagnostics();
    result.succeeded = saveResult.succeeded;
    result.diagnostics = m_diagnosticsSnapshot;
    return result;
}

void ShortcutCustomizationSession::LoadDefaultOverlay()
{
    if (m_overlayPath.empty())
    {
        return;
    }

    WorkspaceCustomizationOverlayStore store;
    WorkspaceCustomizationOverlayLoadResult loadResult =
        store.Load(m_overlayPath);
    m_sessionDiagnostics.insert(m_sessionDiagnostics.end(),
                                loadResult.diagnostics.begin(),
                                loadResult.diagnostics.end());
    if (loadResult.overlay)
    {
        m_loadedOverlay = std::move(loadResult.overlay);
    }
}

void ShortcutCustomizationSession::RebuildController()
{
    ShortcutCustomizationControllerConfig config;
    config.snapshot = m_snapshot;
    config.availability = m_availability;
    config.initialOverlay = m_loadedOverlay;
    config.overlayId = m_overlayId;
    m_controller =
        std::make_unique<ShortcutCustomizationController>(std::move(config));
}

void ShortcutCustomizationSession::RefreshDiagnosticsSnapshot()
{
    m_diagnosticsSnapshot = m_sessionDiagnostics;
    if (m_controller != nullptr)
    {
        const auto& modelDiagnostics = m_controller->GetModel().diagnostics;
        m_diagnosticsSnapshot.insert(m_diagnosticsSnapshot.end(),
                                     modelDiagnostics.begin(),
                                     modelDiagnostics.end());
    }
}

void ShortcutCustomizationSession::PublishDiagnostics() const
{
    if (m_diagnosticsService == nullptr)
    {
        return;
    }

    m_diagnosticsService->ReplaceSource(
        kShortcutCustomizationDiagnosticSource,
        ToEditorDiagnostics(m_diagnosticsSnapshot));
}

void ShortcutCustomizationSession::AddDiagnostic(
    EditorCustomizationDiagnosticSeverity severity,
    std::string code,
    std::string message,
    std::string targetId,
    std::string path)
{
    AddCustomizationDiagnostic(m_sessionDiagnostics,
                               severity,
                               std::move(code),
                               std::move(message),
                               std::move(targetId),
                               std::move(path));
}

} // namespace SagaEditor
