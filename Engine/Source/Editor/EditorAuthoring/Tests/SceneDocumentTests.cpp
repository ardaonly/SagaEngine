// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/SceneEditing/SceneDocument.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <limits>

#ifndef SAGA_SOURCE_ROOT
#define SAGA_SOURCE_ROOT "."
#endif

TEST(SceneDocumentTests, OpensStarterArenaWithStableHierarchyAndComponents)
{
    const auto path = std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Samples/StarterArena/Scenes/StarterArena.sagascene";
    SagaEditor::SceneDocument document(path);
    ASSERT_TRUE(document.Load());
    EXPECT_EQ(document.SceneId(), "starter-arena.scene");
    ASSERT_EQ(document.Entities().size(), 8U);
    const auto* door = document.FindEntity("door-root");
    ASSERT_NE(door, nullptr);
    ASSERT_TRUE(door->parentEntityId.has_value());
    EXPECT_EQ(*door->parentEntityId, "starter-arena");
    EXPECT_TRUE(door->transform.has_value());
}

TEST(SceneDocumentTests, InvalidTransformMutationDoesNotChangeDocument)
{
    const auto source = std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Samples/StarterArena/Scenes/StarterArena.sagascene";
    const auto temp = std::filesystem::temp_directory_path() / "saga_scene_document_invalid.sagascene";
    std::filesystem::copy_file(source, temp, std::filesystem::copy_options::overwrite_existing);
    SagaEditor::SceneDocument document(temp);
    ASSERT_TRUE(document.Load());
    const auto revision = document.Snapshot().inMemoryRevision;
    auto transform = *document.FindEntity("door-root")->transform;
    transform.position[0] = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(document.SetTransform("door-root", transform));
    EXPECT_EQ(document.Snapshot().inMemoryRevision, revision);
    std::filesystem::remove(temp);
}

TEST(SceneDocumentTests, RenameAndTransformParticipateInUndo)
{
    const auto source = std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Samples/StarterArena/Scenes/StarterArena.sagascene";
    const auto temp = std::filesystem::temp_directory_path() / "saga_scene_document_undo.sagascene";
    std::filesystem::copy_file(source, temp, std::filesystem::copy_options::overwrite_existing);
    SagaEditor::SceneDocument document(temp);
    ASSERT_TRUE(document.Load());
    ASSERT_TRUE(document.RenameEntity("door-root", "Entry Door"));
    EXPECT_EQ(document.FindEntity("door-root")->name, "Entry Door");
    ASSERT_TRUE(document.Undo());
    EXPECT_EQ(document.FindEntity("door-root")->name, "DoorRoot");
    std::filesystem::remove(temp);
}
