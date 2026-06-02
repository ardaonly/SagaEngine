/// @file SagaStateCheckTests.cpp
/// @brief Verifies deterministic SagaStateCheck snapshot comparison.

#include "SagaStateCheck/StateCheckReport.hpp"
#include "SagaStateCheck/StateSnapshot.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace
{

using SagaStateCheck::BuildStateCheckReport;
using SagaStateCheck::CompareSnapshots;
using SagaStateCheck::DesyncStatus;
using SagaStateCheck::EntitySnapshot;
using SagaStateCheck::StateSnapshot;
using SagaStateCheck::StateVector3;
using SagaStateCheck::WriteStateCheckReportJson;
using SagaStateCheck::WriteStateCheckReportJsonToFile;

EntitySnapshot Entity(std::uint64_t entityId,
                      std::uint64_t ownerClientId,
                      StateVector3 position)
{
    EntitySnapshot snapshot;
    snapshot.entityId = entityId;
    snapshot.ownerClientId = ownerClientId;
    snapshot.position = position;
    return snapshot;
}

StateSnapshot Snapshot(std::string id,
                       std::initializer_list<EntitySnapshot> entities)
{
    StateSnapshot snapshot;
    snapshot.snapshotId = std::move(id);
    snapshot.tick = 17;
    snapshot.entities.assign(entities.begin(), entities.end());
    return snapshot;
}

std::string WriteReportToString(const StateSnapshot& expected,
                                const StateSnapshot& actual)
{
    std::ostringstream output;
    const auto result =
        WriteStateCheckReportJson(BuildStateCheckReport(expected, actual),
                                  output);
    EXPECT_TRUE(result.Succeeded());
    return output.str();
}

std::filesystem::path TestRoot()
{
    return std::filesystem::temp_directory_path() /
           "saga_state_check_tests";
}

} // namespace

TEST(SagaStateCheckTests, MatchingSnapshotsPass)
{
    const auto expected = Snapshot("expected", {
        Entity(1002, 42, {1.0, 2.0, 3.0}),
        Entity(1001, 41, {4.0, 5.0, 6.0}),
    });
    const auto actual = Snapshot("actual", {
        Entity(1001, 41, {4.0, 5.0, 6.0}),
        Entity(1002, 42, {1.0, 2.0, 3.0}),
    });

    const auto report = CompareSnapshots(expected, actual);

    EXPECT_EQ(report.status, DesyncStatus::Passed);
    EXPECT_EQ(report.mismatchCount, 0u);
    EXPECT_TRUE(report.diagnostics.empty());
}

TEST(SagaStateCheckTests, PositionMismatchFails)
{
    const auto expected = Snapshot("expected", {
        Entity(1001, 41, {1.0, 2.0, 3.0}),
    });
    const auto actual = Snapshot("actual", {
        Entity(1001, 41, {1.0, 2.0, 3.25}),
    });

    const auto report = CompareSnapshots(expected, actual);

    EXPECT_EQ(report.status, DesyncStatus::Failed);
    ASSERT_EQ(report.positionMismatches.size(), 1u);
    EXPECT_EQ(report.positionMismatches[0].entityId, 1001u);
    ASSERT_EQ(report.checksumMismatches.size(), 1u);
}

TEST(SagaStateCheckTests, MissingEntityFails)
{
    const auto expected = Snapshot("expected", {
        Entity(1001, 41, {1.0, 2.0, 3.0}),
        Entity(1002, 42, {4.0, 5.0, 6.0}),
    });
    const auto actual = Snapshot("actual", {
        Entity(1001, 41, {1.0, 2.0, 3.0}),
    });

    const auto report = CompareSnapshots(expected, actual);

    EXPECT_EQ(report.status, DesyncStatus::Failed);
    ASSERT_EQ(report.missingEntities.size(), 1u);
    EXPECT_EQ(report.missingEntities[0], 1002u);
}

