// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Project/ProjectMutation.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
namespace fs = std::filesystem;

fs::path Root(std::string_view name)
{
    const auto root = fs::temp_directory_path() /
        ("saga-project-mutation-" + std::string(name) + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    std::ofstream(root / "Fixture.sagaproj", std::ios::binary) << "manifest-old\n";
    std::ofstream(root / "SagaPackage.lock", std::ios::binary) << "lock-old\n";
    return root;
}

std::string Read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<SagaEditor::ProjectMutationTransaction::CandidateFile> Candidate()
{
    return {{"Fixture.sagaproj", "manifest-new\n"},
            {"SagaPackage.lock", "lock-new\n"}};
}
}

TEST(ProjectMutationLeaseTests, CanonicalSymlinkAliasContendsWithExistingWriter)
{
    const auto root = Root("alias");
    const auto alias = root.parent_path() / (root.filename().string() + "-alias");
    fs::create_directory_symlink(root, alias);
    {
        auto [first, firstResult] =
            SagaEditor::ProjectMutationLease::Acquire(root, "package");
        ASSERT_TRUE(firstResult.succeeded) << firstResult.message;
        auto [second, secondResult] =
            SagaEditor::ProjectMutationLease::Acquire(alias, "package");
        EXPECT_FALSE(secondResult.succeeded);
        EXPECT_EQ(secondResult.code, "Project.Mutation.LeaseContended");
        EXPECT_EQ(first.ProjectRoot(), fs::canonical(root));
    }
    fs::remove(alias);
    fs::remove_all(root);
}

#ifndef _WIN32
TEST(ProjectMutationLeaseTests, SubprocessDriverClassifiesLiveAndDeadOwners)
{
    const auto liveRoot = Root("subprocess-live");
    {
        auto [parentLease, acquired] =
            SagaEditor::ProjectMutationLease::Acquire(liveRoot, "package");
        ASSERT_TRUE(acquired.succeeded) << acquired.message;
        const auto child = ::fork();
        ASSERT_GE(child, 0);
        if (child == 0)
        {
            auto [childLease, childResult] =
                SagaEditor::ProjectMutationLease::Acquire(liveRoot, "package");
            ::_exit(!childResult.succeeded &&
                childResult.code == "Project.Mutation.LeaseContended" ? 0 : 1);
        }
        int status = 0;
        ASSERT_EQ(::waitpid(child, &status, 0), child);
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0);
    }
    fs::remove_all(liveRoot);

    const auto deadRoot = Root("subprocess-dead");
    const auto child = ::fork();
    ASSERT_GE(child, 0);
    if (child == 0)
    {
        auto [childLease, childResult] =
            SagaEditor::ProjectMutationLease::Acquire(deadRoot, "package");
        ::_exit(childResult.succeeded ? 0 : 2);
    }
    int status = 0;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(WEXITSTATUS(status), 0);
    {
        auto [replacement, replacementResult] =
            SagaEditor::ProjectMutationLease::Acquire(deadRoot, "package");
        EXPECT_TRUE(replacementResult.succeeded) << replacementResult.message;
    }
    fs::remove_all(deadRoot);
}
#endif

TEST(ProjectMutationLeaseTests, IndependentProjectsDoNotContend)
{
    const auto firstRoot = Root("independent-first");
    const auto secondRoot = Root("independent-second");
    {
        auto [first, firstResult] =
            SagaEditor::ProjectMutationLease::Acquire(firstRoot, "package");
        auto [second, secondResult] =
            SagaEditor::ProjectMutationLease::Acquire(secondRoot, "package");
        EXPECT_TRUE(firstResult.succeeded) << firstResult.message;
        EXPECT_TRUE(secondResult.succeeded) << secondResult.message;
    }
    fs::remove_all(firstRoot);
    fs::remove_all(secondRoot);
}

TEST(ProjectMutationTransactionTests, PublishesReviewedManifestThenLockAndCleansJournal)
{
    const auto root = Root("commit");
    {
        auto [lease, acquired] =
            SagaEditor::ProjectMutationLease::Acquire(root, "package");
        ASSERT_TRUE(acquired.succeeded) << acquired.message;
        SagaEditor::ProjectMutationTransaction transaction(
            lease, "enable.instrumentation", "test");
        ASSERT_TRUE(transaction.Stage(Candidate()).succeeded);
        EXPECT_EQ(Read(root / "Fixture.sagaproj"), "manifest-old\n");
        EXPECT_EQ(Read(root / "SagaPackage.lock"), "lock-old\n");
        ASSERT_TRUE(transaction.MarkCommit().succeeded);
        ASSERT_TRUE(transaction.Publish().succeeded);
        EXPECT_EQ(Read(root / "Fixture.sagaproj"), "manifest-new\n");
        EXPECT_EQ(Read(root / "SagaPackage.lock"), "lock-new\n");
        EXPECT_FALSE(fs::exists(transaction.JournalPath()));
    }
    fs::remove_all(root);
}

