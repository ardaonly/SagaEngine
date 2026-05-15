/// @file EditorProductionTests.cpp
/// @brief Production foundation tests for SagaEditor startup boundaries.

#include "SagaEditor/Customization/EditorCustomizationCatalog.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Profile/EditorProfile.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Runtime/IEditorEngineBridge.h"
#include "SagaEditor/Selection/SelectionManager.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#ifndef SAGA_SOURCE_ROOT
#define SAGA_SOURCE_ROOT "."
#endif

namespace
{

using namespace SagaEditor;

namespace fs = std::filesystem;

[[nodiscard]] bool Contains(const std::vector<std::string>& values,
                            const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

TEST(EditorProductionHostTest, EngineBridgeStartsReadyBehindEditorBoundary)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));

    const IEditorEngineBridge& bridge = host.GetEngineBridge();
    const EditorEngineBridgeSnapshot snapshot = bridge.Snapshot();

    EXPECT_EQ(bridge.StableIdentity(), "saga.editor.engine_bridge.local");
    EXPECT_EQ(snapshot.state, EditorEngineBridgeState::Ready);
    EXPECT_EQ(snapshot.displayName, "Local Engine Bridge");
    EXPECT_FALSE(snapshot.runtimeRole.empty());
    EXPECT_FALSE(snapshot.engineVersion.empty());

    host.Shutdown();
}

TEST(EditorProductionHostTest, ProfileSwitchDoesNotReplaceCoreOrEngineBridge)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));

    auto* selectionBefore = &host.GetSelectionManager();
    const std::string bridgeIdentityBefore =
        host.GetEngineBridge().StableIdentity();

    ASSERT_TRUE(host.GetEditorProfileRegistry().SetActive(
        "saga.profile.advanced_pipeline"));
    ASSERT_TRUE(host.GetEditorProfileRegistry().SetActive(
        "saga.profile.standard_pipeline"));

    EXPECT_EQ(selectionBefore, &host.GetSelectionManager());
    EXPECT_EQ(bridgeIdentityBefore, host.GetEngineBridge().StableIdentity());
    EXPECT_EQ(host.GetEngineBridge().Snapshot().state,
              EditorEngineBridgeState::Ready);

    host.Shutdown();
}

TEST(EditorProductionHostTest, BuiltinProfilesExposeProductionDashboard)
{
    const EditorProfile profiles[] = {
        MakeBasicProfile(),
        MakeNodeEditorProfile(),
        MakeStandardPipelineProfile(),
        MakeAdvancedPipelineProfile(),
        MakeCustomProfile(),
    };

    for (const EditorProfile& profile : profiles)
    {
        EXPECT_TRUE(Contains(profile.defaultPanels,
                             "saga.panel.production_dashboard"))
            << profile.id;
    }
}

TEST(EditorCustomizationCatalogTest, DefaultsToBuiltinsWhenSdeUnavailable)
{
    EditorCustomizationCatalog catalog;
    ASSERT_TRUE(catalog.Init({}));

    const EditorCustomizationStatus& status = catalog.Status();
#if SAGA_WITH_SDE
    EXPECT_TRUE(status.sdeAvailable);
#else
    EXPECT_FALSE(status.sdeAvailable);
    EXPECT_FALSE(status.loaded);
    EXPECT_NE(status.message.find("built-in"), std::string::npos);
#endif
    EXPECT_FALSE(status.sourceRoot.empty());
}

TEST(EditorCustomizationCatalogTest, LoadsProjectSdeProfilesWhenAvailable)
{
    EditorCustomizationCatalog catalog;
    const fs::path workspaceRoot =
        fs::path(SAGA_SOURCE_ROOT) / "Apps" / "Saga" /
        "Definitions" / "BasicWorkspace";

    ASSERT_TRUE(catalog.Init(workspaceRoot));

    const EditorCustomizationStatus& status = catalog.Status();
    EXPECT_EQ(status.sourceRoot.filename(), ".sde");
    EXPECT_FALSE(status.userConfigRoot.empty());

#if SAGA_WITH_SDE
    ASSERT_FALSE(catalog.Snapshot().projectProfiles.empty());
    EXPECT_TRUE(Contains(
        [&catalog]()
        {
            std::vector<std::string> ids;
            for (const EditorProfile& profile :
                 catalog.Snapshot().projectProfiles)
            {
                ids.push_back(profile.id);
            }
            return ids;
        }(),
        "saga.profile.project_basic"));
#else
    EXPECT_TRUE(catalog.Snapshot().projectProfiles.empty());
#endif
}

} // namespace
