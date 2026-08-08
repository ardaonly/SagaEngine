// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Workspaces/UserWorkspace.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace SagaEditor
{
namespace
{
std::filesystem::path TemporaryRoot(const char* name)
{
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
        (std::string(name) + "-" + std::to_string(tick));
    std::filesystem::create_directories(path);
    return path;
}

struct Fixture
{
    Fixture()
    {
        root = TemporaryRoot("saga-user-workspaces");
        EXPECT_TRUE(modes.Init(settings));
        EXPECT_TRUE(service.Init(settings, root, modes.Modes()));
        EXPECT_TRUE(service.ActivateMode(modes.Active()));
    }
    ~Fixture() { std::filesystem::remove_all(root); }

    MemoryEditorSettingsStore settings;
    EditorWorkModeService modes;
    UserWorkspaceService service;
    std::filesystem::path root;
};
} // namespace

TEST(UserWorkspaceTests, ResolvesOfficialDefaultsNamedWorkspaceAndUserOverlay)
{
    Fixture fixture;
    auto defaults = fixture.service.Effective();
    EXPECT_EQ(defaults.identity, "mode:quick-production");
    EXPECT_EQ(defaults.modeId, "quick-production");
    ASSERT_TRUE(fixture.service.SaveAs("Gameplay Focus", {0x12, 0xab}));
    auto named = fixture.service.Effective();
    EXPECT_EQ(named.identity, "gameplay-focus");
    EXPECT_EQ(named.machinePlacementState, "12ab");
    ASSERT_TRUE(fixture.service.CaptureUserOverlay({0x34}));
    EXPECT_TRUE(fixture.service.State().dirty);
    EXPECT_EQ(fixture.service.Effective().machinePlacementState, "34");
    ASSERT_TRUE(fixture.service.SaveCurrent());
    EXPECT_FALSE(fixture.service.State().dirty);
}

TEST(UserWorkspaceTests, CrudAndModeFallbackAreDeterministic)
{
    Fixture fixture;
    ASSERT_TRUE(fixture.service.SaveAs("One", {1}));
    ASSERT_TRUE(fixture.service.Rename("one", "Renamed One"));
    EXPECT_EQ(fixture.service.State().namedWorkspaces[0].displayName,
        "Renamed One");
    ASSERT_TRUE(fixture.service.Delete("one"));
    EXPECT_TRUE(fixture.service.State().currentWorkspaceId.empty());
    EXPECT_EQ(fixture.service.Effective().identity, "mode:quick-production");
    ASSERT_TRUE(fixture.modes.SetActive("full-control"));
    ASSERT_TRUE(fixture.service.ActivateMode(fixture.modes.Active()));
    EXPECT_EQ(fixture.service.Effective().identity, "mode:full-control");
    EXPECT_GE(fixture.service.Effective().docks.size(), 6u);
}

TEST(UserWorkspaceTests, VersionedJsonRoundTripsAndLeavesNoWriteResidue)
{
    Fixture fixture;
    ASSERT_TRUE(fixture.service.SaveAs("Round Trip", {0xde, 0xad}));
    EXPECT_TRUE(std::filesystem::is_regular_file(fixture.service.StoragePath()));
    EXPECT_FALSE(std::filesystem::exists(
        fixture.service.StoragePath().string() + ".tmp"));
    UserWorkspaceService loaded;
    ASSERT_TRUE(loaded.Init(fixture.settings, fixture.root,
        fixture.modes.Modes()));
    ASSERT_TRUE(loaded.ActivateMode(fixture.modes.Active()));
    EXPECT_EQ(loaded.State().currentWorkspaceId, "round-trip");
    EXPECT_EQ(loaded.Effective().machinePlacementState, "dead");
}

TEST(UserWorkspaceTests, CorruptionIsQuarantinedAndDefaultsRecover)
{
    Fixture fixture;
    {
        std::ofstream output(fixture.service.StoragePath());
        output << "{not-json";
    }
    UserWorkspaceService recovered;
    ASSERT_TRUE(recovered.Init(fixture.settings, fixture.root,
        fixture.modes.Modes()));
    ASSERT_TRUE(recovered.ActivateMode(fixture.modes.Active()));
    EXPECT_TRUE(recovered.State().recoveredFromCorruption);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        fixture.service.StoragePath().string() + ".corrupt"));
    EXPECT_EQ(recovered.Effective().identity, "mode:quick-production");
}

