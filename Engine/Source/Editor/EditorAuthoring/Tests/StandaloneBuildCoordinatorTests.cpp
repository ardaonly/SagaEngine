// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Standalone/StandaloneBuildCoordinator.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <fstream>

namespace SagaEditor::Tests
{
namespace fs = std::filesystem;

class StandaloneBuildCoordinatorTests : public testing::Test
{
protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() / ("saga-standalone-build-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        project = root / "project";
        fs::create_directories(project / "Scripts");
        fs::create_directories(project / "Build/Packages/manifests");
        generation = std::string(64, 'a');
        fs::create_directories(project / "Build/Packages/generations" / generation);
        Write(project / "Game.sagaproj", "{}\n");
        Write(project / "SagaPackage.lock", "{\"schemaVersion\":1}\n");
        Write(project / "BuildProfiles.saga.json", "{\"schemaVersion\":1}\n");
        Write(project / "Scripts/Game.cs", "public class Game {}\n");
        Write(project / "Build/Packages/manifests/current.json",
            nlohmann::json({{"schemaVersion", 1}, {"generationId", generation},
                {"generationHash", std::string(64, 'b')},
                {"graphHash", std::string(64, 'c')}}).dump() + "\n");
        Write(project / "Build/Packages/generations" / generation / "generation.json", "{}\n");
        Write(root / "SagaRuntime", "private-runtime\n");
        const auto stage = root / "managed-stage";
        Write(stage / "managed-tool-runtime.json", R"({"schemaVersion":1})");
        Write(stage / "runtime/dotnet/dotnet", "private-host\n");
        fs::permissions(stage / "runtime/dotnet/dotnet", fs::perms::owner_exec,
            fs::perm_options::add);
        Write(stage / "runtime/dotnet/host/fxr/10.0.10/libhostfxr.so", "fxr\n");
        Write(stage / "runtime/dotnet/shared/Microsoft.NETCore.App/10.0.10/libhostpolicy.so",
            "policy\n");
        for (const auto& [directory, assembly] :
             std::vector<std::pair<std::string, std::string>>{
                 {"SagaProjectKit", "sagaproject"}, {"SagaScript", "sagascript"},
                 {"SagaPackager", "sagapack"}})
        {
            Write(stage / "tools" / directory / (assembly + ".dll"), "assembly\n");
            Write(stage / "tools" / directory / (assembly + ".runtimeconfig.json"), "{}\n");
            Write(stage / "tools" / directory / (assembly + ".deps.json"), "{}\n");
        }

        fence = std::make_shared<const std::atomic_uint64_t>(1);
        request.source.playSessionId = "standalone-test";
        request.source.projectId = "project-id";
        request.source.projectRoot = project;
        request.source.projectPath = project / "Game.sagaproj";
        request.source.projectHash = Hash(project / "Game.sagaproj");
        request.source.trackedLockPath = project / "SagaPackage.lock";
        request.source.trackedLockHash = Hash(project / "SagaPackage.lock");
        request.source.sources.push_back({"script", "game", project / "Scripts/Game.cs",
            "Scripts/Game.cs", Hash(project / "Scripts/Game.cs")});
        request.source.workspaceGeneration = 1;
        request.source.workspaceFence = fence;
        request.buildProfilesPath = project / "BuildProfiles.saga.json";
        request.profileId = "diagnostics";
        request.profileHash = std::string(64, 'd');
        request.runtimeIdentifier = "linux-x64";
        request.runtimeExecutable = root / "SagaRuntime";
        request.outputDirectory = root / "published";
        request.toolchain.engineRoot = root / "repository";
        request.toolchain.managedRuntime.testStage = stage;
    }

    void TearDown() override { std::error_code error; fs::remove_all(root, error); }

    static void Write(const fs::path& path, std::string_view bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << bytes;
    }

    static std::string Hash(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(input), {}};
        return HashDocumentBytes(bytes);
    }

    static std::string Read(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), {}};
    }

    StandaloneBuildExecution Execute(const PlaySourceSnapshot& source,
                                     const fs::path&, const StandaloneBuildRequest&)
    {
        Write(source.projectRoot / "Build/Editor/Play/standalone-test/launch/runtime-launch-v2.json",
            "{\"schemaVersion\":2}\n");
        return {true, false, source.projectRoot / "Build/Editor/Play/standalone-test",
            source.projectRoot / "Build/Editor/Play/standalone-test/launch/runtime-launch-v2.json",
            {}, {{"launchManifest", std::string(64, 'e')}}, {}};
    }

    fs::path root;
    fs::path project;
    std::string generation;
    std::shared_ptr<const std::atomic_uint64_t> fence;
    StandaloneBuildRequest request;
};

