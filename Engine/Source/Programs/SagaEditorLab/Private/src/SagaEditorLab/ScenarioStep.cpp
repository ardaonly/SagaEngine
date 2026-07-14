/// @file ScenarioStep.cpp
/// @brief Identity strings + builders for ScenarioStep.

#include "SagaEditorLab/Scenario/ScenarioStep.h"

#include <utility>

namespace SagaEditorLab
{

// ─── Identity ─────────────────────────────────────────────────────────────────

const char* ScenarioStepKindId(ScenarioStepKind kind) noexcept
{
    switch (kind)
    {
        case ScenarioStepKind::Action:                return "saga.lab.step.action";
        case ScenarioStepKind::Assertion:             return "saga.lab.step.assertion";
        case ScenarioStepKind::Wait:                  return "saga.lab.step.wait";
        case ScenarioStepKind::Sleep:                 return "saga.lab.step.sleep";
        case ScenarioStepKind::Snapshot:              return "saga.lab.step.snapshot";
        case ScenarioStepKind::LoadProject:           return "saga.lab.step.load_project";
        case ScenarioStepKind::SetSelection:          return "saga.lab.step.set_selection";
        case ScenarioStepKind::Undo:                  return "saga.lab.step.undo";
        case ScenarioStepKind::Redo:                  return "saga.lab.step.redo";
        case ScenarioStepKind::Save:                  return "saga.lab.step.save";
        case ScenarioStepKind::OpenPanel:             return "saga.lab.step.open_panel";
        case ScenarioStepKind::ClosePanel:            return "saga.lab.step.close_panel";
        case ScenarioStepKind::RegisterMockExtension: return "saga.lab.step.register_mock_extension";
    }
    return "saga.lab.step.unknown";
}

ScenarioStepKind ScenarioStepKindFromId(const std::string& id) noexcept
{
    if (id == "saga.lab.step.action")                  return ScenarioStepKind::Action;
    if (id == "saga.lab.step.assertion")               return ScenarioStepKind::Assertion;
    if (id == "saga.lab.step.wait")                    return ScenarioStepKind::Wait;
    if (id == "saga.lab.step.sleep")                   return ScenarioStepKind::Sleep;
    if (id == "saga.lab.step.snapshot")                return ScenarioStepKind::Snapshot;
    if (id == "saga.lab.step.load_project")            return ScenarioStepKind::LoadProject;
    if (id == "saga.lab.step.set_selection")           return ScenarioStepKind::SetSelection;
    if (id == "saga.lab.step.undo")                    return ScenarioStepKind::Undo;
    if (id == "saga.lab.step.redo")                    return ScenarioStepKind::Redo;
    if (id == "saga.lab.step.save")                    return ScenarioStepKind::Save;
    if (id == "saga.lab.step.open_panel")              return ScenarioStepKind::OpenPanel;
    if (id == "saga.lab.step.close_panel")             return ScenarioStepKind::ClosePanel;
    if (id == "saga.lab.step.register_mock_extension") return ScenarioStepKind::RegisterMockExtension;
    return ScenarioStepKind::Action;
}

// ─── Builders ─────────────────────────────────────────────────────────────────

ScenarioStep ScenarioStep::MakeAction(std::string commandId, std::string argument)
{
    ScenarioStep s;
    s.kind             = ScenarioStepKind::Action;
    s.action.commandId = std::move(commandId);
    s.action.argument  = std::move(argument);
    return s;
}

ScenarioStep ScenarioStep::MakeAssertion(std::string statePath, std::string expectedValue)
{
    ScenarioStep s;
    s.kind                       = ScenarioStepKind::Assertion;
    s.assertion.statePath        = std::move(statePath);
    s.assertion.expectedValue    = std::move(expectedValue);
    return s;
}

ScenarioStep ScenarioStep::MakeWait(std::uint32_t tickCount) noexcept
{
    ScenarioStep s;
    s.kind           = ScenarioStepKind::Wait;
    s.wait.tickCount = tickCount;
    return s;
}

ScenarioStep ScenarioStep::MakeSleep(std::uint32_t milliseconds) noexcept
{
    ScenarioStep s;
    s.kind                = ScenarioStepKind::Sleep;
    s.sleep.milliseconds  = milliseconds;
    return s;
}

ScenarioStep ScenarioStep::MakeSnapshot(std::string label)
{
    ScenarioStep s;
    s.kind           = ScenarioStepKind::Snapshot;
    s.snapshot.label = std::move(label);
    return s;
}

ScenarioStep ScenarioStep::MakeLoadProject(std::string path)
{
    ScenarioStep s;
    s.kind                = ScenarioStepKind::LoadProject;
    s.loadProject.path    = std::move(path);
    return s;
}

ScenarioStep ScenarioStep::MakeSetSelection(std::string entityIds)
{
    ScenarioStep s;
    s.kind                  = ScenarioStepKind::SetSelection;
    s.selection.entityIds   = std::move(entityIds);
    return s;
}

ScenarioStep ScenarioStep::MakeUndo() noexcept
{
    ScenarioStep s;
    s.kind = ScenarioStepKind::Undo;
    return s;
}

ScenarioStep ScenarioStep::MakeRedo() noexcept
{
    ScenarioStep s;
    s.kind = ScenarioStepKind::Redo;
    return s;
}

ScenarioStep ScenarioStep::MakeSave() noexcept
{
    ScenarioStep s;
    s.kind = ScenarioStepKind::Save;
    return s;
}

ScenarioStep ScenarioStep::MakeOpenPanel(std::string panelId)
{
    ScenarioStep s;
    s.kind          = ScenarioStepKind::OpenPanel;
    s.panel.panelId = std::move(panelId);
    return s;
}

ScenarioStep ScenarioStep::MakeClosePanel(std::string panelId)
{
    ScenarioStep s;
    s.kind          = ScenarioStepKind::ClosePanel;
    s.panel.panelId = std::move(panelId);
    return s;
}

ScenarioStep ScenarioStep::MakeRegisterMockExtension(std::string extensionId,
                                                      std::string manifestJson)
{
    ScenarioStep s;
    s.kind                          = ScenarioStepKind::RegisterMockExtension;
    s.mockExtension.extensionId     = std::move(extensionId);
    s.mockExtension.manifestJson    = std::move(manifestJson);
    return s;
}

} // namespace SagaEditorLab
