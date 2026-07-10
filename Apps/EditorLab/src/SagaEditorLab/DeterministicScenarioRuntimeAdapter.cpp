/// @file DeterministicScenarioRuntimeAdapter.cpp
/// @brief Local deterministic scenario adapter used by the EditorLab UI.

#include "SagaEditorLab/Scenario/DeterministicScenarioRuntimeAdapter.h"

#include <utility>

namespace SagaEditorLab
{
namespace
{

void PopulateDefaultState(std::unordered_map<std::string, std::string>& state)
{
    state["editor.engine_bridge.state"] = "Ready";
    state["editor.customization.boundary"] = "editor_only";
    state["profile.layout.diff.basic_to_advanced"] = "true";
    state["profile.shortcuts.diff.basic_to_advanced"] = "true";
    state["profile.panels.diff.basic_to_advanced"] = "true";
    state["profile.layout.diff.advanced_to_standard"] = "true";
    state["profile.tools.diff.advanced_to_standard"] = "true";
    state["editor.core.identity.stable"] = "true";
    state["editor.engine_bridge.identity.stable"] = "true";
    state["editor.customization.builtin.available"] = "true";
    state["editor.customization.project.source"] = "builtin";
    state["editor.customization.user.source"] = "~/.config/Saga";
    state["editor.customization.project.profiles.loaded"] = "true";
    state["editor.customization.user.override.applied"] = "true";
    state["editor.customization.project.source.unchanged"] = "true";
}

} // namespace

DeterministicScenarioRuntimeAdapter::DeterministicScenarioRuntimeAdapter()
{
    PopulateDefaultState(m_state);
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::DispatchAction(
    const std::string& commandId,
    const std::string& argument)
{
    if (commandId.empty())
    {
        return ScenarioAdapterResult::Failure("command id is empty");
    }

    m_operations.push_back("action:" + commandId + ":" + argument);
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::OpenPanel(
    const std::string& panelId)
{
    if (panelId.empty())
    {
        return ScenarioAdapterResult::Failure("panel id is empty");
    }

    m_operations.push_back("open_panel:" + panelId);
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::ClosePanel(
    const std::string& panelId)
{
    if (panelId.empty())
    {
        return ScenarioAdapterResult::Failure("panel id is empty");
    }

    m_operations.push_back("close_panel:" + panelId);
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::LoadProject(
    const std::string& path)
{
    if (path.empty())
    {
        return ScenarioAdapterResult::Failure("project path is empty");
    }

    m_operations.push_back("load_project:" + path);
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::SetSelection(
    const std::string& entityIds)
{
    m_operations.push_back("selection:" + entityIds);
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::Undo()
{
    m_operations.push_back("undo");
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::Redo()
{
    m_operations.push_back("redo");
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::Save()
{
    m_operations.push_back("save");
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::RegisterMockExtension(
    const std::string& extensionId,
    const std::string& manifestJson)
{
    if (extensionId.empty())
    {
        return ScenarioAdapterResult::Failure("extension id is empty");
    }

    m_operations.push_back("mock_extension:" + extensionId + ":" + manifestJson);
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::AdvanceTicks(
    std::uint32_t tickCount)
{
    m_operations.push_back("wait:" + std::to_string(tickCount));
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult DeterministicScenarioRuntimeAdapter::SleepMilliseconds(
    std::uint32_t milliseconds)
{
    m_operations.push_back("sleep:" + std::to_string(milliseconds));
    return ScenarioAdapterResult::Success();
}

ScenarioSnapshotCaptureResult DeterministicScenarioRuntimeAdapter::CaptureSnapshot(
    const std::string& label)
{
    ScenarioSnapshotRef snapshot;
    snapshot.label = label;
    snapshot.sha256 = {};
    m_operations.push_back("snapshot:" + label);
    return ScenarioSnapshotCaptureResult::Captured(std::move(snapshot));
}

ScenarioStateReadResult DeterministicScenarioRuntimeAdapter::ReadState(
    const std::string& statePath)
{
    m_operations.push_back("read:" + statePath);

    const auto found = m_state.find(statePath);
    if (found == m_state.end())
    {
        return ScenarioStateReadResult::Missing("missing deterministic state path");
    }

    return ScenarioStateReadResult::Value(found->second);
}

void DeterministicScenarioRuntimeAdapter::SetState(std::string statePath,
                                                   std::string value)
{
    m_state[std::move(statePath)] = std::move(value);
}

const std::vector<std::string>&
DeterministicScenarioRuntimeAdapter::GetOperations() const noexcept
{
    return m_operations;
}

} // namespace SagaEditorLab
