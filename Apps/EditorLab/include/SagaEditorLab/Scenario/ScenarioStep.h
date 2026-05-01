/// @file ScenarioStep.h
/// @brief One step inside an EditorLab scenario — pure data, no editor dep.
///
/// Layer  : Tools / EditorLab
/// Purpose: A scenario is a list of `ScenarioStep` values. Each step is a
///          tagged union: pick a kind, fill the matching payload, run.
/// Isolation: this header includes nothing from SagaEditor or SagaEngine.
///            EditorLab consumes editor types only through `Adapters/`.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEditorLab
{

// ─── Step Kind ────────────────────────────────────────────────────────────────

/// What the step does. Concrete kinds correspond directly to actions the
/// editor's public surface exposes; new kinds land alongside new editor
/// affordances. Adding a kind is a public-API change for scenarios so the
/// id strings (`ScenarioStepKindId`) are part of the JSON contract.
enum class ScenarioStepKind : std::uint8_t
{
    /// Invoke a named editor command (`saga.command.file.save`, …).
    Action,

    /// Assert that a named property of the editor state has the
    /// expected value. The lab's `StateInspector` evaluates the path.
    Assertion,

    /// Pause the scenario for `wait.tickCount` simulation ticks.
    /// Used to drive deterministic scheduling without wall-clock sleeps.
    Wait,

    /// Real-time sleep (milliseconds). Used sparingly; non-deterministic.
    Sleep,

    /// Capture a `StateSnapshot` and tag it with `snapshot.label` for
    /// later diff-based assertions.
    Snapshot,

    /// Open a project at `loadProject.path`.
    LoadProject,

    /// Replace the editor selection with the supplied entity id list.
    SetSelection,

    /// Perform an editor undo / redo operation.
    Undo,
    Redo,

    /// Save the active scene through the editor's command system.
    Save,

    /// Toggle a panel by id (e.g. `saga.panel.console`).
    OpenPanel,
    ClosePanel,

    /// Register a mock extension provided by the lab itself (used by
    /// extension-API integration scenarios).
    RegisterMockExtension,
};

[[nodiscard]] const char*       ScenarioStepKindId(ScenarioStepKind kind) noexcept;
[[nodiscard]] ScenarioStepKind  ScenarioStepKindFromId(const std::string& id) noexcept;

// ─── Payloads ─────────────────────────────────────────────────────────────────

struct ActionPayload
{
    std::string commandId;       ///< e.g. "saga.command.file.save"
    std::string argument;        ///< Optional opaque argument (JSON string).
};

struct AssertionPayload
{
    std::string statePath;       ///< Dot-separated path inside the snapshot.
    std::string expectedValue;   ///< String-encoded expected value.
};

struct WaitPayload
{
    std::uint32_t tickCount = 0;
};

struct SleepPayload
{
    std::uint32_t milliseconds = 0;
};

struct SnapshotPayload
{
    std::string label;           ///< Stable name the snapshot is filed under.
};

struct LoadProjectPayload
{
    std::string path;
};

struct SelectionPayload
{
    /// Comma-separated entity ids, e.g. "1001,1002,1003". Stored as a
    /// string to keep the payload struct trivially copyable / JSON-friendly.
    std::string entityIds;
};

struct PanelPayload
{
    std::string panelId;
};

struct MockExtensionPayload
{
    std::string extensionId;
    std::string manifestJson;
};

// ─── Scenario Step ────────────────────────────────────────────────────────────

/// One step in a scenario. The payload union is intentionally simple
/// (every payload's fields side-by-side, only the active one matters)
/// so the JSON loader can populate fields by name without a tagged
/// std::variant decoder.
struct ScenarioStep
{
    ScenarioStepKind kind = ScenarioStepKind::Action;
    std::string      label;                   ///< Optional friendly name for reports.

    ActionPayload         action;
    AssertionPayload      assertion;
    WaitPayload           wait;
    SleepPayload          sleep;
    SnapshotPayload       snapshot;
    LoadProjectPayload    loadProject;
    SelectionPayload      selection;
    PanelPayload          panel;
    MockExtensionPayload  mockExtension;

    // ─── Convenience Builders ─────────────────────────────────────────────────

    [[nodiscard]] static ScenarioStep MakeAction(std::string commandId,
                                                  std::string argument = {});
    [[nodiscard]] static ScenarioStep MakeAssertion(std::string statePath,
                                                    std::string expectedValue);
    [[nodiscard]] static ScenarioStep MakeWait(std::uint32_t tickCount) noexcept;
    [[nodiscard]] static ScenarioStep MakeSleep(std::uint32_t milliseconds) noexcept;
    [[nodiscard]] static ScenarioStep MakeSnapshot(std::string label);
    [[nodiscard]] static ScenarioStep MakeLoadProject(std::string path);
    [[nodiscard]] static ScenarioStep MakeSetSelection(std::string entityIds);
    [[nodiscard]] static ScenarioStep MakeUndo() noexcept;
    [[nodiscard]] static ScenarioStep MakeRedo() noexcept;
    [[nodiscard]] static ScenarioStep MakeSave() noexcept;
    [[nodiscard]] static ScenarioStep MakeOpenPanel(std::string panelId);
    [[nodiscard]] static ScenarioStep MakeClosePanel(std::string panelId);
    [[nodiscard]] static ScenarioStep MakeRegisterMockExtension(std::string extensionId,
                                                                 std::string manifestJson);
};

} // namespace SagaEditorLab
