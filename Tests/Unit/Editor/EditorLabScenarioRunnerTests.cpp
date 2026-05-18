/// @file EditorLabScenarioRunnerTests.cpp
/// @brief Unit coverage for the headless EditorLab scenario runner.

#include "SagaEditorLab/Scenario/CustomizationScenario.h"
#include "SagaEditorLab/Scenario/BuiltinScenarioDefinitions.h"
#include "SagaEditorLab/Scenario/DeterministicScenarioRuntimeAdapter.h"
#include "SagaEditorLab/Scenario/EditorHostScenarioRuntimeAdapter.h"
#include "SagaEditorLab/Scenario/EditorShellScenarioRuntimeAdapter.h"
#include "SagaEditorLab/Scenario/ProfileSwitchScenario.h"
#include "SagaEditorLab/Scenario/ScenarioDefinition.h"
#include "SagaEditorLab/Scenario/ScenarioRunner.h"
#include "SagaEditorLab/UI/ScenarioRunnerDockPanel.h"
#include "SagaEditorLab/UI/ScenarioRunnerPanelViewModel.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Diagnostics/EditorDiagnostic.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Panels/IPanel.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

using namespace SagaEditorLab;

class FakeMainWindow final : public SagaEditor::IUIMainWindow
{
public:
    void Show() override { shown = true; }
    void ShowMaximized() override
    {
        maximized = true;
        shown = true;
    }
    void Hide() override { shown = false; }
    void Close() override
    {
        closed = true;
        if (onClose)
        {
            onClose();
        }
    }

    void SetTitle(const std::string& value) override { title = value; }
    void SetSize(int w, int h) override
    {
        width = w;
        height = h;
    }

    [[nodiscard]] int GetWidth() const noexcept override { return width; }
    [[nodiscard]] int GetHeight() const noexcept override { return height; }
    [[nodiscard]] void* GetNativeHandle() const noexcept override { return nullptr; }

    void ApplyShellLayout(const SagaEditor::ShellLayout& value) override
    {
        layout = value;
    }

    void SetStatusMessage(const std::string& value) override { status = value; }

    void DockPanel(void*,
                   const std::string& panelId,
                   const std::string&,
                   SagaEditor::UIDockArea) override
    {
        visiblePanels[panelId] = true;
    }

    void UndockPanel(const std::string& panelId) override
    {
        visiblePanels.erase(panelId);
    }

    void FocusPanel(const std::string& panelId) override
    {
        focusedPanels.push_back(panelId);
    }

    void SetPanelVisible(const std::string& panelId, bool visible) override
    {
        visiblePanels[panelId] = visible;
    }

    [[nodiscard]] std::vector<uint8_t> SaveState() const override { return {}; }
    [[nodiscard]] bool RestoreState(const std::vector<uint8_t>&) override { return true; }

    void SetOnClose(CloseCallback cb) override { onClose = std::move(cb); }
    void SetOnCommand(CommandCallback cb) override { onCommand = std::move(cb); }

    int width = 1600;
    int height = 900;
    bool shown = false;
    bool maximized = false;
    bool closed = false;
    std::string title;
    std::string status;
    SagaEditor::ShellLayout layout;
    std::unordered_map<std::string, bool> visiblePanels;
    std::vector<std::string> focusedPanels;
    CloseCallback onClose;
    CommandCallback onCommand;
};

class FakePanel final : public SagaEditor::IPanel
{
public:
    FakePanel(std::string panelId, std::string panelTitle)
        : id(std::move(panelId))
        , title(std::move(panelTitle))
    {}

    [[nodiscard]] SagaEditor::PanelId GetPanelId() const override { return id; }
    [[nodiscard]] std::string GetTitle() const override { return title; }
    [[nodiscard]] void* GetNativeWidget() const noexcept override { return nullptr; }

private:
    std::string id;
    std::string title;
};

class FakeScenarioRuntimeAdapter final : public IScenarioRuntimeAdapter
{
public:
    ScenarioAdapterResult DispatchAction(const std::string& commandId,
                                         const std::string& argument) override
    {
        operations.push_back("action:" + commandId + ":" + argument);
        const auto found = actionFailures.find(commandId);
        if (found != actionFailures.end())
        {
            return ScenarioAdapterResult::Failure(found->second);
        }
        return ScenarioAdapterResult::Success();
    }

