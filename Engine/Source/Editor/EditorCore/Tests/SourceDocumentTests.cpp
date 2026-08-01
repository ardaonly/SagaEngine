// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Documents/SourceDocuments.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
std::filesystem::path TemporaryFile(std::string_view name, std::string_view bytes)
{
    const auto path = std::filesystem::temp_directory_path() /
        ("saga-" + std::string(name) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::ofstream output(path, std::ios::binary);
    output << bytes;
    return path;
}
}

TEST(SourceDocumentTests, CSharpDirtyBufferSurvivesInvalidMutationAndUndo)
{
    const auto path = TemporaryFile("script.cs", "public class Door {}\n");
    SagaEditor::CSharpSourceDocument document(path);
    ASSERT_TRUE(document.Load());
    ASSERT_TRUE(document.ReplaceText("public class Door { public void Open() {} }\n"));
    EXPECT_FALSE(document.ReplaceText(std::string("bad\0source", 10)));
    EXPECT_TRUE(document.Snapshot().dirty);
    EXPECT_TRUE(document.Undo());
    EXPECT_EQ(document.Text(), "public class Door {}\n");
    std::filesystem::remove(path);
}

TEST(SourceDocumentTests, InputMapRejectsDuplicateActionsAndBindings)
{
    const auto path = TemporaryFile("input.json", R"({"schemaVersion":1,"inputMapId":"game","actions":[]})");
    SagaEditor::InputMapDocument document(path);
    ASSERT_TRUE(document.Load());
    EXPECT_FALSE(document.ReplaceText(R"({"schemaVersion":1,"inputMapId":"game","actions":[{"actionId":"Interact","displayName":"Interact","runtimeRole":"Gameplay","bindings":[]},{"actionId":"Interact","displayName":"Again","runtimeRole":"Gameplay","bindings":[]}]})"));
    EXPECT_EQ(document.Diagnostics().front().code, "Input.ActionDuplicate");
    EXPECT_FALSE(document.Snapshot().dirty);
    std::filesystem::remove(path);
}

TEST(SourceDocumentTests, ProjectDocumentParticipatesInExplicitSave)
{
    const auto path = TemporaryFile("project.sagaproj", "{}\n");
    SagaEditor::ProjectDocument document(path);
    ASSERT_TRUE(document.Load());
    EXPECT_EQ(document.Snapshot().kind, SagaEditor::DocumentKind::Project);
    EXPECT_TRUE(document.Snapshot().canSave);
    ASSERT_TRUE(document.ReplaceText("{\"changed\":true}\n"));
    EXPECT_TRUE(document.Snapshot().dirty);
    EXPECT_EQ(document.Save(), SagaEditor::DocumentSaveStatus::Saved);
    EXPECT_FALSE(document.Snapshot().dirty);
    std::filesystem::remove(path);
}
