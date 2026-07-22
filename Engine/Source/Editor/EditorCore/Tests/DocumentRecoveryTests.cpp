// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Documents/TextDocument.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

namespace
{
namespace fs = std::filesystem;

fs::path Root(std::string_view name)
{
    const auto root = fs::temp_directory_path() / ("saga-recovery-" + std::string(name) + "-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "Scripts");
    std::ofstream(root / "Fixture.sagaproj") << "{}\n";
    std::ofstream(root / "Scripts/Door.cs") << "original\n";
    return root;
}

std::string Read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}

TEST(DocumentRecoveryTests, RestartOffersExactBaseRecoveryWithoutAutomaticOverwrite)
{
    const auto root = Root("restart");
    {
        SagaEditor::TextDocument first("door", SagaEditor::DocumentKind::CSharpSource,
            root / "Scripts/Door.cs", 1, true);
        ASSERT_TRUE(first.Load());
        ASSERT_TRUE(first.ReplaceText("recover me\n"));
        EXPECT_TRUE(fs::is_regular_file(first.Snapshot().recoveryReference));
    }
    SagaEditor::TextDocument restarted("door", SagaEditor::DocumentKind::CSharpSource,
        root / "Scripts/Door.cs", 1, true);
    ASSERT_TRUE(restarted.Load());
    EXPECT_EQ(restarted.Text(), "original\n");
    EXPECT_EQ(restarted.Recovery().status, SagaEditor::DocumentRecoveryStatus::Recoverable);
    ASSERT_TRUE(restarted.RecoverAsDirtyBuffer());
    EXPECT_EQ(restarted.Text(), "recover me\n");
    EXPECT_TRUE(restarted.Snapshot().dirty);
    fs::remove_all(root);
}

TEST(DocumentRecoveryTests, ExternalEditMakesCandidateStaleAndNeverAutoApplies)
{
    const auto root = Root("external");
    {
        SagaEditor::TextDocument first("door", SagaEditor::DocumentKind::CSharpSource,
            root / "Scripts/Door.cs", 1, true);
        ASSERT_TRUE(first.Load());
        ASSERT_TRUE(first.ReplaceText("candidate\n"));
    }
    std::ofstream(root / "Scripts/Door.cs") << "external\n";
    SagaEditor::TextDocument restarted("door", SagaEditor::DocumentKind::CSharpSource,
        root / "Scripts/Door.cs", 1, true);
    ASSERT_TRUE(restarted.Load());
    EXPECT_EQ(restarted.Text(), "external\n");
    EXPECT_EQ(restarted.Recovery().status, SagaEditor::DocumentRecoveryStatus::StaleBase);
    EXPECT_FALSE(restarted.RecoverAsDirtyBuffer());
    EXPECT_EQ(restarted.Text(), "external\n");
    fs::remove_all(root);
}

TEST(DocumentRecoveryTests, PreRenameFaultsPreserveCanonicalBytes)
{
    using Point = SagaEditor::DocumentPersistenceFaultPoint;
    for (const auto point : {Point::TemporaryOpen, Point::TemporaryWrite,
            Point::TemporaryFlush, Point::FileSync, Point::AtomicRename})
    {
        const auto root = Root("fault");
        auto persistence = std::make_shared<SagaEditor::AtomicDocumentPersistence>(
            [point, source = root / "Scripts/Door.cs"](Point current, const fs::path& path)
            {
                return current == point && (path == source || path.filename().string().find("Door.cs.saga-tmp") == 0)
                    ? std::make_error_code(std::errc::io_error) : std::error_code{};
            });
        SagaEditor::TextDocument document("door", SagaEditor::DocumentKind::CSharpSource,
            root / "Scripts/Door.cs", 1, true, {}, persistence);
        ASSERT_TRUE(document.Load());
        ASSERT_TRUE(document.ReplaceText("replacement\n"));
        EXPECT_EQ(document.Save(), SagaEditor::DocumentSaveStatus::WriteFailed);
        EXPECT_EQ(Read(root / "Scripts/Door.cs"), "original\n");
        fs::remove_all(root);
    }
}

TEST(DocumentRecoveryTests, PostRenameSyncAndRecoveryCleanupFaultsKeepNewCanonicalFileValid)
{
    using Point = SagaEditor::DocumentPersistenceFaultPoint;
    for (const auto point : {Point::ParentDirectorySync, Point::RecoveryCleanup})
    {
        const auto root = Root("post-rename");
        auto persistence = std::make_shared<SagaEditor::AtomicDocumentPersistence>(
            [point, source = root / "Scripts/Door.cs"](Point current, const fs::path& path)
            {
                const auto applies = point == Point::ParentDirectorySync
                    ? path == source.parent_path() : path.extension() == ".json";
                return current == point && applies
                    ? std::make_error_code(std::errc::io_error) : std::error_code{};
            });
        SagaEditor::TextDocument document("door", SagaEditor::DocumentKind::CSharpSource,
            root / "Scripts/Door.cs", 1, true, {}, persistence);
        ASSERT_TRUE(document.Load());
        ASSERT_TRUE(document.ReplaceText("replacement\n"));
        EXPECT_EQ(document.Save(), SagaEditor::DocumentSaveStatus::Saved);
        EXPECT_EQ(Read(root / "Scripts/Door.cs"), "replacement\n");
        EXPECT_FALSE(document.Snapshot().dirty);
        fs::remove_all(root);
    }
}

TEST(DocumentRecoveryTests, SuccessfulSaveRemovesObsoleteJournal)
{
    const auto root = Root("cleanup");
    SagaEditor::TextDocument document("door", SagaEditor::DocumentKind::CSharpSource,
        root / "Scripts/Door.cs", 1, true);
    ASSERT_TRUE(document.Load());
    ASSERT_TRUE(document.ReplaceText("saved\n"));
    const auto journal = document.Snapshot().recoveryReference;
    ASSERT_TRUE(fs::exists(journal));
    ASSERT_EQ(document.Save(), SagaEditor::DocumentSaveStatus::Saved);
    EXPECT_FALSE(fs::exists(journal));
    fs::remove_all(root);
}

TEST(DocumentRecoveryTests, InvalidJournalIsTypedAndCanBeDiscarded)
{
    const auto root = Root("invalid");
    fs::path journal;
    {
        SagaEditor::TextDocument first("door", SagaEditor::DocumentKind::CSharpSource,
            root / "Scripts/Door.cs", 1, true);
        ASSERT_TRUE(first.Load());
        ASSERT_TRUE(first.ReplaceText("candidate\n"));
        journal = first.Snapshot().recoveryReference;
    }
    std::ofstream(journal, std::ios::binary | std::ios::trunc) << "not json\n";
    SagaEditor::TextDocument restarted("door", SagaEditor::DocumentKind::CSharpSource,
        root / "Scripts/Door.cs", 1, true);
    ASSERT_TRUE(restarted.Load());
    EXPECT_EQ(restarted.Recovery().status, SagaEditor::DocumentRecoveryStatus::Invalid);
    EXPECT_EQ(restarted.Recovery().diagnostic.code, "Document.RecoveryInvalid");
    EXPECT_TRUE(restarted.DiscardRecovery());
    EXPECT_FALSE(fs::exists(journal));
    fs::remove_all(root);
}

TEST(DocumentRecoveryTests, CandidateAlreadyOnDiskIsTypedObsolete)
{
    const auto root = Root("obsolete");
    {
        SagaEditor::TextDocument first("door", SagaEditor::DocumentKind::CSharpSource,
            root / "Scripts/Door.cs", 1, true);
        ASSERT_TRUE(first.Load());
        ASSERT_TRUE(first.ReplaceText("already saved\n"));
    }
    std::ofstream(root / "Scripts/Door.cs", std::ios::binary | std::ios::trunc)
        << "already saved\n";
    SagaEditor::TextDocument restarted("door", SagaEditor::DocumentKind::CSharpSource,
        root / "Scripts/Door.cs", 1, true);
    ASSERT_TRUE(restarted.Load());
    EXPECT_EQ(restarted.Recovery().status, SagaEditor::DocumentRecoveryStatus::Obsolete);
    EXPECT_EQ(restarted.Text(), "already saved\n");
    EXPECT_FALSE(restarted.Snapshot().dirty);
    fs::remove_all(root);
}

TEST(DocumentRecoveryTests, SameDocumentIdAtDifferentPathsUsesDistinctJournals)
{
    const auto root = Root("path-identity");
    fs::create_directories(root / "Other");
    std::ofstream(root / "Other/Door.cs") << "other\n";
    SagaEditor::TextDocument first("door", SagaEditor::DocumentKind::CSharpSource,
        root / "Scripts/Door.cs", 1, true);
    SagaEditor::TextDocument second("door", SagaEditor::DocumentKind::CSharpSource,
        root / "Other/Door.cs", 1, true);
    ASSERT_TRUE(first.Load());
    ASSERT_TRUE(second.Load());
    EXPECT_NE(first.Snapshot().recoveryReference, second.Snapshot().recoveryReference);
    ASSERT_TRUE(first.ReplaceText("first\n"));
    ASSERT_TRUE(second.ReplaceText("second\n"));
    EXPECT_TRUE(fs::is_regular_file(first.Snapshot().recoveryReference));
    EXPECT_TRUE(fs::is_regular_file(second.Snapshot().recoveryReference));
    fs::remove_all(root);
}