    ScenarioAdapterResult OpenPanel(const std::string& panelId) override
    {
        operations.push_back("open_panel:" + panelId);
        return ScenarioAdapterResult::Success();
    }

    ScenarioAdapterResult ClosePanel(const std::string& panelId) override
    {
        operations.push_back("close_panel:" + panelId);
        return ScenarioAdapterResult::Success();
    }

    ScenarioAdapterResult LoadProject(const std::string& path) override
    {
        operations.push_back("load_project:" + path);
        return ScenarioAdapterResult::Success();
    }

    ScenarioAdapterResult SetSelection(const std::string& entityIds) override
    {
        operations.push_back("selection:" + entityIds);
        return ScenarioAdapterResult::Success();
    }

    ScenarioAdapterResult Undo() override
    {
        operations.push_back("undo");
        return ScenarioAdapterResult::Success();
    }

    ScenarioAdapterResult Redo() override
    {
        operations.push_back("redo");
        return ScenarioAdapterResult::Success();
    }

    ScenarioAdapterResult Save() override
    {
        operations.push_back("save");
        return ScenarioAdapterResult::Success();
    }

    ScenarioAdapterResult RegisterMockExtension(const std::string& extensionId,
                                                const std::string& manifestJson) override
    {
        operations.push_back("mock_extension:" + extensionId + ":" + manifestJson);
        return ScenarioAdapterResult::Success();
    }

    ScenarioAdapterResult AdvanceTicks(std::uint32_t tickCount) override
    {
        operations.push_back("wait:" + std::to_string(tickCount));
        return ScenarioAdapterResult::Success();
    }

    ScenarioAdapterResult SleepMilliseconds(std::uint32_t milliseconds) override
    {
        operations.push_back("sleep:" + std::to_string(milliseconds));
        return ScenarioAdapterResult::Success();
    }

    ScenarioSnapshotCaptureResult CaptureSnapshot(const std::string& label) override
    {
        operations.push_back("snapshot:" + label);

        ScenarioSnapshotRef snapshot;
        snapshot.label = label;
        snapshot.sha256 = "sha-" + label;
        return ScenarioSnapshotCaptureResult::Captured(std::move(snapshot));
    }

    ScenarioStateReadResult ReadState(const std::string& statePath) override
    {
        operations.push_back("read:" + statePath);

        const auto failure = stateFailures.find(statePath);
        if (failure != stateFailures.end())
        {
            return ScenarioStateReadResult::Failure(failure->second);
        }

        const auto found = state.find(statePath);
        if (found == state.end())
        {
            return ScenarioStateReadResult::Missing("missing state path");
        }

        return ScenarioStateReadResult::Value(found->second);
    }

    std::vector<std::string> operations;
    std::unordered_map<std::string, std::string> state;
    std::unordered_map<std::string, std::string> stateFailures;
    std::unordered_map<std::string, std::string> actionFailures;
};

[[nodiscard]] ScenarioDefinition MakeScenario(std::vector<ScenarioStep> steps)
{
    ScenarioDefinition scenario;
    scenario.id = "saga.lab.test";
    scenario.name = "EditorLab test";
    scenario.steps = std::move(steps);
    return scenario;
}

void PopulateBuiltinScenarioState(FakeScenarioRuntimeAdapter& adapter)
{
    adapter.state["editor.engine_bridge.state"] = "Ready";
    adapter.state["editor.customization.boundary"] = "editor_only";
    adapter.state["profile.layout.diff.basic_to_advanced"] = "true";
    adapter.state["profile.shortcuts.diff.basic_to_advanced"] = "true";
    adapter.state["profile.panels.diff.basic_to_advanced"] = "true";
    adapter.state["profile.layout.diff.advanced_to_standard"] = "true";
    adapter.state["profile.tools.diff.advanced_to_standard"] = "true";
    adapter.state["editor.core.identity.stable"] = "true";
    adapter.state["editor.engine_bridge.identity.stable"] = "true";
    adapter.state["editor.customization.builtin.available"] = "true";
    adapter.state["editor.customization.project.source"] = ".sde";
    adapter.state["editor.customization.user.source"] = "~/.config/Saga";
    adapter.state["editor.customization.project.profiles.loaded"] = "true";
    adapter.state["editor.customization.user.override.applied"] = "true";
    adapter.state["editor.customization.project.source.unchanged"] = "true";
}

} // namespace

