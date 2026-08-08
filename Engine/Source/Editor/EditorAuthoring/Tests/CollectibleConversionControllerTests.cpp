// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Performance/CollectibleConversionController.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

namespace SagaEditor::Tests
{
namespace fs = std::filesystem;

namespace
{
std::string Read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void Write(const fs::path& path, std::string_view bytes)
{
    std::ofstream(path, std::ios::binary) << bytes;
}

constexpr std::string_view HighSource = R"(class Fixture
{
// SAGA-COLLECTIBLE-SURFACE-BEGIN
void Update() { CollectiblesHighLevel.AdvanceRespawnTime(state, 0.5); }
// SAGA-COLLECTIBLE-SURFACE-END
}
)";
constexpr std::string_view LowSource = R"(class Fixture
{
// SAGA-COLLECTIBLE-SURFACE-BEGIN
void Update() { CollectiblesLowLevelBatch.Advance(0.5, 0); }
// SAGA-COLLECTIBLE-SURFACE-END
}
)";
constexpr std::string_view HighProject = R"({"projectId":"fixture","dependencies":[{"packageId":"com.saga.gameplay.challenge","version":"0.0.11","modules":["collectibles.highlevel"]}]})";
constexpr std::string_view LowProject = R"({"projectId":"fixture","dependencies":[{"packageId":"com.saga.gameplay.challenge","version":"0.0.11","modules":["collectibles.lowlevel"]}]})";
constexpr std::string_view HighLock = R"({"schemaVersion":1,"projectId":"fixture","graphHash":"sha256:high","packages":[{"packageId":"com.saga.gameplay.challenge","version":"0.0.11","modules":["collectibles.highlevel"]}]})";
constexpr std::string_view LowLock = R"({"schemaVersion":1,"projectId":"fixture","graphHash":"sha256:low","packages":[{"packageId":"com.saga.gameplay.challenge","version":"0.0.11","modules":["collectibles.lowlevel"]}]})";
}

class CollectibleConversionControllerTests : public testing::Test
{
protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() / ("saga-conversion-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root / "Scripts");
        Write(root / "Fixture.sagaproj", HighProject);
        Write(root / "SagaPackage.lock", HighLock);
        Write(root / "Scripts/Collectibles.cs", HighSource);
        Write(root / "CollectibleConversion.saga.json",
            "{\n  \"schemaVersion\": 1,\n  \"state\": \"HighLevel\"\n}\n");
    }
    void TearDown() override { fs::remove_all(root); }
    CollectibleConversionCapture Capture() const
    {
        return {root, root / "Fixture.sagaproj", root / "SagaPackage.lock",
            root / "Scripts/Collectibles.cs", root / "CollectibleConversion.saga.json",
            "generation-1", "sha256:high", "revision-7"};
    }
    fs::path root;
};

TEST_F(CollectibleConversionControllerTests, AppliesAllCandidatesAndReversesFromExactProvenance)
{
    CollectibleConversionController controller;
    const auto prepared = controller.Prepare(
        Capture(), std::string(LowProject), std::string(LowLock), std::string(LowSource), "0.0.11");
    ASSERT_EQ(prepared.state, CollectibleConversionState::AwaitingReview);
    ASSERT_TRUE(prepared.plan.has_value());
    EXPECT_EQ(prepared.plan->supportedRegionChecksums.size(), 2U);
    EXPECT_NE(prepared.plan->deterministicDiff.find("collectibles.lowlevel"), std::string::npos);
    EXPECT_EQ(controller.Apply().state, CollectibleConversionState::Applied);
    EXPECT_EQ(Read(root / "Fixture.sagaproj"), LowProject);
    EXPECT_EQ(Read(root / "SagaPackage.lock"), LowLock);
    EXPECT_EQ(Read(root / "Scripts/Collectibles.cs"), LowSource);
    EXPECT_EQ(controller.Reverse(Capture()).state, CollectibleConversionState::Reversed);
    EXPECT_EQ(Read(root / "Fixture.sagaproj"), HighProject);
    EXPECT_EQ(Read(root / "SagaPackage.lock"), HighLock);
    EXPECT_EQ(Read(root / "Scripts/Collectibles.cs"), HighSource);
}