TEST_F(StandaloneBuildCoordinatorTests, CapturesMaterializesAndPublishesOneImmutableSnapshot)
{
    std::vector<StandaloneBuildProgress> progress;
    request.progress = [&](const auto& value) { progress.push_back(value); };
    const auto result = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto& source, const auto& candidate, const auto& build)
        { return Execute(source, candidate, build); });

    ASSERT_TRUE(result.Succeeded()) << (result.diagnostics.empty() ? "" :
        result.diagnostics.front().code + ": " + result.diagnostics.front().message);
    EXPECT_EQ(result.snapshot.snapshotId.size(), 64U);
    EXPECT_EQ(result.snapshot.schemaVersion, 2);
    EXPECT_FALSE(result.snapshot.managedRuntimeClosure.entries.empty());
    EXPECT_EQ(result.snapshot.managedRuntimeClosure.hash.size(), 64U);
    EXPECT_TRUE(fs::is_regular_file(result.manifestPath));
    EXPECT_TRUE(fs::is_regular_file(
        request.outputDirectory / "Project/BuildInputSnapshot.json"));
    EXPECT_TRUE(fs::is_regular_file(request.outputDirectory / "bin/SagaRuntime"));
    EXPECT_EQ(progress.back().state, StandaloneBuildState::Succeeded);
    EXPECT_EQ(StandaloneBuildCoordinator::CompareReproducibility(result, result).Name(),
              "byte-reproducible");
}

TEST_F(StandaloneBuildCoordinatorTests, ClassifiesOnlyPublicationMetadataAsSemantic)
{
    request.outputDirectory = root / "published-a";
    const auto first = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto& source, const auto& candidate, const auto& build)
        { return Execute(source, candidate, build); });
    ASSERT_TRUE(first.Succeeded());
    request.outputDirectory = root / "published-b";
    const auto second = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto& source, const auto& candidate, const auto& build)
        { return Execute(source, candidate, build); });
    ASSERT_TRUE(second.Succeeded());
    const auto semantic = StandaloneBuildCoordinator::CompareReproducibility(first, second);
    EXPECT_EQ(semantic.classification,
        ReproducibilityClassification::SemanticContentAddressed);
    ASSERT_FALSE(semantic.differences.empty());
    EXPECT_TRUE(std::all_of(semantic.differences.begin(), semantic.differences.end(),
        [](const auto& difference) { return difference.publicationMetadata; }));

    Write(second.outputDirectory / "Project/Game.sagaproj", "changed\n");
    const auto changed = StandaloneBuildCoordinator::CompareReproducibility(first, second);
    EXPECT_EQ(changed.classification, ReproducibilityClassification::NonReproducible);
}

TEST_F(StandaloneBuildCoordinatorTests, RejectsMixedGenerationBeforeExecutorRuns)
{
    bool executed = false;
    request.captureAttempts = 1;
    request.beforeIdentitySecondRead = [&]
    {
        auto current = nlohmann::json::parse(std::ifstream(
            project / "Build/Packages/manifests/current.json"));
        current["generationHash"] = std::string(64, 'f');
        Write(project / "Build/Packages/manifests/current.json", current.dump() + "\n");
    };
    const auto result = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto&, const auto&, const auto&)
        { executed = true; return StandaloneBuildExecution{}; });
    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(executed);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.back().code, "Build.Snapshot.GenerationChanged");
}

TEST_F(StandaloneBuildCoordinatorTests, RejectsManagedClosureInventoryMutationDuringCapture)
{
    request.beforeIdentitySecondRead = [&]
    { Write(root / "managed-stage/tools/SagaScript/unexpected.asset", "new\n"); };
    const auto result = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto&, const auto&, const auto&)
        { ADD_FAILURE() << "Executor must not run for a mixed closure.";
          return StandaloneBuildExecution{}; });
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.back().code, "Build.Snapshot.ClosureChanged");
}

TEST_F(StandaloneBuildCoordinatorTests, PostCaptureMutationCannotChangeMaterializedBuild)
{
    request.afterCapture = [&] { Write(project / "Scripts/Game.cs", "mutated\n"); };
    bool observedCapturedBytes = false;
    const auto result = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto& source, const auto&, const auto&)
        {
            observedCapturedBytes = Read(source.projectRoot / "Scripts/Game.cs") ==
                "public class Game {}\n";
            return Execute(source, {}, request);
        });
    EXPECT_TRUE(result.Succeeded());
    EXPECT_TRUE(observedCapturedBytes);
    EXPECT_EQ(Read(request.outputDirectory / "Project/Scripts/Game.cs"),
        "public class Game {}\n");
}

