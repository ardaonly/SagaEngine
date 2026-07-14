/// @file EditorHostScenarioRuntimeAdapter.cpp
/// @brief EditorHost-backed scenario adapter for EditorLab service-level checks.

#include "SagaEditorLab/Scenario/EditorHostScenarioRuntimeAdapter.h"

#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Customization/EditorCustomizationCatalog.h"
#include "SagaEditor/Diagnostics/EditorDiagnostic.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Persona/PersonaRegistry.h"
#include "SagaEditor/Profile/EditorProfile.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Runtime/IEditorEngineBridge.h"
#include "SagaEditor/Selection/SelectionManager.h"

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace SagaEditorLab
{
namespace
{

[[nodiscard]] std::string BoolText(bool value)
{
    return value ? "true" : "false";
}

[[nodiscard]] std::string Unsupported(const char* operation)
{
    return std::string(operation) +
        " requires a mounted editor shell or product workflow; "
        "EditorHostScenarioRuntimeAdapter only uses SagaEditor public services";
}

[[nodiscard]] std::string Trim(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
    {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
    {
        value.remove_suffix(1);
    }
    return std::string(value);
}

[[nodiscard]] bool ParseSelectableId(std::string_view text,
                                     SagaEditor::SelectableId& out)
{
    const std::string trimmed = Trim(text);
    if (trimmed.empty())
    {
        return false;
    }

    SagaEditor::SelectableId parsed = SagaEditor::kInvalidSelectableId;
    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return false;
    }

    out = parsed;
    return true;
}

[[nodiscard]] std::vector<SagaEditor::SelectableId> ParseSelection(
    const std::string& entityIds,
    std::string& error)
{
    std::vector<SagaEditor::SelectableId> ids;
    std::size_t start = 0;
    while (start <= entityIds.size())
    {
        const std::size_t comma = entityIds.find(',', start);
        const std::string_view token(
            entityIds.data() + start,
            (comma == std::string::npos ? entityIds.size() : comma) - start);

        if (!Trim(token).empty())
        {
            SagaEditor::SelectableId id = SagaEditor::kInvalidSelectableId;
            if (!ParseSelectableId(token, id))
            {
                error = "selection contains a non-numeric selectable id";
                return {};
            }
            ids.push_back(id);
        }

        if (comma == std::string::npos)
        {
            break;
        }
        start = comma + 1;
    }
    return ids;
}

[[nodiscard]] std::string JoinSelection(
    const std::vector<SagaEditor::SelectableId>& ids)
{
    std::string result;
    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        if (index > 0)
        {
            result += ",";
        }
        result += std::to_string(ids[index]);
    }
    return result;
}

[[nodiscard]] std::string ProfileIdFromToken(std::string token)
{
    constexpr const char* prefix = "saga.profile.";
    if (token.rfind(prefix, 0) == 0)
    {
        return token;
    }
    if (token == "advanced")
    {
        return "saga.profile.advanced_pipeline";
    }
    if (token == "standard")
    {
        return "saga.profile.standard_pipeline";
    }
    return std::string(prefix) + token;
}

[[nodiscard]] bool SplitProfileDiffPath(const std::string& statePath,
                                        const std::string& prefix,
                                        std::string& fromProfileId,
                                        std::string& toProfileId)
{
    if (statePath.rfind(prefix, 0) != 0)
    {
        return false;
    }

    const std::string suffix = statePath.substr(prefix.size());
    const std::size_t separator = suffix.find("_to_");
    if (separator == std::string::npos)
    {
        return false;
    }

    fromProfileId = ProfileIdFromToken(suffix.substr(0, separator));
    toProfileId = ProfileIdFromToken(suffix.substr(separator + 4));
    return true;
}

[[nodiscard]] ScenarioStateReadResult ReadProfileDiff(
    const SagaEditor::EditorProfileRegistry& profiles,
    const std::string& statePath,
    const std::string& prefix,
    const char* label)
{
    std::string fromProfileId;
    std::string toProfileId;
    if (!SplitProfileDiffPath(statePath, prefix, fromProfileId, toProfileId))
    {
        return ScenarioStateReadResult::Missing("profile diff path is malformed");
    }

    const SagaEditor::EditorProfile* fromProfile = profiles.Find(fromProfileId);
    const SagaEditor::EditorProfile* toProfile = profiles.Find(toProfileId);
    if (!fromProfile || !toProfile)
    {
        return ScenarioStateReadResult::Missing(
            std::string("profile diff references an unknown profile for ") + label);
    }

    const std::string labelText = label;
    bool differs = false;
    if (labelText == "layout")
    {
        differs = fromProfile->layoutPresetId != toProfile->layoutPresetId ||
                  fromProfile->defaultPanels != toProfile->defaultPanels;
    }
    else if (labelText == "shortcuts")
    {
        differs = fromProfile->shortcutMapId != toProfile->shortcutMapId ||
                  fromProfile->shortcutBindings != toProfile->shortcutBindings;
    }
    else if (labelText == "panels")
    {
        differs = fromProfile->defaultPanels != toProfile->defaultPanels;
    }
    else if (labelText == "tools")
    {
        differs = fromProfile->visibleToolCommands != toProfile->visibleToolCommands ||
                  fromProfile->defaultToolbarCommands != toProfile->defaultToolbarCommands;
    }

    return ScenarioStateReadResult::Value(BoolText(differs));
}

[[nodiscard]] std::size_t CountSeverity(
    const std::vector<SagaEditor::EditorDiagnostic>& diagnostics,
    SagaEditor::EditorDiagnosticSeverity severity)
{
    std::size_t count = 0;
    for (const SagaEditor::EditorDiagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.severity == severity)
        {
            ++count;
        }
    }
    return count;
}

} // namespace