TEST(EditorLabScenarioRunnerTests, ValidScenarioExecutesActionsAssertionsAndSnapshots)
{
    FakeScenarioRuntimeAdapter adapter;
    adapter.state["editor.ready"] = "true";

    const ScenarioDefinition scenario = MakeScenario({
        ScenarioStep::MakeAction("saga.command.profile.basic", "fast"),
        ScenarioStep::MakeAssertion("editor.ready", "true"),
        ScenarioStep::MakeSnapshot("after-basic"),
    });

    const ScenarioResult result = ScenarioRunner{}.Run(scenario, adapter);

    EXPECT_TRUE(result.Ok());
    EXPECT_EQ(result.verdict, ScenarioVerdict::Pass);
    EXPECT_EQ(result.stepsTotal, 3u);
    EXPECT_EQ(result.stepsExecuted, 3u);
    ASSERT_EQ(result.snapshots.size(), 1u);
    EXPECT_EQ(result.snapshots[0].label, "after-basic");
    EXPECT_EQ(result.snapshots[0].sha256, "sha-after-basic");
    EXPECT_EQ(result.snapshots[0].stepIndex, 2);
    EXPECT_EQ(adapter.operations[0], "action:saga.command.profile.basic:fast");
    EXPECT_EQ(adapter.operations[1], "read:editor.ready");
    EXPECT_EQ(adapter.operations[2], "snapshot:after-basic");
}

TEST(EditorLabScenarioRunnerTests, FailedAssertionProducesDeterministicDiagnostic)
{
    FakeScenarioRuntimeAdapter adapter;
    adapter.state["editor.ready"] = "false";

    ScenarioStep assertion = ScenarioStep::MakeAssertion("editor.ready", "true");
    assertion.label = "ready check";

    const ScenarioResult result = ScenarioRunner{}.Run(MakeScenario({assertion}), adapter);

    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.verdict, ScenarioVerdict::Fail);
    EXPECT_EQ(result.stepsExecuted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].stepIndex, 0);
    EXPECT_EQ(result.diagnostics[0].stepLabel, "ready check");
    EXPECT_EQ(result.diagnostics[0].message,
              "Assertion failed for 'editor.ready': expected 'true' but got 'false'");
}

TEST(EditorLabScenarioRunnerTests, StopsOnFirstFailureByDefault)
{
    FakeScenarioRuntimeAdapter adapter;
    adapter.actionFailures["saga.command.fail"] = "command missing";

    const ScenarioDefinition scenario = MakeScenario({
        ScenarioStep::MakeAction("saga.command.fail"),
        ScenarioStep::MakeSave(),
    });

    const ScenarioResult result = ScenarioRunner{}.Run(scenario, adapter);

    EXPECT_EQ(result.stepsExecuted, 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].message,
              "Step failed: saga.lab.step.action: command missing");
    ASSERT_EQ(adapter.operations.size(), 1u);
    EXPECT_EQ(adapter.operations[0], "action:saga.command.fail:");
}

TEST(EditorLabScenarioRunnerTests, ContinueAfterFailureAccumulatesDiagnostics)
{
    FakeScenarioRuntimeAdapter adapter;
    adapter.state["first"] = "bad";
    adapter.state["second"] = "bad";

    ScenarioRunnerOptions options;
    options.continueAfterFailure = true;

    const ScenarioDefinition scenario = MakeScenario({
        ScenarioStep::MakeAssertion("first", "good"),
        ScenarioStep::MakeAssertion("second", "good"),
    });

    const ScenarioResult result = ScenarioRunner{}.Run(scenario, adapter, options);

    EXPECT_EQ(result.stepsExecuted, 2u);
    EXPECT_EQ(result.diagnostics.size(), 2u);
    EXPECT_EQ(result.ErrorCount(), 2u);
    EXPECT_TRUE(result.HasErrors());
}

