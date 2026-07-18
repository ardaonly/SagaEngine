// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/SceneEditing/SceneSelectionIdentity.h"

#include <gtest/gtest.h>

TEST(SceneSelectionIdentityTests, IdentityIsStableAndSceneScoped)
{
    const auto first = SagaEditor::SceneSelectionId("scene-a", "door-root");
    EXPECT_EQ(first, SagaEditor::SceneSelectionId("scene-a", "door-root"));
    EXPECT_NE(first, SagaEditor::SceneSelectionId("scene-a", "ground"));
    EXPECT_NE(first, SagaEditor::SceneSelectionId("scene-b", "door-root"));
    EXPECT_NE(first, SagaEditor::kInvalidSelectableId);
}