TEST_F(CollectibleConversionControllerTests, RejectsStaleReviewWithoutPublishingAnyCandidate)
{
    CollectibleConversionController controller;
    ASSERT_EQ(controller.Prepare(Capture(), std::string(LowProject), std::string(LowLock),
        std::string(LowSource), "0.0.11").state,
        CollectibleConversionState::AwaitingReview);
    Write(root / "Scripts/Collectibles.cs", "custom source\n");
    EXPECT_EQ(controller.Apply().state, CollectibleConversionState::Rejected);
    EXPECT_EQ(controller.State(), CollectibleConversionState::Rejected);
    EXPECT_EQ(Read(root / "Fixture.sagaproj"), HighProject);
    EXPECT_EQ(Read(root / "SagaPackage.lock"), HighLock);
    EXPECT_EQ(Read(root / "Scripts/Collectibles.cs"), "custom source\n");
}

TEST_F(CollectibleConversionControllerTests, DivergenceDetachesAndNeverGuessesCustomSource)
{
    CollectibleConversionController controller;
    ASSERT_EQ(controller.Prepare(Capture(), std::string(LowProject), std::string(LowLock),
        std::string(LowSource), "0.0.11").state,
        CollectibleConversionState::AwaitingReview);
    ASSERT_EQ(controller.Apply().state, CollectibleConversionState::Applied);
    Write(root / "Scripts/Collectibles.cs", "custom low-level source\n");
    const auto reversed = controller.Reverse(Capture());
    EXPECT_EQ(reversed.state, CollectibleConversionState::DetachedRequiresReview);
    EXPECT_EQ(reversed.code, "Conversion.Detached");
    EXPECT_EQ(Read(root / "Scripts/Collectibles.cs"), "custom low-level source\n");
}

TEST_F(CollectibleConversionControllerTests, RejectsUnsupportedCandidateAndMissingIdentities)
{
    CollectibleConversionController controller;
    auto capture = Capture();
    capture.projectGeneration.clear();
    EXPECT_EQ(controller.Prepare(capture, std::string(LowProject), std::string(LowLock),
        "class Custom {}\n", "0.0.11").state,
        CollectibleConversionState::Rejected);
    EXPECT_EQ(Read(root / "Scripts/Collectibles.cs"), HighSource);
}

TEST_F(CollectibleConversionControllerTests, RejectsUnverifiedCandidateLock)
{
    CollectibleConversionController controller;
    const auto result = controller.Prepare(Capture(), std::string(LowProject),
        R"({"schemaVersion":1,"projectId":"fixture","graphHash":"","packages":[]})",
        std::string(LowSource), "0.0.11");
    EXPECT_EQ(result.state, CollectibleConversionState::Rejected);
    EXPECT_EQ(result.code, "Conversion.PackageOwnershipInvalid");
    EXPECT_EQ(Read(root / "Fixture.sagaproj"), HighProject);
    EXPECT_EQ(Read(root / "SagaPackage.lock"), HighLock);
}

TEST_F(CollectibleConversionControllerTests, RejectsCandidateForAnotherProject)
{
    CollectibleConversionController controller;
    const auto otherLock = R"({"schemaVersion":1,"projectId":"other",
      "graphHash":"sha256:low","packages":[{
      "packageId":"com.saga.gameplay.challenge","version":"0.0.11",
      "modules":["collectibles.lowlevel"]}]})";
    const auto result = controller.Prepare(Capture(), std::string(LowProject),
        otherLock, std::string(LowSource), "0.0.11");
    EXPECT_EQ(result.state, CollectibleConversionState::Rejected);
    EXPECT_EQ(result.code, "Conversion.PackageIdentityMismatch");
    EXPECT_EQ(Read(root / "Fixture.sagaproj"), HighProject);
}
}