TEST(EditorLabScenarioRunnerTests, DelegatesEditorOperationsToAdapter)
{
    FakeScenarioRuntimeAdapter adapter;

    const ScenarioDefinition scenario = MakeScenario({
        ScenarioStep::MakeOpenPanel("saga.panel.console"),
        ScenarioStep::MakeClosePanel("saga.panel.console"),
        ScenarioStep::MakeLoadProject("Samples/Project"),
        ScenarioStep::MakeSetSelection("1001,1002"),
        ScenarioStep::MakeUndo(),
        ScenarioStep::MakeRedo(),
        ScenarioStep::MakeSave(),
        ScenarioStep::MakeWait(3),
        ScenarioStep::MakeSleep(7),
        ScenarioStep::MakeRegisterMockExtension("saga.extension.test", "{}"),
    });

    const ScenarioResult result = ScenarioRunner{}.Run(scenario, adapter);

    EXPECT_TRUE(result.Ok());
    const std::vector<std::string> expected = {
        "open_panel:saga.panel.console",
        "close_panel:saga.panel.console",
        "load_project:Samples/Project",
        "selection:1001,1002",
        "undo",
        "redo",
        "save",
        "wait:3",
        "sleep:7",
        "mock_extension:saga.extension.test:{}",
    };
    EXPECT_EQ(adapter.operations, expected);
}

TEST(EditorLabScenarioRunnerTests, MissingStatePathFailsWithoutCrashing)
{
    FakeScenarioRuntimeAdapter adapter;

    const ScenarioResult result = ScenarioRunner{}.Run(
        MakeScenario({ScenarioStep::MakeAssertion("editor.missing", "true")}),
        adapter);

    EXPECT_EQ(result.verdict, ScenarioVerdict::Fail);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].message,
              "Assertion failed for 'editor.missing': value was not available: missing state path");
}

TEST(EditorLabScenarioRunnerTests, ScenarioResultCountsWarningsAndErrors)
{
    ScenarioResult result;
    result.verdict = ScenarioVerdict::Pass;

    ScenarioDiagnostic warning;
    warning.severity = ScenarioDiagnostic::Severity::Warning;
    warning.message = "warn";
    result.AddDiagnostic(warning);

    ScenarioDiagnostic error;
    error.severity = ScenarioDiagnostic::Severity::Error;
    error.message = "error";
    result.AddDiagnostic(error);

    EXPECT_EQ(result.WarningCount(), 1u);
    EXPECT_EQ(result.ErrorCount(), 1u);
    EXPECT_TRUE(result.HasErrors());
    EXPECT_EQ(result.verdict, ScenarioVerdict::Fail);
    EXPECT_STREQ(ScenarioVerdictId(ScenarioVerdict::PassWithWarnings),
                 "pass_with_warnings");
}

TEST(EditorLabScenarioRunnerTests, BuiltinProfileScenarioRunsThroughFakeAdapter)
{
    FakeScenarioRuntimeAdapter adapter;
    PopulateBuiltinScenarioState(adapter);

    ScenarioDefinition scenario;
    scenario.id = "saga.lab.profile_switch";
    scenario.name = "Profile switch validation";
    scenario.steps = MakeProfileSwitchValidationScenario();

    const ScenarioResult result = ScenarioRunner{}.Run(scenario, adapter);

    EXPECT_TRUE(result.Ok());
    EXPECT_EQ(result.stepsExecuted, result.stepsTotal);
    EXPECT_EQ(result.snapshots.size(), 3u);
}

TEST(EditorLabScenarioRunnerTests, BuiltinCustomizationScenarioRunsThroughFakeAdapter)
{
    FakeScenarioRuntimeAdapter adapter;
    PopulateBuiltinScenarioState(adapter);

    ScenarioDefinition scenario;
    scenario.id = "saga.lab.customization";
    scenario.name = "Customization precedence";
    scenario.steps = MakeCustomizationPrecedenceScenario();

    const ScenarioResult result = ScenarioRunner{}.Run(scenario, adapter);

    EXPECT_TRUE(result.Ok());
    EXPECT_EQ(result.stepsExecuted, result.stepsTotal);
    EXPECT_EQ(result.snapshots.size(), 3u);
}

