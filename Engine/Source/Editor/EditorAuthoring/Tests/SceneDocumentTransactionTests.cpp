// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/SceneEditing/SceneDocumentTransaction.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

TEST(SceneDocumentTransactionTests, MutationFlowsThroughSharedUndoJournal)
{
    const auto path = std::filesystem::temp_directory_path() /
        ("saga-scene-transaction-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    std::ofstream(path) <<
        R"({"schemaVersion":1,"sceneId":"main","entities":[{"entityId":"root","name":"Before","components":[{"type":"Transform","position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]}]}]})";
    SagaEditor::SceneDocument document(path);
    ASSERT_TRUE(document.Load());
    SagaEditor::UndoRedoStack journal;
    SagaEditor::SceneDocumentTransaction transaction(document, journal);

    ASSERT_TRUE(transaction.RenameEntity("root", "After"));
    EXPECT_EQ(document.FindEntity("root")->name, "After");
    EXPECT_EQ(journal.GetUndoLabel(), "Rename Entity");
    journal.Undo();
    EXPECT_EQ(document.FindEntity("root")->name, "Before");
    journal.Redo();
    EXPECT_EQ(document.FindEntity("root")->name, "After");
    std::filesystem::remove(path);
}

TEST(SceneDocumentTransactionTests, InvalidMutationNeverEntersJournal)
{
    const auto path = std::filesystem::temp_directory_path() /
        ("saga-scene-invalid-transaction-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    std::ofstream(path) << R"({"schemaVersion":1,"sceneId":"main","entities":[]})";
    SagaEditor::SceneDocument document(path);
    ASSERT_TRUE(document.Load());
    SagaEditor::UndoRedoStack journal;
    SagaEditor::SceneDocumentTransaction transaction(document, journal);

    EXPECT_FALSE(transaction.RenameEntity("missing", "Nope"));
    EXPECT_FALSE(journal.CanUndo());
    EXPECT_FALSE(document.Snapshot().dirty);
    std::filesystem::remove(path);
}