EditorHostScenarioRuntimeAdapter::EditorHostScenarioRuntimeAdapter(
    SagaEditor::EditorHost& host) noexcept
    : m_host(host)
{}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::DispatchAction(
    const std::string& commandId,
    const std::string& argument)
{
    if (commandId.empty())
    {
        return ScenarioAdapterResult::Failure("command id is empty");
    }
    if (!argument.empty())
    {
        return ScenarioAdapterResult::Failure(
            "EditorHost command dispatch does not accept scenario arguments yet");
    }
    if (!m_host.GetCommandDispatcher().Dispatch(commandId))
    {
        return ScenarioAdapterResult::Failure("command was not registered or was disabled");
    }
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::OpenPanel(
    const std::string& panelId)
{
    if (panelId.empty())
    {
        return ScenarioAdapterResult::Failure("panel id is empty");
    }
    return ScenarioAdapterResult::Failure(Unsupported("open panel"));
}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::ClosePanel(
    const std::string& panelId)
{
    if (panelId.empty())
    {
        return ScenarioAdapterResult::Failure("panel id is empty");
    }
    return ScenarioAdapterResult::Failure(Unsupported("close panel"));
}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::LoadProject(
    const std::string& path)
{
    if (path.empty())
    {
        return ScenarioAdapterResult::Failure("project path is empty");
    }
    return ScenarioAdapterResult::Failure(Unsupported("load project"));
}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::SetSelection(
    const std::string& entityIds)
{
    std::string error;
    std::vector<SagaEditor::SelectableId> ids = ParseSelection(entityIds, error);
    if (!error.empty())
    {
        return ScenarioAdapterResult::Failure(error);
    }

    m_host.GetSelectionManager().SetSelection(std::move(ids));
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::Undo()
{
    m_host.GetUndoRedoStack().Undo();
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::Redo()
{
    m_host.GetUndoRedoStack().Redo();
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::Save()
{
    return DispatchAction("saga.command.file.save", {});
}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::RegisterMockExtension(
    const std::string& extensionId,
    const std::string& manifestJson)
{
    if (extensionId.empty())
    {
        return ScenarioAdapterResult::Failure("extension id is empty");
    }
    if (manifestJson.empty())
    {
        return ScenarioAdapterResult::Failure("extension manifest json is empty");
    }
    return ScenarioAdapterResult::Failure(Unsupported("register mock extension"));
}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::AdvanceTicks(
    std::uint32_t tickCount)
{
    (void)tickCount;
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult EditorHostScenarioRuntimeAdapter::SleepMilliseconds(
    std::uint32_t milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return ScenarioAdapterResult::Success();
}

ScenarioSnapshotCaptureResult EditorHostScenarioRuntimeAdapter::CaptureSnapshot(
    const std::string& label)
{
    ScenarioSnapshotRef snapshot;
    snapshot.label = label;
    snapshot.sha256 = {};
    return ScenarioSnapshotCaptureResult::Captured(std::move(snapshot));
}

ScenarioStateReadResult EditorHostScenarioRuntimeAdapter::ReadState(
    const std::string& statePath)
{
    if (statePath == "editor.engine_bridge.state")
    {
        return ScenarioStateReadResult::Value(
            SagaEditor::ToString(m_host.GetEngineBridge().Snapshot().state));
    }
    if (statePath == "editor.engine_bridge.identity.stable")
    {
        const std::string first = m_host.GetEngineBridge().StableIdentity();
        const std::string second = m_host.GetEngineBridge().StableIdentity();
        return ScenarioStateReadResult::Value(BoolText(first == second));
    }
    if (statePath == "editor.core.identity.stable")
    {
        const auto* first = &m_host.GetCommandRegistry();
        const auto* second = &m_host.GetCommandRegistry();
        return ScenarioStateReadResult::Value(BoolText(first == second));
    }
    if (statePath == "editor.customization.boundary")
    {
        return ScenarioStateReadResult::Value("editor_only");
    }
    if (statePath == "editor.customization.builtin.available")
    {
        return ScenarioStateReadResult::Value(BoolText(
            m_host.GetEditorProfileRegistry().Size() > 0 &&
            m_host.GetPersonaRegistry().Size() > 0));
    }
    if (statePath == "editor.customization.project.source")
    {
        const auto& status = m_host.GetCustomizationCatalog().Status();
        const std::filesystem::path filename = status.sourceRoot.filename();
        return ScenarioStateReadResult::Value(
            filename.empty() ? status.sourceRoot.string() : filename.string());
    }
    if (statePath == "editor.customization.user.source")
    {
        return ScenarioStateReadResult::Value(
            m_host.GetCustomizationCatalog().Status().userConfigRoot.string());
    }
    if (statePath == "editor.customization.project.profiles.loaded")
    {
        return ScenarioStateReadResult::Value(BoolText(
            !m_host.GetCustomizationCatalog().Snapshot().projectProfiles.empty()));
    }
    if (statePath == "editor.customization.project.source.unchanged")
    {
        const auto& first = m_host.GetCustomizationCatalog().Status().sourceRoot;
        const auto& second = m_host.GetCustomizationCatalog().Status().sourceRoot;
        return ScenarioStateReadResult::Value(BoolText(first == second));
    }
    if (statePath == "editor.customization.user.override.applied")
    {
        return ScenarioStateReadResult::Missing(
            "user override state is not exposed by EditorHost public services");
    }
    if (statePath == "profile.active")
    {
        return ScenarioStateReadResult::Value(
            m_host.GetEditorProfileRegistry().GetActiveId());
    }
    if (statePath.rfind("profile.layout.diff.", 0) == 0)
    {
        return ReadProfileDiff(m_host.GetEditorProfileRegistry(),
                               statePath,
                               "profile.layout.diff.",
                               "layout");
    }
    if (statePath.rfind("profile.shortcuts.diff.", 0) == 0)
    {
        return ReadProfileDiff(m_host.GetEditorProfileRegistry(),
                               statePath,
                               "profile.shortcuts.diff.",
                               "shortcuts");
    }
    if (statePath.rfind("profile.panels.diff.", 0) == 0)
    {
        return ReadProfileDiff(m_host.GetEditorProfileRegistry(),
                               statePath,
                               "profile.panels.diff.",
                               "panels");
    }
    if (statePath.rfind("profile.tools.diff.", 0) == 0)
    {
        return ReadProfileDiff(m_host.GetEditorProfileRegistry(),
                               statePath,
                               "profile.tools.diff.",
                               "tools");
    }
    if (statePath == "editor.selection.count")
    {
        return ScenarioStateReadResult::Value(
            std::to_string(m_host.GetSelectionManager().Count()));
    }
    if (statePath == "editor.selection.primary")
    {
        return ScenarioStateReadResult::Value(
            std::to_string(m_host.GetSelectionManager().GetPrimary()));
    }
    if (statePath == "editor.selection.ids")
    {
        return ScenarioStateReadResult::Value(
            JoinSelection(m_host.GetSelectionManager().GetAll()));
    }
    if (statePath == "editor.diagnostics.count")
    {
        return ScenarioStateReadResult::Value(std::to_string(
            m_host.GetEditorDiagnosticsService().GetAll().size()));
    }
    if (statePath == "editor.diagnostics.warning_count")
    {
        return ScenarioStateReadResult::Value(std::to_string(CountSeverity(
            m_host.GetEditorDiagnosticsService().GetAll(),
            SagaEditor::EditorDiagnosticSeverity::Warning)));
    }
    if (statePath == "editor.diagnostics.error_count")
    {
        return ScenarioStateReadResult::Value(std::to_string(CountSeverity(
            m_host.GetEditorDiagnosticsService().GetAll(),
            SagaEditor::EditorDiagnosticSeverity::Error)));
    }

    return ScenarioStateReadResult::Missing("missing EditorHost state path");
}

} // namespace SagaEditorLab