TEST(EditorLabScenarioRunnerTests, BuiltinScenarioDefinitionsPopulatePanelList)
{
    ScenarioRunnerPanelViewModel viewModel;

    const auto items = viewModel.GetScenarioItems();

    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].id, "saga.lab.profile_switch");
    EXPECT_EQ(items[0].name, "Profile Switch Validation");
    EXPECT_GT(items[0].stepCount, 0u);
    EXPECT_EQ(items[1].id, "saga.lab.customization_precedence");
    EXPECT_EQ(items[1].name, "Customization Precedence");
    EXPECT_GT(items[1].stepCount, 0u);
}

TEST(EditorLabScenarioRunnerTests, SelectingScenarioExposesMetadata)
{
    ScenarioRunnerPanelViewModel viewModel;

    ASSERT_TRUE(viewModel.SelectScenario("saga.lab.customization_precedence"));
    const ScenarioDefinition* selected = viewModel.GetSelectedScenario();

    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->id, "saga.lab.customization_precedence");
    EXPECT_EQ(selected->name, "Customization Precedence");
    EXPECT_EQ(selected->steps.size(), MakeCustomizationPrecedenceScenario().size());
}

TEST(EditorLabScenarioRunnerTests, RunningBuiltinScenarioThroughViewModelSucceeds)
{
    ScenarioRunnerPanelViewModel viewModel;

    ASSERT_TRUE(viewModel.SelectScenario("saga.lab.profile_switch"));
    ASSERT_TRUE(viewModel.RunSelectedScenario());
    const ScenarioRunnerResultSummary summary = viewModel.GetResultSummary();

    EXPECT_TRUE(summary.hasResult);
    EXPECT_EQ(summary.verdict, "pass");
    EXPECT_EQ(summary.stepsExecuted, summary.stepsTotal);
    EXPECT_EQ(summary.warningCount, 0u);
    EXPECT_EQ(summary.errorCount, 0u);
    EXPECT_EQ(summary.diagnosticCount, 0u);
    EXPECT_EQ(summary.snapshotCount, 3u);
}

TEST(EditorLabScenarioRunnerTests, ViewModelDefaultsToDeterministicRuntimeMode)
{
    ScenarioRunnerPanelViewModel viewModel;

    const auto modes = viewModel.GetRuntimeModeItems();

    ASSERT_EQ(modes.size(), 1u);
    EXPECT_EQ(modes[0].id, "deterministic");
    EXPECT_EQ(modes[0].name, "Deterministic");
    EXPECT_EQ(viewModel.GetSelectedRuntimeModeId(), "deterministic");
}

TEST(EditorLabScenarioRunnerTests, ViewModelRejectsConnectedModeWithoutAdapter)
{
    ScenarioRunnerPanelViewModel viewModel;

    EXPECT_FALSE(viewModel.SelectRuntimeMode("connected"));
    EXPECT_EQ(viewModel.GetSelectedRuntimeModeId(), "deterministic");
}

TEST(EditorLabScenarioRunnerTests, ViewModelListsAndSelectsConnectedModeWithAdapter)
{
    FakeScenarioRuntimeAdapter adapter;
    ScenarioRunnerPanelViewModel viewModel(adapter);

    const auto modes = viewModel.GetRuntimeModeItems();

    ASSERT_EQ(modes.size(), 2u);
    EXPECT_EQ(modes[0].id, "deterministic");
    EXPECT_EQ(modes[1].id, "connected");
    EXPECT_EQ(modes[1].name, "Connected");
    ASSERT_TRUE(viewModel.SelectRuntimeMode("connected"));
    EXPECT_EQ(viewModel.GetSelectedRuntimeModeId(), "connected");
}

TEST(EditorLabScenarioRunnerTests, ViewModelRejectsUnknownRuntimeMode)
{
    FakeScenarioRuntimeAdapter adapter;
    ScenarioRunnerPanelViewModel viewModel(adapter);

    ASSERT_TRUE(viewModel.SelectRuntimeMode("connected"));
    EXPECT_FALSE(viewModel.SelectRuntimeMode("runtime.invalid"));
    EXPECT_EQ(viewModel.GetSelectedRuntimeModeId(), "connected");
}

