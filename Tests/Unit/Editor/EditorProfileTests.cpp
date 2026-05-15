/// @file EditorProfileTests.cpp
/// @brief GoogleTest coverage for editor workflow profiles and shell switching.

#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/ShortcutManager.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Panels/IPanel.h"
#include "SagaEditor/Profile/EditorProfile.h"
#include "SagaEditor/Profile/EditorProfileActivator.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Profile/EditorProfileSettingsBinding.h"
#include "SagaEditor/Selection/SelectionManager.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIApplication.h"
#include "SagaEditor/UI/IUIFactory.h"
#include "SagaEditor/UI/IUIMainWindow.h"
#include "SagaEditorLab/Scenario/CustomizationScenario.h"
#include "SagaEditorLab/Scenario/ProfileSwitchScenario.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

using namespace SagaEditor;
using SagaEditorLab::MakeCustomizationPrecedenceScenario;
using SagaEditorLab::MakeProfileSwitchValidationScenario;

class FakeMainWindow final : public IUIMainWindow
{
public:
    void Show() override { shown = true; }
    void ShowMaximized() override { maximized = true; shown = true; }
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
    void SetSize(int w, int h) override { width = w; height = h; }

    [[nodiscard]] int GetWidth() const noexcept override { return width; }
    [[nodiscard]] int GetHeight() const noexcept override { return height; }
    [[nodiscard]] void* GetNativeHandle() const noexcept override { return nullptr; }

    void ApplyShellLayout(const ShellLayout& value) override
    {
        layout = value;
        ++layoutApplyCount;
    }

    void SetStatusMessage(const std::string& value) override { status = value; }

    void DockPanel(void*,
                   const std::string& panelId,
                   const std::string&,
                   UIDockArea) override
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
    ShellLayout layout;
    int layoutApplyCount = 0;
    std::unordered_map<std::string, bool> visiblePanels;
    std::vector<std::string> focusedPanels;
    CloseCallback onClose;
    CommandCallback onCommand;
};

class FakeUIFactory final : public IUIFactory
{
public:
    [[nodiscard]] std::unique_ptr<IUIApplication>
    CreateApplication(int&, char**) override
    {
        return nullptr;
    }

    [[nodiscard]] std::unique_ptr<IUIMainWindow>
    CreateMainWindow(const std::string&, int, int) override
    {
        return std::make_unique<FakeMainWindow>();
    }

    [[nodiscard]] std::unique_ptr<IEditorSettingsStore>
    CreateSettingsStore() override
    {
        return std::make_unique<MemoryEditorSettingsStore>();
    }
};

class FakePanel final : public IPanel
{
public:
    FakePanel(std::string panelId, std::string titleText)
        : id(std::move(panelId)), title(std::move(titleText))
    {}

    [[nodiscard]] PanelId GetPanelId() const override { return id; }
    [[nodiscard]] std::string GetTitle() const override { return title; }
    [[nodiscard]] void* GetNativeWidget() const noexcept override { return nullptr; }

    std::string id;
    std::string title;
};