TEST_F(StandaloneBuildCoordinatorTests, CancellationAndFailurePreservePreviousOutput)
{
    fs::create_directories(request.outputDirectory);
    Write(request.outputDirectory / "valid.txt", "previous\n");
    request.cancellationRequested = [] { return true; };
    auto cancelled = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto&, const auto&, const auto&) { return StandaloneBuildExecution{}; });
    EXPECT_EQ(cancelled.state, StandaloneBuildState::Cancelled);
    EXPECT_EQ(Hash(request.outputDirectory / "valid.txt"),
              HashDocumentBytes("previous\n"));

    request.cancellationRequested = {};
    auto failed = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto&, const auto&, const auto&)
        {
            StandaloneBuildExecution value;
            value.diagnostics.push_back({"Injected.BuildFailure", "failure", {}, 0, 0});
            return value;
        });
    EXPECT_FALSE(failed.Succeeded());
    EXPECT_TRUE(fs::is_regular_file(request.outputDirectory / "valid.txt"));
}

TEST_F(StandaloneBuildCoordinatorTests, PublicationCancellationRestoresVerifiedPreviousOutput)
{
    const auto first = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto& source, const auto& candidate, const auto& build)
        { return Execute(source, candidate, build); });
    ASSERT_TRUE(first.Succeeded());
    const auto previousManifestHash = Hash(first.manifestPath);
    request.publicationAllowed = [] { return false; };
    const auto cancelled = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto& source, const auto& candidate, const auto& build)
        { return Execute(source, candidate, build); });
    EXPECT_EQ(cancelled.state, StandaloneBuildState::Cancelled);
    EXPECT_TRUE(cancelled.preservedPreviousOutput);
    EXPECT_EQ(cancelled.publicationRecovery.cancellationOutcome,
        StandalonePublicationCancellationOutcome::PreviousOutputRestored);
    EXPECT_EQ(Hash(first.manifestPath), previousManifestHash);
    EXPECT_FALSE(fs::exists(StandalonePublicationTransaction::OwnedTransactionDirectory(
        request.outputDirectory, cancelled.snapshot.snapshotId)));
}

TEST_F(StandaloneBuildCoordinatorTests, RecoversIdempotentlyFromEveryJournalBoundary)
{
    const std::vector<StandalonePublicationBoundary> boundaries{
        StandalonePublicationBoundary::Prepared,
        StandalonePublicationBoundary::PreviousMoveCompleted,
        StandalonePublicationBoundary::PreviousMoved,
        StandalonePublicationBoundary::CandidateMoveCompleted,
        StandalonePublicationBoundary::CandidatePublished,
        StandalonePublicationBoundary::Completed};
    for (std::size_t index = 0; index < boundaries.size(); ++index)
    {
        request.outputDirectory = root / ("interrupted-" + std::to_string(index));
        request.publicationTransitionAllowed = {};
        const auto previous = StandaloneBuildCoordinator{}.Build(request,
            [&](const auto& source, const auto& candidate, const auto& build)
            { return Execute(source, candidate, build); });
        ASSERT_TRUE(previous.Succeeded());
        const auto boundary = boundaries[index];
        request.publicationTransitionAllowed = [boundary](const auto observed)
        { return observed != boundary; };
        const auto interrupted = StandaloneBuildCoordinator{}.Build(request,
            [&](const auto& source, const auto& candidate, const auto& build)
            { return Execute(source, candidate, build); });
        ASSERT_FALSE(interrupted.Succeeded());
        ASSERT_FALSE(interrupted.diagnostics.empty());
        EXPECT_EQ(interrupted.diagnostics.back().code, "Build.Publish.Interrupted");
        const auto recovered = StandalonePublicationTransaction::RecoverOwned(
            request.outputDirectory);
        ASSERT_TRUE(recovered.Succeeded()) <<
            (recovered.diagnostics.empty() ? "" : recovered.diagnostics.front().message);
        EXPECT_TRUE(recovered.recovered);
        EXPECT_TRUE(fs::is_regular_file(
            request.outputDirectory / "StandaloneManifest.saga.json"));
        const auto repeated = StandalonePublicationTransaction::RecoverOwned(
            request.outputDirectory);
        EXPECT_TRUE(repeated.Succeeded());
        EXPECT_FALSE(repeated.recovered);
    }
}