TEST(EditorLabScenarioRunnerTests, ViewModelConnectedModeRunsInjectedAdapter)
{
    FakeScenarioRuntimeAdapter adapter;
    adapter.state["editor.connected"] = "true";
    ScenarioRunnerPanelViewModel viewModel({MakeScenario({
        ScenarioStep::MakeAssertion("editor.connected", "true"),
    })}, adapter);
    ASSERT_TRUE(viewModel.SelectRuntimeMode("connected"));

    ASSERT_TRUE(viewModel.RunSelectedScenario());
    const ScenarioResult* result = viewModel.GetLastResult();

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->Ok());
    ASSERT_EQ(adapter.operations.size(), 1u);
    EXPECT_EQ(adapter.operations[0], "read:editor.connected");
}

TEST(EditorLabScenarioRunnerTests, ViewModelDeterministicModeIgnoresInjectedAdapter)
{
    FakeScenarioRuntimeAdapter adapter;
    adapter.state["editor.lab.missing"] = "true";
    ScenarioRunnerPanelViewModel viewModel({MakeScenario({
        ScenarioStep::MakeAssertion("editor.lab.missing", "true"),
    })}, adapter);

    ASSERT_TRUE(viewModel.RunSelectedScenario());
    const ScenarioRunnerResultSummary summary = viewModel.GetResultSummary();
    const auto diagnostics = viewModel.GetDiagnosticRows();

    EXPECT_EQ(summary.verdict, "fail");
    EXPECT_TRUE(adapter.operations.empty());
    ASSERT_EQ(diagnostics.size(), 1u);
    EXPECT_NE(diagnostics[0].message.find("missing deterministic state path"),
              std::string::npos);
}

TEST(EditorLabScenarioRunnerTests, FailingViewModelScenarioProducesDiagnostics)
{
    ScenarioDefinition scenario = MakeScenario({
        ScenarioStep::MakeAssertion("editor.lab.missing", "true"),
    });

    ScenarioRunnerPanelViewModel viewModel({scenario});

    ASSERT_TRUE(viewModel.RunSelectedScenario());
    const ScenarioRunnerResultSummary summary = viewModel.GetResultSummary();
    const auto diagnostics = viewModel.GetDiagnosticRows();

    EXPECT_EQ(summary.verdict, "fail");
    EXPECT_EQ(summary.stepsExecuted, 1u);
    EXPECT_EQ(summary.errorCount, 1u);
    ASSERT_EQ(diagnostics.size(), 1u);
    EXPECT_EQ(diagnostics[0].severity, "Error");
    EXPECT_EQ(diagnostics[0].stepIndex, 0);
    EXPECT_NE(diagnostics[0].message.find("missing deterministic state path"),
              std::string::npos);
}

TEST(EditorLabScenarioRunnerTests, ViewModelSnapshotSummaryPreservesLabels)
{
    ScenarioRunnerPanelViewModel viewModel;

    ASSERT_TRUE(viewModel.SelectScenario("saga.lab.customization_precedence"));
    ASSERT_TRUE(viewModel.RunSelectedScenario());
    const auto snapshots = viewModel.GetSnapshotRows();

    ASSERT_EQ(snapshots.size(), 3u);
    EXPECT_EQ(snapshots[0].label, "customization.builtin");
    EXPECT_EQ(snapshots[1].label, "customization.project.basic");
    EXPECT_EQ(snapshots[2].label, "customization.project.advanced");
    EXPECT_GE(snapshots[0].stepIndex, 0);
    EXPECT_TRUE(snapshots[0].sha256.empty());
}

TEST(EditorLabScenarioRunnerTests, DeterministicAdapterRunsBuiltinsWithoutEditorHost)
{
    DeterministicScenarioRuntimeAdapter adapter;

    ScenarioDefinition scenario;
    scenario.id = "saga.lab.profile_switch";
    scenario.name = "Profile Switch Validation";
    scenario.steps = MakeProfileSwitchValidationScenario();

    const ScenarioResult result = ScenarioRunner{}.Run(scenario, adapter);

    EXPECT_TRUE(result.Ok());
    EXPECT_FALSE(adapter.GetOperations().empty());
    EXPECT_EQ(result.snapshots.size(), 3u);
}

