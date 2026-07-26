// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Documents/TextDocument.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

namespace
{
std::filesystem::path TempFile(const char* name)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto root = std::filesystem::temp_directory_path() /
        (std::string("saga_document_") + name + "_" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    return root / "source.txt";
}

void Write(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}
}

TEST(DocumentTests, TracksDirtyUndoRedoAndAtomicSave)
{
    const auto path = TempFile("journal");
    Write(path, "closed\n");
    SagaEditor::TextDocument document("door", SagaEditor::DocumentKind::CSharpSource,
                                      path, 1, true);
    ASSERT_TRUE(document.Load());
    ASSERT_TRUE(document.ReplaceText("open\n"));
    EXPECT_TRUE(document.Snapshot().dirty);
    EXPECT_EQ(document.RequestClose(), SagaEditor::DocumentCloseStatus::RequiresDecision);
    ASSERT_TRUE(document.Undo());
    EXPECT_EQ(document.Text(), "closed\n");
    ASSERT_TRUE(document.Redo());
    EXPECT_EQ(document.Text(), "open\n");
    EXPECT_EQ(document.Save(), SagaEditor::DocumentSaveStatus::Saved);
    std::error_code error;
    EXPECT_EQ(SagaEditor::ReadDocumentBytes(path, error), "open\n");
    EXPECT_FALSE(error);
}

TEST(DocumentTests, ExternalEditBlocksDirtySaveAndPreservesBuffer)
{
    const auto path = TempFile("conflict");
    Write(path, "base\n");
    SagaEditor::TextDocument document("script", SagaEditor::DocumentKind::CSharpSource,
                                      path, 1, true);
    ASSERT_TRUE(document.Load());
    ASSERT_TRUE(document.ReplaceText("memory\n"));
    Write(path, "external\n");
    EXPECT_EQ(document.Save(), SagaEditor::DocumentSaveStatus::Conflict);
    EXPECT_EQ(document.Text(), "memory\n");
    EXPECT_TRUE(document.Snapshot().dirty);
}

TEST(DocumentTests, InvalidMutationDoesNotCorruptCurrentText)
{
    const auto path = TempFile("validation");
    Write(path, "valid");
    SagaEditor::TextDocument document(
        "input", SagaEditor::DocumentKind::InputMap, path, 1, true,
        [path](const std::string& text)
        {
            if (text == "invalid")
                return std::vector<SagaEditor::DocumentDiagnostic>{{"Input.Invalid", "invalid", path}};
            return std::vector<SagaEditor::DocumentDiagnostic>{};
        });
    ASSERT_TRUE(document.Load());
    EXPECT_FALSE(document.ReplaceText("invalid"));
    EXPECT_EQ(document.Text(), "valid");
    EXPECT_FALSE(document.Snapshot().dirty);
}