[[nodiscard]] bool Contains(const std::vector<std::string>& values,
                            const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

// ─── EditorProfile ───────────────────────────────────────────────────────────

TEST(EditorProfileTest, BuiltinProfilesHaveStableDistinctIds)
{
    const EditorProfile profiles[] = {
        MakeBasicProfile(),
        MakeNodeEditorProfile(),
        MakeStandardPipelineProfile(),
        MakeAdvancedPipelineProfile(),
        MakeCustomProfile(),
    };

    const std::string expectedIds[] = {
        "saga.profile.basic",
        "saga.profile.node_editor",
        "saga.profile.standard_pipeline",
        "saga.profile.advanced_pipeline",
        "saga.profile.custom",
    };

    for (std::size_t i = 0; i < std::size(profiles); ++i)
    {
        EXPECT_EQ(profiles[i].id, expectedIds[i]);
        EXPECT_FALSE(profiles[i].displayName.empty());

        for (std::size_t j = 0; j < i; ++j)
        {
            EXPECT_NE(profiles[i].id, profiles[j].id);
        }
    }
}

TEST(EditorProfileTest, FiveProfilesRepresentDifferentWorkflowShapes)
{
    const auto basic = MakeBasicProfile();
    const auto node = MakeNodeEditorProfile();
    const auto standard = MakeStandardPipelineProfile();
    const auto advanced = MakeAdvancedPipelineProfile();
    const auto custom = MakeCustomProfile();

    EXPECT_FALSE(basic.exposesAssetBrowser);
    EXPECT_FALSE(basic.exposesProfiler);
    EXPECT_TRUE(node.exposesGraphEditor);
    EXPECT_FALSE(node.exposesAssetBrowser);
    EXPECT_TRUE(standard.exposesConsole);
    EXPECT_TRUE(advanced.exposesProfiler);
    EXPECT_EQ(custom.layoutPresetId, "custom");

    EXPECT_NE(basic.layoutPresetId, node.layoutPresetId);
    EXPECT_NE(node.layoutPresetId, standard.layoutPresetId);
    EXPECT_NE(standard.layoutPresetId, advanced.layoutPresetId);
}

// ─── EditorProfileRegistry ───────────────────────────────────────────────────

TEST(EditorProfileRegistryTest, BuiltinRegistrationProducesFiveProfiles)
{
    EditorProfileRegistry registry;
    registry.RegisterBuiltinProfiles();

    EXPECT_EQ(registry.Size(), 5u);
    EXPECT_TRUE(registry.Has("saga.profile.basic"));
    EXPECT_TRUE(registry.Has("saga.profile.node_editor"));
    EXPECT_TRUE(registry.Has("saga.profile.standard_pipeline"));
    EXPECT_TRUE(registry.Has("saga.profile.advanced_pipeline"));
    EXPECT_TRUE(registry.Has("saga.profile.custom"));
}

TEST(EditorProfileRegistryTest, SetActiveFiresChangeCallback)
{
    EditorProfileRegistry registry;
    registry.RegisterBuiltinProfiles();

    std::atomic<int> calls{0};
    std::string seenId;
    const auto sub = registry.OnProfileChanged(
        [&](const EditorProfile& profile)
        {
            ++calls;
            seenId = profile.id;
        });

    EXPECT_NE(sub, kInvalidEditorProfileSubscription);
    EXPECT_TRUE(registry.SetActive("saga.profile.advanced_pipeline"));
    EXPECT_EQ(calls.load(), 1);
    EXPECT_EQ(seenId, "saga.profile.advanced_pipeline");

    EXPECT_TRUE(registry.SetActive("saga.profile.advanced_pipeline"));
    EXPECT_EQ(calls.load(), 1);
    EXPECT_FALSE(registry.SetActive("saga.profile.missing"));
    EXPECT_EQ(calls.load(), 1);
}

TEST(EditorProfileRegistryTest, LegacyIdsResolveToCanonicalProfiles)
{
    EditorProfileRegistry registry;
    registry.RegisterBuiltinProfiles();

    EXPECT_TRUE(registry.SetActive("saga.profile.creator"));
    EXPECT_EQ(registry.GetActiveId(), "saga.profile.basic");

    EXPECT_TRUE(registry.SetActive("saga.profile.studio"));
    EXPECT_EQ(registry.GetActiveId(), "saga.profile.advanced_pipeline");
}

// ─── EditorProfileSettingsBinding ────────────────────────────────────────────

TEST(EditorProfileSettingsBindingTest, RestoresValidStoredProfile)
{
    EditorProfileRegistry registry;
    registry.RegisterBuiltinProfiles();

    MemoryEditorSettingsStore settings;
    settings.SetString("profile.activeId", "saga.profile.node_editor");

    EditorProfileSettingsBinding binding;
    ASSERT_TRUE(binding.Init(registry, settings));

    EXPECT_EQ(registry.GetActiveId(), "saga.profile.node_editor");
}

TEST(EditorProfileSettingsBindingTest, FallsBackWhenStoredProfileIsInvalid)
{
    EditorProfileRegistry registry;
    registry.RegisterBuiltinProfiles();

    MemoryEditorSettingsStore settings;
    settings.SetString("profile.activeId", "saga.profile.missing");

    EditorProfileSettingsBinding binding;
    ASSERT_TRUE(binding.Init(registry, settings));

    EXPECT_EQ(registry.GetActiveId(), DefaultEditorProfileId());
    EXPECT_EQ(settings.GetString("profile.activeId", ""), DefaultEditorProfileId());
}

TEST(EditorProfileSettingsBindingTest, MigratesLegacyStoredProfile)
{
    EditorProfileRegistry registry;
    registry.RegisterBuiltinProfiles();

    MemoryEditorSettingsStore settings;
    settings.SetString("profile.activeId", "saga.profile.builder");

    EditorProfileSettingsBinding binding;
    ASSERT_TRUE(binding.Init(registry, settings));

    EXPECT_EQ(registry.GetActiveId(), "saga.profile.standard_pipeline");
    EXPECT_EQ(settings.GetString("profile.activeId", ""),
              "saga.profile.standard_pipeline");
}

TEST(EditorProfileSettingsBindingTest, PersistsProfileChanges)
{
    EditorProfileRegistry registry;
    registry.RegisterBuiltinProfiles();

    MemoryEditorSettingsStore settings;

    EditorProfileSettingsBinding binding;
    ASSERT_TRUE(binding.Init(registry, settings));

    EXPECT_TRUE(registry.SetActive("saga.profile.advanced_pipeline"));
    EXPECT_EQ(settings.GetString("profile.activeId", ""),
              "saga.profile.advanced_pipeline");
}

// ─── EditorProfileActivator ──────────────────────────────────────────────────

TEST(EditorProfileActivatorTest, ApplyFansOutToEverySink)
{
    EditorProfileActivator activator;
    const auto profile = MakeAdvancedPipelineProfile();

    std::string seenLayout;
    EditorProfile seenShortcutProfile;
    std::vector<std::string> seenToolbar;
    std::vector<std::string> seenPanels;
    EditorProfile seenChromeProfile;
    std::vector<std::string> seenTools;

    activator.SetLayoutSink(
        [&](const std::string& id) { seenLayout = id; return true; });
    activator.SetShortcutMapSink(
        [&](const EditorProfile& p) { seenShortcutProfile = p; return true; });
    activator.SetToolbarSink(
        [&](const std::vector<std::string>& ids) { seenToolbar = ids; return true; });
    activator.SetPanelVisibilitySink(
        [&](const std::vector<std::string>& ids) { seenPanels = ids; return true; });
    activator.SetShellChromeSink(
        [&](const EditorProfile& p) { seenChromeProfile = p; return true; });
    activator.SetToolVisibilitySink(
        [&](const std::vector<std::string>& ids) { seenTools = ids; return true; });

    const auto result = activator.Apply(profile);
    EXPECT_TRUE(result.AllApplied());
    EXPECT_EQ(seenLayout, profile.layoutPresetId);
    EXPECT_EQ(seenShortcutProfile, profile);
    EXPECT_EQ(seenToolbar, profile.defaultToolbarCommands);
    EXPECT_EQ(seenPanels, profile.defaultPanels);
    EXPECT_EQ(seenChromeProfile, profile);
    EXPECT_EQ(seenTools, profile.visibleToolCommands);
}

TEST(EditorProfileActivatorTest, FailingSinkDoesNotAbortLaterSinks)
{
    EditorProfileActivator activator;
    std::atomic<int> calls{0};

    activator.SetLayoutSink(
        [&](const std::string&) { ++calls; return false; });
    activator.SetShortcutMapSink(
        [&](const EditorProfile&) { ++calls; return true; });
    activator.SetToolbarSink(
        [&](const std::vector<std::string>&) { ++calls; return true; });

    const auto result = activator.Apply(MakeStandardPipelineProfile());
    EXPECT_EQ(calls.load(), 3);
    EXPECT_FALSE(result.layoutApplied);
    EXPECT_TRUE(result.shortcutMapApplied);
    EXPECT_TRUE(result.toolbarApplied);
    EXPECT_FALSE(result.lastError.empty());
}

// ─── EditorHost / EditorShell Runtime Switch ─────────────────────────────────

TEST(EditorProfileShellTest, RuntimeProfileSwitchChangesShellOnly)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));

    auto* selectionBefore = &host.GetSelectionManager();
    auto* commandRegistryBefore = &host.GetCommandRegistry();

    FakeUIFactory factory;
    EditorShell shell;
    EditorShellConfig cfg;
    cfg.registerDefaultPanels = false;
    cfg.applyActivePersona = false;
    ASSERT_TRUE(shell.Init(host, factory, cfg));

    shell.RegisterPanel(std::make_unique<FakePanel>("saga.panel.hierarchy", "Hierarchy"),
                        UIDockArea::Left);
    shell.RegisterPanel(std::make_unique<FakePanel>("saga.panel.viewport", "Viewport"),
                        UIDockArea::Center);
    shell.RegisterPanel(std::make_unique<FakePanel>("saga.panel.inspector", "Inspector"),
                        UIDockArea::Right);
    shell.RegisterPanel(std::make_unique<FakePanel>("saga.panel.console", "Console"),
                        UIDockArea::Bottom);
    shell.RegisterPanel(std::make_unique<FakePanel>("saga.panel.assetbrowser", "Assets"),
                        UIDockArea::Bottom);

    ASSERT_TRUE(host.GetCommandDispatcher().Dispatch("saga.command.profile.basic"));
    auto& window = dynamic_cast<FakeMainWindow&>(shell.GetMainWindow());

    EXPECT_EQ(host.GetEditorProfileRegistry().GetActiveId(), "saga.profile.basic");
    EXPECT_EQ(shell.GetActiveLayoutPresetId(), "basic");
    EXPECT_TRUE(window.title.find("Basic") != std::string::npos);
    EXPECT_TRUE(window.status.find("Basic") != std::string::npos);
    EXPECT_FALSE(window.visiblePanels["saga.panel.assetbrowser"]);
    EXPECT_TRUE(window.visiblePanels["saga.panel.console"]);
    EXPECT_EQ(host.GetShortcutManager().GetBoundCommand(KeyChord{1u, 'S'}),
              "saga.command.file.save");
    EXPECT_EQ(selectionBefore, &host.GetSelectionManager());
    EXPECT_EQ(commandRegistryBefore, &host.GetCommandRegistry());

    ASSERT_TRUE(host.GetCommandDispatcher().Dispatch("saga.command.profile.advanced_pipeline"));
    EXPECT_EQ(host.GetEditorProfileRegistry().GetActiveId(), "saga.profile.advanced_pipeline");
    EXPECT_EQ(shell.GetActiveLayoutPresetId(), "advanced_pipeline");
    EXPECT_TRUE(window.title.find("Advanced Pipeline") != std::string::npos);
    EXPECT_TRUE(window.visiblePanels["saga.panel.assetbrowser"]);
    EXPECT_FALSE(shell.GetShellLayout().mainToolbarItems.empty());
    EXPECT_TRUE(Contains(shell.GetActiveToolbarCommands(), "saga.command.build"));
    EXPECT_TRUE(Contains(shell.GetActiveToolCommands(), "saga.command.build"));
    EXPECT_EQ(host.GetShortcutManager().GetBoundCommand(KeyChord{1u | 2u, 'B'}),
              "saga.command.build");
    EXPECT_EQ(selectionBefore, &host.GetSelectionManager());
    EXPECT_EQ(commandRegistryBefore, &host.GetCommandRegistry());

    ASSERT_TRUE(host.GetCommandDispatcher().Dispatch("saga.command.profile.standard_pipeline"));
    EXPECT_EQ(host.GetEditorProfileRegistry().GetActiveId(), "saga.profile.standard_pipeline");
    EXPECT_EQ(shell.GetActiveLayoutPresetId(), "standard_pipeline");
    EXPECT_FALSE(Contains(shell.GetActiveToolCommands(), "saga.command.build"));

    shell.Shutdown();
    host.Shutdown();
}