TEST(EditorLabScenarioRunnerTests, EditorHostAdapterDispatchesRegisteredCommands)
{
    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<SagaEditor::MemoryEditorSettingsStore>()));

    bool invoked = false;
    SagaEditor::CommandDescriptor command;
    command.id = "saga.command.lab.test";
    command.label = "Lab Test";
    command.category = "Tests";
    command.handler = [&invoked]() { invoked = true; };
    host.GetCommandRegistry().Register(std::move(command));

    EditorHostScenarioRuntimeAdapter adapter(host);
    const ScenarioAdapterResult ok =
        adapter.DispatchAction("saga.command.lab.test", {});
    EXPECT_TRUE(ok.succeeded);
    EXPECT_TRUE(invoked);

    const ScenarioAdapterResult missing =
        adapter.DispatchAction("saga.command.lab.missing", {});
    EXPECT_FALSE(missing.succeeded);

    host.Shutdown();
}

TEST(EditorLabScenarioRunnerTests, EditorHostAdapterReadsPublicEditorState)
{
    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<SagaEditor::MemoryEditorSettingsStore>()));

    SagaEditor::EditorDiagnostic diagnostic;
    diagnostic.severity = SagaEditor::EditorDiagnosticSeverity::Error;
    diagnostic.source = "editorlab-test";
    diagnostic.code = "EDITORLAB_TEST";
    diagnostic.message = "test diagnostic";
    host.GetEditorDiagnosticsService().Add(std::move(diagnostic));

    EditorHostScenarioRuntimeAdapter adapter(host);
    ASSERT_TRUE(adapter.SetSelection("1001, 1002").succeeded);

    EXPECT_EQ(adapter.ReadState("editor.engine_bridge.state").value, "Ready");
    EXPECT_EQ(adapter.ReadState("editor.customization.boundary").value,
              "editor_only");
    EXPECT_EQ(adapter.ReadState("editor.customization.builtin.available").value,
              "true");
    EXPECT_EQ(adapter.ReadState("profile.layout.diff.basic_to_advanced").value,
              "true");
    EXPECT_EQ(adapter.ReadState("profile.shortcuts.diff.basic_to_advanced").value,
              "true");
    EXPECT_EQ(adapter.ReadState("editor.selection.count").value, "2");
    EXPECT_EQ(adapter.ReadState("editor.selection.primary").value, "1001");
    EXPECT_EQ(adapter.ReadState("editor.selection.ids").value, "1001,1002");
    EXPECT_EQ(adapter.ReadState("editor.diagnostics.count").value, "1");
    EXPECT_EQ(adapter.ReadState("editor.diagnostics.error_count").value, "1");

    host.Shutdown();
}

TEST(EditorLabScenarioRunnerTests, EditorShellAdapterOpensRegisteredPanel)
{
    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<SagaEditor::MemoryEditorSettingsStore>()));

    auto window = std::make_unique<FakeMainWindow>();
    FakeMainWindow* windowRaw = window.get();

    SagaEditor::EditorShell shell;
    SagaEditor::EditorShellConfig config;
    config.registerDefaultPanels = false;
    config.showOnInit = false;
    config.applyActivePersona = false;
    ASSERT_TRUE(shell.Init(host, std::move(window), config));
    shell.RegisterPanel(
        std::make_unique<FakePanel>("saga.panel.editorlab.test", "EditorLab Test"),
        SagaEditor::UIDockArea::Left);
    windowRaw->focusedPanels.clear();

    EditorShellScenarioRuntimeAdapter adapter(shell);
    const ScenarioAdapterResult result =
        adapter.OpenPanel("saga.panel.editorlab.test");

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(windowRaw->visiblePanels["saga.panel.editorlab.test"]);
    EXPECT_TRUE(windowRaw->focusedPanels.empty());

    shell.Shutdown();
    host.Shutdown();
}

