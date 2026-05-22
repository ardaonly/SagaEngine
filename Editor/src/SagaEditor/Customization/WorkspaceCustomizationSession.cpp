/// @file WorkspaceCustomizationSession.cpp
/// @brief Host-owned workspace customization session implementation.

#include "SagaEditor/Customization/WorkspaceCustomizationSession.h"

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

constexpr const char* kWorkspaceCustomizationDiagnosticSource =
    "editor.customization.workspace";

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

[[nodiscard]] bool HasUnsupportedWorkspaceOverlaySections(
    const EditorCustomizationOverlay& overlay)
{
    return !overlay.shortcutOverrides.empty() ||
           !overlay.toolbarOverrides.empty();
}

} // namespace

WorkspaceCustomizationSession::WorkspaceCustomizationSession() = default;
WorkspaceCustomizationSession::~WorkspaceCustomizationSession() = default;

bool WorkspaceCustomizationSession::Init(
    WorkspaceCustomizationSessionConfig config)
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

void WorkspaceCustomizationSession::Shutdown()
{
    if (m_diagnosticsService != nullptr)
    {
        m_diagnosticsService->ClearSource(kWorkspaceCustomizationDiagnosticSource);
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

void WorkspaceCustomizationSession::RefreshAvailability(
    EditorCustomizationAvailability availability)
{
    m_availability = std::move(availability);
    RebuildController();
    RefreshDiagnosticsSnapshot();
    PublishDiagnostics();
}

const WorkspaceCustomizationModel&
WorkspaceCustomizationSession::GetModel() const noexcept
{
    return m_controller != nullptr ? m_controller->GetModel() : m_emptyModel;
}

WorkspaceCustomizationController*
WorkspaceCustomizationSession::GetController() noexcept
{
    return m_controller.get();
}

const WorkspaceCustomizationController*
WorkspaceCustomizationSession::GetController() const noexcept
{
    return m_controller.get();
}

const std::filesystem::path&
WorkspaceCustomizationSession::OverlayPath() const noexcept
{
    return m_overlayPath;
}

const std::vector<EditorCustomizationDiagnostic>&
WorkspaceCustomizationSession::Diagnostics() const noexcept
{
    return m_diagnosticsSnapshot;
}

WorkspaceCustomizationSessionResult
WorkspaceCustomizationSession::SaveOverlay()
{
    WorkspaceCustomizationSessionResult result;
    if (m_controller == nullptr)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Blocker,
                      "Customization.NoCompositionSnapshot",
                      "Cannot save workspace customization without a controller.");
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    if (m_overlayPath.empty())
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.WriteFailed",
                      "Cannot save workspace customization without a workspace root.");
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

WorkspaceCustomizationSessionResult
WorkspaceCustomizationSession::ResetOverlay()
{
    WorkspaceCustomizationSessionResult result;
    if (m_overlayPath.empty())
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.WriteFailed",
                      "Cannot reset workspace customization without a workspace root.");
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
    }

    if (m_loadedOverlay)
    {
        m_loadedOverlay->layoutOverrides.clear();
        m_loadedOverlay->visibilityOverrides.clear();
    }

    WorkspaceCustomizationOverlayStore store;
    WorkspaceCustomizationOverlayStoreResult resetResult;
    if (m_loadedOverlay &&
        (!m_loadedOverlay->shortcutOverrides.empty() ||
         !m_loadedOverlay->toolbarOverrides.empty()))
    {
        resetResult = store.Save(m_overlayPath, *m_loadedOverlay);
    }
    else
    {
        resetResult = store.Reset(m_overlayPath);
        if (resetResult.succeeded)
        {
            m_loadedOverlay.reset();
        }
    }
    m_sessionDiagnostics.insert(m_sessionDiagnostics.end(),
                                resetResult.diagnostics.begin(),
                                resetResult.diagnostics.end());
    if (resetResult.succeeded)
    {
        RebuildController();
    }

    RefreshDiagnosticsSnapshot();
    PublishDiagnostics();
    result.succeeded = resetResult.succeeded;
    result.diagnostics = m_diagnosticsSnapshot;
    return result;
}

WorkspaceCustomizationSessionResult
WorkspaceCustomizationSession::ReloadOverlay()
{
    WorkspaceCustomizationSessionResult result;
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

WorkspaceCustomizationSessionResult
WorkspaceCustomizationSession::ExportOverlay(const std::filesystem::path& path)
{
    WorkspaceCustomizationSessionResult result;
    if (m_controller == nullptr)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Blocker,
                      "Customization.NoCompositionSnapshot",
                      "Cannot export workspace customization without a controller.",
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
                      "Cannot export workspace customization without a path.");
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
    overlay.shortcutOverrides.clear();
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

WorkspaceCustomizationSessionResult
WorkspaceCustomizationSession::ImportOverlay(const std::filesystem::path& path)
{
    WorkspaceCustomizationSessionResult result;
    m_sessionDiagnostics.clear();

    if (path.empty())
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.InvalidOverlay",
                      "Cannot import workspace customization without a path.");
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
                      "Could not find workspace customization overlay to import.",
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
    if (HasUnsupportedWorkspaceOverlaySections(imported))
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.InvalidOverlay",
                      "Workspace customization import does not accept shortcut or toolbar overrides.",
                      {},
                      path.string());
    }

    if (m_snapshot != nullptr &&
        !imported.baseArtifactHash.empty() &&
        imported.baseArtifactHash != m_snapshot->artifactHash)
    {
        AddDiagnostic(EditorCustomizationDiagnosticSeverity::Error,
                      "Customization.InvalidOverlay",
                      "Imported workspace customization targets a different composition artifact hash.",
                      imported.baseArtifactHash,
                      path.string());
    }

    WorkspaceCustomizationControllerConfig controllerConfig;
    controllerConfig.snapshot = m_snapshot;
    controllerConfig.availability = m_availability;
    controllerConfig.initialOverlay = imported;
    controllerConfig.overlayId = m_overlayId;
    WorkspaceCustomizationController validationController(
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
                      "Cannot import workspace customization without a workspace root.",
                      {},
                      path.string());
        RefreshDiagnosticsSnapshot();
        PublishDiagnostics();
        result.diagnostics = m_diagnosticsSnapshot;
        return result;
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

void WorkspaceCustomizationSession::LoadDefaultOverlay()
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

void WorkspaceCustomizationSession::RebuildController()
{
    WorkspaceCustomizationControllerConfig config;
    config.snapshot = m_snapshot;
    config.availability = m_availability;
    config.initialOverlay = m_loadedOverlay;
    config.overlayId = m_overlayId;
    m_controller =
        std::make_unique<WorkspaceCustomizationController>(std::move(config));
}

void WorkspaceCustomizationSession::RefreshDiagnosticsSnapshot()
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

void WorkspaceCustomizationSession::PublishDiagnostics() const
{
    if (m_diagnosticsService == nullptr)
    {
        return;
    }

    m_diagnosticsService->ReplaceSource(
        kWorkspaceCustomizationDiagnosticSource,
        ToEditorDiagnostics(m_diagnosticsSnapshot));
}

void WorkspaceCustomizationSession::AddDiagnostic(
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