TEST(EditorProfileShellTest, MountedWindowInitUsesInjectedWindowWithoutShowing)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));

    auto window = std::make_unique<FakeMainWindow>();
    FakeMainWindow* windowRaw = window.get();

    EditorShell shell;
    EditorShellConfig cfg;
    cfg.windowTitle = "Saga - Mounted Project";
    cfg.windowWidth = 1280;
    cfg.windowHeight = 720;
    cfg.registerDefaultPanels = false;
    cfg.applyActivePersona = false;
    cfg.showOnInit = false;

    ASSERT_TRUE(shell.Init(host, std::move(window), cfg));

    EXPECT_EQ(windowRaw->title, "Saga - Mounted Project - Basic");
    EXPECT_EQ(windowRaw->width, 1280);
    EXPECT_EQ(windowRaw->height, 720);
    EXPECT_FALSE(windowRaw->shown);
    EXPECT_FALSE(windowRaw->maximized);

    shell.RegisterPanel(std::make_unique<FakePanel>("saga.panel.hierarchy", "Hierarchy"),
                        UIDockArea::Left);
    shell.RegisterPanel(std::make_unique<FakePanel>("saga.panel.viewport", "Viewport"),
                        UIDockArea::Center);

    const std::vector<std::string> titles = shell.GetRegisteredPanelTitles();
    ASSERT_EQ(titles.size(), 2u);
    EXPECT_EQ(titles[0], "Hierarchy");
    EXPECT_EQ(titles[1], "Viewport");
    EXPECT_TRUE(windowRaw->visiblePanels["saga.panel.hierarchy"]);
    EXPECT_TRUE(windowRaw->visiblePanels["saga.panel.viewport"]);

    shell.Shutdown();
    host.Shutdown();
}