TEST(EditorLabScenarioRunnerTests, EditorShellAdapterClosesRegisteredPanel)
{
    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<SagaEditor::MemoryEditorSettingsStore>()));

    auto window = std::make_unique<FakeMainWindow>();
    FakeMainWindow* windowRaw = window.get();

    SagaEditor::EditorShell shell;
    SagaEditor::EditorShellConfig config;
    config.registerDefaultPanels = false;
    config.showOnInit = false;
    config.applyActivePersona = false;
    ASSERT_TRUE(shell.Init(host, std::move(window), config));
    shell.RegisterPanel(
        std::make_unique<FakePanel>("saga.panel.editorlab.test", "EditorLab Test"),
        SagaEditor::UIDockArea::Left);
    windowRaw->visiblePanels["saga.panel.editorlab.test"] = true;
    windowRaw->focusedPanels.clear();

    EditorShellScenarioRuntimeAdapter adapter(shell);
    const ScenarioAdapterResult result =
        adapter.ClosePanel("saga.panel.editorlab.test");

    EXPECT_TRUE(result.succeeded);
    EXPECT_FALSE(windowRaw->visiblePanels["saga.panel.editorlab.test"]);
    EXPECT_TRUE(windowRaw->focusedPanels.empty());

    shell.Shutdown();
    host.Shutdown();
}

TEST(EditorLabScenarioRunnerTests, EditorShellAdapterRejectsUnknownPanel)
{
    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<SagaEditor::MemoryEditorSettingsStore>()));

    SagaEditor::EditorShell shell;
    SagaEditor::EditorShellConfig config;
    config.registerDefaultPanels = false;
    config.showOnInit = false;
    config.applyActivePersona = false;
    ASSERT_TRUE(shell.Init(host, std::make_unique<FakeMainWindow>(), config));

    EditorShellScenarioRuntimeAdapter adapter(shell);
    const ScenarioAdapterResult openResult = adapter.OpenPanel("saga.panel.unknown");
    const ScenarioAdapterResult closeResult = adapter.ClosePanel("saga.panel.unknown");

    EXPECT_FALSE(openResult.succeeded);
    EXPECT_EQ(openResult.message, "panel is not registered: saga.panel.unknown");
    EXPECT_FALSE(closeResult.succeeded);
    EXPECT_EQ(closeResult.message, "panel is not registered: saga.panel.unknown");

    shell.Shutdown();
    host.Shutdown();
}

TEST(EditorLabScenarioRunnerTests, EditorShellAdapterDelegatesHostStateReads)
{
    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<SagaEditor::MemoryEditorSettingsStore>()));

    SagaEditor::EditorShell shell;
    SagaEditor::EditorShellConfig config;
    config.registerDefaultPanels = false;
    config.showOnInit = false;
    config.applyActivePersona = false;
    ASSERT_TRUE(shell.Init(host, std::make_unique<FakeMainWindow>(), config));

    EditorShellScenarioRuntimeAdapter adapter(shell);
    ASSERT_TRUE(adapter.SetSelection("42").succeeded);

    EXPECT_EQ(adapter.ReadState("editor.selection.primary").value, "42");

    shell.Shutdown();
    host.Shutdown();
}

TEST(EditorLabScenarioRunnerTests, ScenarioRunnerDockPanelHasStableIdentity)
{
    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<SagaEditor::MemoryEditorSettingsStore>()));

    SagaEditor::EditorShell shell;
    SagaEditor::EditorShellConfig config;
    config.registerDefaultPanels = false;
    config.showOnInit = false;
    config.applyActivePersona = false;
    ASSERT_TRUE(shell.Init(host, std::make_unique<FakeMainWindow>(), config));

    ScenarioRunnerDockPanel panel(shell);

    EXPECT_EQ(panel.GetPanelId(), "saga.panel.editorlab.scenario_runner");
    EXPECT_EQ(panel.GetTitle(), "EditorLab Scenarios");

    shell.Shutdown();
    host.Shutdown();
}

TEST(EditorLabScenarioRunnerTests, ViewModelCanRunScenarioThroughInjectedAdapter)
{
    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<SagaEditor::MemoryEditorSettingsStore>()));

    EditorHostScenarioRuntimeAdapter adapter(host);
    ScenarioRunnerPanelViewModel viewModel({MakeScenario({
        ScenarioStep::MakeSetSelection("42"),
        ScenarioStep::MakeAssertion("editor.selection.primary", "42"),
    })}, adapter);

    ASSERT_TRUE(viewModel.SelectRuntimeMode("connected"));
    ASSERT_TRUE(viewModel.RunSelectedScenario());
    const ScenarioResult* result = viewModel.GetLastResult();
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->Ok());

    host.Shutdown();
}