TEST_F(StandaloneBuildCoordinatorTests, MalformedOwnedJournalFailsClosedWithoutMutation)
{
    const auto transaction = StandalonePublicationTransaction::OwnedTransactionDirectory(
        request.outputDirectory, std::string(64, 'f'));
    Write(transaction / "publish-journal.json",
        "{\"schemaVersion\":2,\"candidate\":\"../escaped\"}\n");
    const auto recovery = StandalonePublicationTransaction::RecoverOwned(
        request.outputDirectory);
    EXPECT_FALSE(recovery.Succeeded());
    EXPECT_TRUE(recovery.retainedEvidence);
    EXPECT_TRUE(recovery.terminalFailure);
    EXPECT_TRUE(fs::is_regular_file(transaction / "publish-journal.json"));
}

TEST_F(StandaloneBuildCoordinatorTests, ChangedCandidateIsRetainedAndPreviousOutputStaysIntact)
{
    const auto previous = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto& source, const auto& candidate, const auto& build)
        { return Execute(source, candidate, build); });
    ASSERT_TRUE(previous.Succeeded());
    const auto previousManifest = Hash(previous.manifestPath);
    request.publicationTransitionAllowed = [](const auto boundary)
    { return boundary != StandalonePublicationBoundary::Prepared; };
    const auto interrupted = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto& source, const auto& candidate, const auto& build)
        { return Execute(source, candidate, build); });
    ASSERT_FALSE(interrupted.Succeeded());
    const auto transaction = StandalonePublicationTransaction::OwnedTransactionDirectory(
        request.outputDirectory, interrupted.snapshot.snapshotId);
    Write(transaction / "candidate/tampered.txt", "tampered\n");
    const auto recovery = StandalonePublicationTransaction::RecoverOwned(
        request.outputDirectory);
    EXPECT_FALSE(recovery.Succeeded());
    EXPECT_TRUE(recovery.retainedEvidence);
    EXPECT_EQ(Hash(previous.manifestPath), previousManifest);
}

TEST_F(StandaloneBuildCoordinatorTests, RejectsRootSourceAndSymlinkOutputs)
{
    request.outputDirectory = project;
    auto source = StandaloneBuildCoordinator{}.Build(request);
    ASSERT_FALSE(source.diagnostics.empty());
    EXPECT_EQ(source.diagnostics.front().code, "Build.Output.OverlapsSource");

    std::error_code error;
    fs::create_directory_symlink(root / "elsewhere", root / "link", error);
    if (!error)
    {
        request.outputDirectory = root / "link/output";
        auto symlink = StandaloneBuildCoordinator{}.Build(request);
        ASSERT_FALSE(symlink.diagnostics.empty());
        EXPECT_EQ(symlink.diagnostics.front().code, "Build.Output.SymlinkEscape");
    }
}

TEST_F(StandaloneBuildCoordinatorTests, MissingRuntimeStagingCollisionAndPublishFailureFailClosed)
{
    request.runtimeExecutable = root / "missing-runtime";
    auto missing = StandaloneBuildCoordinator{}.Build(request);
    ASSERT_FALSE(missing.diagnostics.empty());
    EXPECT_EQ(missing.diagnostics.front().code, "Build.Snapshot.RuntimeMissing");

    request.runtimeExecutable = root / "SagaRuntime";
    std::vector<DocumentDiagnostic> diagnostics;
    const auto snapshot = StandaloneBuildCoordinator::Capture(request, diagnostics);
    ASSERT_TRUE(snapshot.has_value());
    const auto collision = StandalonePublicationTransaction::OwnedTransactionDirectory(
        request.outputDirectory, snapshot->snapshotId);
    fs::create_directories(collision);
    auto collided = StandaloneBuildCoordinator{}.Build(request);
    ASSERT_FALSE(collided.diagnostics.empty());
    EXPECT_EQ(collided.diagnostics.front().code,
              "Build.Publish.RecoveryResidueUntrusted");
    EXPECT_TRUE(fs::exists(collision));
    fs::remove_all(collision);

    fs::create_directories(request.outputDirectory);
    Write(request.outputDirectory / "valid.txt", "previous\n");
    request.publicationAllowed = [] { return false; };
    auto failed = StandaloneBuildCoordinator{}.Build(request,
        [&](const auto& source, const auto& candidate, const auto& build)
        { return Execute(source, candidate, build); });
    EXPECT_FALSE(failed.Succeeded());
    EXPECT_FALSE(failed.preservedPreviousOutput);
    ASSERT_FALSE(failed.diagnostics.empty());
    EXPECT_EQ(failed.diagnostics.back().code,
              "Build.Publish.PreviousOutputUnverified");
    EXPECT_EQ(Hash(request.outputDirectory / "valid.txt"),
              HashDocumentBytes("previous\n"));
    EXPECT_FALSE(fs::exists(collision));
}
} // namespace SagaEditor::Tests
