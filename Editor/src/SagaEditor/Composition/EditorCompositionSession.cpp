/// @file EditorCompositionSession.cpp
/// @brief EditorCompositionSession implementation.

#include "SagaEditor/Composition/EditorCompositionSession.h"

#include "SagaEditor/Composition/EditorCompositionArtifactLoader.h"
#include "SagaEditor/Composition/EditorCompositionResolver.h"
#include "SagaEditor/Customization/EditorCustomizationOverlayLoader.h"

#include <utility>

namespace SagaEditor
{
namespace
{

void AddSessionDiagnostic(std::vector<EditorCompositionDiagnostic>& diagnostics,
                          EditorCompositionDiagnosticSeverity severity,
                          std::string code,
                          std::string message,
                          const std::filesystem::path& documentPath = {},
                          std::string jsonPointer = {})
{
    diagnostics.push_back({ severity,
                            std::move(code),
                            std::move(message),
                            documentPath.string(),
                            std::move(jsonPointer),
                            {} });
}

} // namespace

bool EditorCompositionSession::Init(const EditorCompositionStartupConfig& config)
{
    Shutdown();
    m_config = config;

    if (m_config.manifestPath.empty())
    {
        m_status = EditorCompositionSessionStatus::Unconfigured;
        if (!m_config.overlayPaths.empty() || m_config.requireValidComposition)
        {
            AddSessionDiagnostic(m_diagnostics,
                                 EditorCompositionDiagnosticSeverity::Blocker,
                                 "EditorCompositionStartup.ManifestRequired",
                                 "Editor composition startup requires a compiled manifest path.",
                                 m_config.manifestPath,
                                 "/manifestPath");
            m_status = EditorCompositionSessionStatus::Failed;
        }
        return !m_config.requireValidComposition;
    }

    EditorCompositionArtifactLoader artifactLoader;
    EditorCompositionLoadResult loadResult =
        artifactLoader.LoadFromManifestFile(m_config.manifestPath);
    m_diagnostics.insert(m_diagnostics.end(),
                         loadResult.diagnostics.begin(),
                         loadResult.diagnostics.end());

    std::vector<EditorCustomizationOverlay> overlays;
    EditorCustomizationOverlayLoader overlayLoader;
    for (const std::filesystem::path& overlayPath : m_config.overlayPaths)
    {
        EditorCustomizationOverlayLoadResult overlayResult =
            overlayLoader.LoadFile(overlayPath);
        m_diagnostics.insert(m_diagnostics.end(),
                             overlayResult.diagnostics.begin(),
                             overlayResult.diagnostics.end());
        if (overlayResult.overlay)
        {
            overlays.push_back(std::move(*overlayResult.overlay));
        }
    }

    if (!loadResult.manifest || !loadResult.artifact)
    {
        m_status = EditorCompositionSessionStatus::Failed;
        return !m_config.requireValidComposition;
    }

    EditorCompositionResolver resolver;
    m_snapshot = resolver.Resolve(*loadResult.manifest,
                                  *loadResult.artifact,
                                  overlays);
    m_diagnostics.insert(m_diagnostics.end(),
                         m_snapshot->diagnostics.begin(),
                         m_snapshot->diagnostics.end());
    m_status = m_snapshot->isUsable &&
                   !HasErrorCompositionDiagnostic(m_diagnostics)
               ? EditorCompositionSessionStatus::Loaded
               : EditorCompositionSessionStatus::Failed;

    return !m_config.requireValidComposition || IsUsable();
}

void EditorCompositionSession::Shutdown()
{
    m_config = {};
    m_status = EditorCompositionSessionStatus::Unconfigured;
    m_snapshot.reset();
    m_diagnostics.clear();
}

const EditorCompositionStartupConfig&
EditorCompositionSession::Config() const noexcept
{
    return m_config;
}

EditorCompositionSessionStatus EditorCompositionSession::Status() const noexcept
{
    return m_status;
}

bool EditorCompositionSession::IsUsable() const noexcept
{
    return m_status == EditorCompositionSessionStatus::Loaded &&
           m_snapshot.has_value() &&
           m_snapshot->isUsable;
}

const std::optional<ResolvedEditorCompositionSnapshot>&
EditorCompositionSession::Snapshot() const noexcept
{
    return m_snapshot;
}

const std::vector<EditorCompositionDiagnostic>&
EditorCompositionSession::Diagnostics() const noexcept
{
    return m_diagnostics;
}

} // namespace SagaEditor