TEST(UserWorkspaceTests, FailedAtomicWriteRollsBackMemoryAndLeavesNoResidue)
{
    const auto root = TemporaryRoot("saga-user-workspace-write-failure");
    const auto blockedRoot = root / "not-a-directory";
    {
        std::ofstream output(blockedRoot);
        output << "blocked\n";
    }
    MemoryEditorSettingsStore settings;
    EditorWorkModeService modes;
    ASSERT_TRUE(modes.Init(settings));
    UserWorkspaceService service;
    ASSERT_TRUE(service.Init(settings, blockedRoot, modes.Modes()));
    ASSERT_TRUE(service.ActivateMode(modes.Active()));
    EXPECT_FALSE(service.SaveAs("Must Roll Back", {1, 2, 3}));
    EXPECT_TRUE(service.State().namedWorkspaces.empty());
    EXPECT_TRUE(service.State().currentWorkspaceId.empty());
    EXPECT_FALSE(std::filesystem::exists(
        service.StoragePath().string() + ".tmp"));
    std::filesystem::remove_all(root);
}

TEST(UserWorkspaceTests, AppearanceDoesNotChangeWorkspaceIdentity)
{
    Fixture fixture;
    ASSERT_TRUE(fixture.service.SaveAs("Identity", {1, 2}));
    const auto identity = fixture.service.Effective().identity;
    AppearanceSettings appearance;
    appearance.theme = "Nord";
    appearance.density = "compact";
    appearance.accent = "blue";
    ASSERT_TRUE(fixture.service.SetAppearance(appearance));
    EXPECT_EQ(fixture.service.Effective().identity, identity);
    EXPECT_EQ(fixture.service.Effective().appearance.theme, "Nord");
    UserWorkspaceService loaded;
    ASSERT_TRUE(loaded.Init(fixture.settings, fixture.root,
        fixture.modes.Modes()));
    EXPECT_EQ(loaded.Appearance().density, "compact");
    EXPECT_EQ(loaded.Appearance().accent, "blue");
}

TEST(UserWorkspaceTests, PanelDeviationIsScopedPersistedAndReset)
{
    Fixture fixture;
    ASSERT_TRUE(fixture.service.SetPanelVisible(
        "saga.panel.packages", true));
    EXPECT_TRUE(fixture.service.State().dirty);
    const auto visible = fixture.service.Effective();
    const auto packages = std::find_if(visible.docks.begin(), visible.docks.end(),
        [](const WorkspaceDockDefinition& dock)
        { return dock.panelId == "saga.panel.packages"; });
    ASSERT_NE(packages, visible.docks.end());
    EXPECT_TRUE(packages->visible);
    ASSERT_TRUE(fixture.service.SaveCurrent());

    UserWorkspaceService loaded;
    ASSERT_TRUE(loaded.Init(fixture.settings, fixture.root,
        fixture.modes.Modes()));
    ASSERT_TRUE(loaded.ActivateMode(fixture.modes.Active()));
    const auto persisted = loaded.Effective();
    const auto persistedPackages = std::find_if(persisted.docks.begin(),
        persisted.docks.end(), [](const WorkspaceDockDefinition& dock)
        { return dock.panelId == "saga.panel.packages"; });
    ASSERT_NE(persistedPackages, persisted.docks.end());
    EXPECT_TRUE(persistedPackages->visible);
    ASSERT_TRUE(loaded.ResetModeDefaults());
    const auto reset = loaded.Effective();
    const auto resetPackages = std::find_if(reset.docks.begin(), reset.docks.end(),
        [](const WorkspaceDockDefinition& dock)
        { return dock.panelId == "saga.panel.packages"; });
    ASSERT_NE(resetPackages, reset.docks.end());
    EXPECT_FALSE(resetPackages->visible);
}

TEST(UserWorkspaceTests, RejectsUnboundedAndDuplicateNames)
{
    Fixture fixture;
    EXPECT_FALSE(fixture.service.SaveAs("", {1}));
    ASSERT_TRUE(fixture.service.SaveAs("Same Name", {1}));
    EXPECT_FALSE(fixture.service.SaveAs("Same Name", {2}));
    EXPECT_FALSE(fixture.service.Rename("same-name", std::string(129, 'x')));
    EXPECT_FALSE(fixture.service.SetPanelVisible("plugin.unknown", true));
}
} // namespace SagaEditor
