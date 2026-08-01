// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Packages/PackageAuthoring.h"
#include "SagaEditor/Packages/PackagesPanelViewModel.h"

#include <gtest/gtest.h>

namespace
{
const char* Inspection = R"({
  "schemaVersion":1,"status":"Passed","offline":true,
  "graphHash":"sha256:graph","generationId":"generation",
  "modules":[
    {"packageId":"challenge","version":"1","module":"core","enabled":true,
     "runtimeReadiness":"Ready","artifactKinds":["apiSurface"],
     "apiSurfaces":["challenge/core"],"blocksSurfaces":["challenge/core"],
     "capabilities":["Gameplay"],"license":"MPL-2.0",
     "trust":"TrustedEngineDistribution","origin":"built-in","offline":true},
    {"packageId":"challenge","version":"1","module":"instrumentation","enabled":false,
     "runtimeReadiness":"Ready","artifactKinds":["apiSurface"],
     "apiSurfaces":["challenge/instrumentation"],"blocksSurfaces":[],
     "capabilities":["DevelopmentDiagnostics"],"license":"MPL-2.0",
     "trust":"TrustedEngineDistribution","origin":"built-in","offline":true}
  ]})";
}

TEST(PackagesPanelViewModelTests, ProjectsVerifiedRowsAndReadinessActions)
{
    SagaEditor::PackageStateCatalog catalog;
    std::string diagnostic;
    ASSERT_TRUE(catalog.LoadInspection(Inspection, diagnostic)) << diagnostic;
    SagaEditor::ProjectReadinessSession readiness;
    SagaEditor::PackagesPanelViewModel model;
    model.Refresh(catalog, readiness);
    ASSERT_EQ(model.Rows().size(), 2);
    EXPECT_EQ(model.Rows()[0].state, "Enabled");
    EXPECT_TRUE(model.Rows()[0].canDisable);
    EXPECT_EQ(model.Rows()[1].state, "Available");
    EXPECT_TRUE(model.Rows()[1].canEnable);
    EXPECT_EQ(model.Rows()[1].capabilities, "DevelopmentDiagnostics");

    readiness.SetToolingOutOfDate(true);
    model.Refresh(catalog, readiness);
    EXPECT_TRUE(model.BusyOrUnavailable());
    EXPECT_FALSE(model.Rows()[0].canDisable);
    EXPECT_FALSE(model.Rows()[1].canEnable);
}