TEST(ProjectMutationTransactionTests, CompareAndSwapRejectsExternalEditBeforePublication)
{
    const auto root = Root("cas");
    {
        auto [lease, acquired] =
            SagaEditor::ProjectMutationLease::Acquire(root, "package");
        ASSERT_TRUE(acquired.succeeded);
        SagaEditor::ProjectMutationTransaction transaction(
            lease, "disable.instrumentation", "test");
        ASSERT_TRUE(transaction.Stage(Candidate()).succeeded);
        std::ofstream(root / "Fixture.sagaproj", std::ios::binary | std::ios::trunc)
            << "external\n";
        const auto result = transaction.MarkCommit();
        EXPECT_FALSE(result.succeeded);
        EXPECT_EQ(result.code, "Project.Mutation.StaleReview");
        EXPECT_FALSE(fs::exists(transaction.JournalPath()));
        EXPECT_EQ(Read(root / "SagaPackage.lock"), "lock-old\n");
    }
    fs::remove_all(root);
}

TEST(ProjectMutationTransactionTests,
    PublicationTimeDivergencePreservesExternalAndEntersRecoveryRequired)
{
    const auto root = Root("publication-divergence");
    {
        auto [lease, acquired] =
            SagaEditor::ProjectMutationLease::Acquire(root, "package");
        ASSERT_TRUE(acquired.succeeded);
        SagaEditor::ProjectMutationTransaction transaction(
            lease, "disable.instrumentation", "test",
            [&root](const auto point)
            {
                if (point != SagaEditor::ProjectMutationFaultPoint::AfterFirstPublication)
                    return false;
                std::ofstream(root / "SagaPackage.lock",
                    std::ios::binary | std::ios::trunc) << "external-lock\n";
                return false;
            });
        ASSERT_TRUE(transaction.Stage(Candidate()).succeeded);
        ASSERT_TRUE(transaction.MarkCommit().succeeded);
        const auto result = transaction.Publish();
        EXPECT_FALSE(result.succeeded);
        EXPECT_EQ(result.code, "Project.Mutation.ExternalPublicationConflict");
        EXPECT_EQ(transaction.Journal().phase,
                  SagaEditor::ProjectMutationPhase::RecoveryRequired);
        EXPECT_TRUE(fs::exists(transaction.JournalPath()));
        EXPECT_EQ(Read(root / "Fixture.sagaproj"), "manifest-new\n");
        EXPECT_EQ(Read(root / "SagaPackage.lock"), "external-lock\n");
        const auto stagedLock = root / transaction.Journal().files[1].stagedPath;
        EXPECT_EQ(Read(stagedLock), "lock-new\n");
    }
    fs::remove_all(root);
}

TEST(ProjectMutationRecoveryTests, UnmarkedStagingIsDiscardedWithoutCanonicalChange)
{
    const auto root = Root("discard");
    fs::path journal;
    {
        auto [lease, acquired] =
            SagaEditor::ProjectMutationLease::Acquire(root, "package");
        ASSERT_TRUE(acquired.succeeded);
        SagaEditor::ProjectMutationTransaction transaction(
            lease, "enable.instrumentation", "test");
        ASSERT_TRUE(transaction.Stage(Candidate()).succeeded);
        journal = transaction.JournalPath();
        ASSERT_TRUE(fs::exists(journal));
    }
    const auto result = SagaEditor::ProjectMutationRecovery::Recover(root, journal);
    EXPECT_EQ(result.action, SagaEditor::ProjectMutationRecoveryAction::Discarded);
    EXPECT_EQ(Read(root / "Fixture.sagaproj"), "manifest-old\n");
    EXPECT_EQ(Read(root / "SagaPackage.lock"), "lock-old\n");
    fs::remove_all(root);
}

TEST(ProjectMutationRecoveryTests, PartialPublicationRollsForwardRemainingFile)
{
    const auto root = Root("roll-forward");
    fs::path journal;
    {
        auto [lease, acquired] =
            SagaEditor::ProjectMutationLease::Acquire(root, "package");
        ASSERT_TRUE(acquired.succeeded);
        SagaEditor::ProjectMutationTransaction transaction(
            lease, "enable.instrumentation", "test",
            [](const auto point)
            {
                return point == SagaEditor::ProjectMutationFaultPoint::AfterFirstPublication;
            });
        ASSERT_TRUE(transaction.Stage(Candidate()).succeeded);
        ASSERT_TRUE(transaction.MarkCommit().succeeded);
        EXPECT_FALSE(transaction.Publish().succeeded);
        journal = transaction.JournalPath();
        EXPECT_EQ(Read(root / "Fixture.sagaproj"), "manifest-new\n");
        EXPECT_EQ(Read(root / "SagaPackage.lock"), "lock-old\n");
    }
    const auto result = SagaEditor::ProjectMutationRecovery::Recover(root, journal);
    EXPECT_EQ(result.action, SagaEditor::ProjectMutationRecoveryAction::RolledForward);
    EXPECT_EQ(Read(root / "Fixture.sagaproj"), "manifest-new\n");
    EXPECT_EQ(Read(root / "SagaPackage.lock"), "lock-new\n");
    fs::remove_all(root);
}

