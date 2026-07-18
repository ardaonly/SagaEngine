// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/SceneEditing/SceneDocument.h"
#include "SagaEditor/Viewport/EditorRenderSceneSnapshot.h"

#include <gtest/gtest.h>

TEST(EditorRenderSceneSnapshotTests, BuildsStableHierarchyAndLogicalRenderIdentities)
{
    SagaEditor::SceneDocument scene(std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Samples/StarterArena/Scenes/StarterArena.sagascene");
    ASSERT_TRUE(scene.Load());
    const auto result = SagaEditor::BuildEditorRenderSceneSnapshot(scene);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.snapshot.entities.size(), 3U);
    EXPECT_EQ(result.snapshot.entities[0].sceneEntityId, "door-frame");
    EXPECT_EQ(result.snapshot.entities[1].sceneEntityId, "door-mesh");
    EXPECT_EQ(result.snapshot.entities[1].meshArtifactId, "starter-arena.door.mesh");
    EXPECT_FLOAT_EQ(result.snapshot.entities[1].worldTransform.position.y, 1.0f);
    EXPECT_TRUE(result.snapshot.directionalLight.has_value());
    EXPECT_FALSE(result.snapshot.revisionHash.empty());
}