TEST(SagaStateCheckTests, ExtraEntityFailsDeterministically)
{
    const auto expected = Snapshot("expected", {
        Entity(1002, 42, {4.0, 5.0, 6.0}),
    });
    const auto actual = Snapshot("actual", {
        Entity(1003, 43, {7.0, 8.0, 9.0}),
        Entity(1002, 42, {4.0, 5.0, 6.0}),
    });

    const auto report = CompareSnapshots(expected, actual);

    EXPECT_EQ(report.status, DesyncStatus::Failed);
    ASSERT_EQ(report.extraEntities.size(), 1u);
    EXPECT_EQ(report.extraEntities[0], 1003u);
    ASSERT_FALSE(report.diagnostics.empty());
    EXPECT_EQ(report.diagnostics[0], "extra entity 1003");
}

TEST(SagaStateCheckTests, OwnershipMismatchFails)
{
    const auto expected = Snapshot("expected", {
        Entity(1001, 41, {1.0, 2.0, 3.0}),
    });
    const auto actual = Snapshot("actual", {
        Entity(1001, 99, {1.0, 2.0, 3.0}),
    });

    const auto report = CompareSnapshots(expected, actual);

    EXPECT_EQ(report.status, DesyncStatus::Failed);
    ASSERT_EQ(report.ownershipMismatches.size(), 1u);
    EXPECT_EQ(report.ownershipMismatches[0].expectedOwnerClientId, 41u);
    EXPECT_EQ(report.ownershipMismatches[0].actualOwnerClientId, 99u);
    ASSERT_EQ(report.checksumMismatches.size(), 1u);
}

TEST(SagaStateCheckTests, EmptySnapshotsAreExplicitlyHandled)
{
    const auto report = CompareSnapshots(Snapshot("expected", {}),
                                         Snapshot("actual", {}));

    EXPECT_EQ(report.status, DesyncStatus::Passed);
    EXPECT_EQ(report.mismatchCount, 0u);
    EXPECT_TRUE(report.missingEntities.empty());
    EXPECT_TRUE(report.extraEntities.empty());
}

TEST(SagaStateCheckTests, ReportJsonIsDeterministicAcrossInsertionOrder)
{
    const auto expectedA = Snapshot("expected", {
        Entity(1002, 42, {4.0, 5.0, 6.0}),
        Entity(1001, 41, {1.0, 2.0, 3.0}),
    });
    const auto expectedB = Snapshot("expected", {
        Entity(1001, 41, {1.0, 2.0, 3.0}),
        Entity(1002, 42, {4.0, 5.0, 6.0}),
    });

    EXPECT_EQ(WriteReportToString(expectedA, expectedA),
              WriteReportToString(expectedB, expectedB));
}

TEST(SagaStateCheckTests, StateCheckReportJsonSerializesExpectedShape)
{
    const auto expected = Snapshot("expected", {
        Entity(1001, 41, {1.0, 2.0, 3.0}),
    });
    const auto actual = Snapshot("actual", {
        Entity(1001, 41, {1.0, 2.0, 3.25}),
    });

    const auto json = nlohmann::json::parse(WriteReportToString(expected, actual));

    EXPECT_EQ(json.at("reportKind"), "state_check_report");
    EXPECT_EQ(json.at("desync").at("status"), "Failed");
    EXPECT_EQ(json.at("desync").at("positionMismatches").size(), 1u);
    EXPECT_EQ(json.at("expected").at("entities").at(0).at("entityId"), 1001u);
}

TEST(SagaStateCheckTests, StateCheckReportCanWriteStateCheckReportJsonFile)
{
    const auto path = TestRoot() / "state_check_report.json";
    const auto document = BuildStateCheckReport(
        Snapshot("expected", {Entity(1001, 41, {1.0, 2.0, 3.0})}),
        Snapshot("actual", {Entity(1001, 41, {1.0, 2.0, 3.0})}));

    const auto result = WriteStateCheckReportJsonToFile(document, path);

    ASSERT_TRUE(result.Succeeded());
    std::ifstream input(path);
    ASSERT_TRUE(input.is_open());
    const auto json = nlohmann::json::parse(input);
    EXPECT_EQ(json.at("desync").at("status"), "Passed");
}