TEST(ProjectMutationRecoveryTests, SixPointCrashMatrixIsDeterministic)
{
    using Fault = SagaEditor::ProjectMutationFaultPoint;
    using Action = SagaEditor::ProjectMutationRecoveryAction;
    const std::vector<std::pair<Fault, Action>> cases{
        {Fault::AfterStaging, Action::Discarded},
        {Fault::AfterDurableJournal, Action::Discarded},
        {Fault::AfterCommitMarker, Action::RolledForward},
        {Fault::AfterFirstPublication, Action::RolledForward},
        {Fault::AfterSecondPublication, Action::Finalized},
        {Fault::BeforeFinalVerification, Action::Finalized},
    };
    std::size_t caseIndex = 0;
    for (const auto& [fault, expected] : cases)
    {
        const auto root = Root("crash-matrix-" + std::to_string(caseIndex++));
        fs::path journal;
        {
            auto [lease, acquired] =
                SagaEditor::ProjectMutationLease::Acquire(root, "package");
            ASSERT_TRUE(acquired.succeeded) << acquired.message;
            SagaEditor::ProjectMutationTransaction transaction(
                lease, "enable.instrumentation", "test",
                [fault](const auto point) { return point == fault; });
            const auto staged = transaction.Stage(Candidate());
            if (fault == Fault::AfterStaging || fault == Fault::AfterDurableJournal)
                EXPECT_FALSE(staged.succeeded);
            else
            {
                ASSERT_TRUE(staged.succeeded) << staged.message;
                const auto marked = transaction.MarkCommit();
                if (fault == Fault::AfterCommitMarker)
                    EXPECT_FALSE(marked.succeeded);
                else
                {
                    ASSERT_TRUE(marked.succeeded) << marked.message;
                    EXPECT_FALSE(transaction.Publish().succeeded);
                }
            }
            journal = transaction.JournalPath();
            EXPECT_TRUE(fs::exists(journal));
        }
        const auto recovered = SagaEditor::ProjectMutationRecovery::Recover(
            root, journal);
        EXPECT_EQ(recovered.action, expected) << recovered.message;
        EXPECT_EQ(Read(root / "Fixture.sagaproj"),
                  expected == Action::Discarded ? "manifest-old\n" : "manifest-new\n");
        EXPECT_EQ(Read(root / "SagaPackage.lock"),
                  expected == Action::Discarded ? "lock-old\n" : "lock-new\n");
        EXPECT_FALSE(fs::exists(journal));
        fs::remove_all(root);
    }
}

TEST(ProjectMutationRecoveryTests, DivergentCanonicalBytesArePreservedForInspection)
{
    const auto root = Root("divergence");
    fs::path journal;
    {
        auto [lease, acquired] =
            SagaEditor::ProjectMutationLease::Acquire(root, "package");
        ASSERT_TRUE(acquired.succeeded);
        SagaEditor::ProjectMutationTransaction transaction(
            lease, "enable.instrumentation", "test",
            [](const auto point)
            {
                return point == SagaEditor::ProjectMutationFaultPoint::AfterCommitMarker;
            });
        ASSERT_TRUE(transaction.Stage(Candidate()).succeeded);
        EXPECT_FALSE(transaction.MarkCommit().succeeded);
        journal = transaction.JournalPath();
    }
    std::ofstream(root / "SagaPackage.lock", std::ios::binary | std::ios::trunc)
        << "unreviewed-third-version\n";
    const auto result = SagaEditor::ProjectMutationRecovery::Recover(root, journal);
    EXPECT_EQ(result.action,
              SagaEditor::ProjectMutationRecoveryAction::RecoveryRequired);
    EXPECT_EQ(result.code, "Project.Mutation.RecoveryDivergence");
    EXPECT_EQ(Read(root / "SagaPackage.lock"), "unreviewed-third-version\n");
    EXPECT_TRUE(fs::exists(journal));
    fs::remove_all(root);
}

TEST(ProjectMutationRecoveryTests, NewerSchemaIsPreservedAndFailsClosed)
{
    const auto root = Root("newer-schema");
    const auto directory = root / ".saga/local/transactions/package/future";
    fs::create_directories(directory);
    const auto journal = directory / "journal.v1.json";
    std::ofstream(journal) << "{\"schemaVersion\":"
        << SagaEditor::ProjectMutationJournal::CurrentSchemaVersion + 1 << "}\n";

    const auto result = SagaEditor::ProjectMutationRecovery::Recover(root, journal);

    EXPECT_EQ(result.action,
              SagaEditor::ProjectMutationRecoveryAction::UnsupportedJournal);
    EXPECT_EQ(result.code, "Project.Mutation.UnsupportedJournal");
    EXPECT_TRUE(fs::exists(journal));
    fs::remove_all(root);
}