TEST(EditorLabProfileScenarioTest, BuiltinScenarioCapturesVisibleProfileDiffs)
{
    const auto steps = MakeProfileSwitchValidationScenario();
    ASSERT_GE(steps.size(), 14u);

    EXPECT_EQ(steps[0].panel.panelId, "saga.panel.production_dashboard");
    EXPECT_EQ(steps[1].assertion.statePath, "editor.engine_bridge.state");
    EXPECT_EQ(steps[3].action.commandId, "saga.command.profile.basic");
    EXPECT_EQ(steps[5].action.commandId, "saga.command.profile.advanced_pipeline");
    EXPECT_EQ(steps[10].action.commandId, "saga.command.profile.standard_pipeline");
    EXPECT_EQ(steps[7].assertion.statePath, "profile.layout.diff.basic_to_advanced");
    EXPECT_EQ(steps[8].assertion.statePath, "profile.shortcuts.diff.basic_to_advanced");
    EXPECT_EQ(steps[9].assertion.statePath, "profile.panels.diff.basic_to_advanced");
    EXPECT_EQ(steps[steps.size() - 2].assertion.statePath, "editor.core.identity.stable");
    EXPECT_EQ(steps.back().assertion.statePath, "editor.engine_bridge.identity.stable");
}

TEST(EditorLabCustomizationScenarioTest, CapturesProjectAndUserPrecedence)
{
    const auto steps = MakeCustomizationPrecedenceScenario();
    ASSERT_GE(steps.size(), 12u);

    EXPECT_EQ(steps[0].assertion.statePath,
              "editor.customization.builtin.available");
    EXPECT_EQ(steps[1].assertion.expectedValue, ".sde");
    EXPECT_EQ(steps[2].assertion.expectedValue, "~/.config/Saga");
    EXPECT_EQ(steps[4].assertion.statePath,
              "editor.customization.project.profiles.loaded");
    EXPECT_EQ(steps[5].action.commandId, "saga.command.profile.basic");
    EXPECT_EQ(steps[7].action.commandId,
              "saga.command.profile.advanced_pipeline");
    EXPECT_EQ(steps[11].assertion.statePath,
              "editor.customization.user.override.applied");
    EXPECT_EQ(steps.back().assertion.statePath,
              "editor.core.identity.stable");
}

} // namespace
