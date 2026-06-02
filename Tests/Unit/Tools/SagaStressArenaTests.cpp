/// @file SagaStressArenaTests.cpp
/// @brief Verifies bounded SagaStressArena diagnostics artifact scenarios.

#include "SagaStressArena/ScenarioRunner.hpp"

#include "SagaEngine/Input/Commands/InputCommandSerializer.h"
#include "SagaServer/Networking/Server/ZoneServer.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace
{

using SagaEngine::Input::FixedFromFloat;
using SagaEngine::Input::InputCommand;
using SagaEngine::Input::InputCommandSerializer;
using SagaEngine::Input::MakeInputCommand;
using SagaEngine::Networking::ClientId;
using SagaEngine::Networking::PacketType;
using SagaEngine::Server::Simulation::ActorOwnershipResult;
using SagaEngine::Server::Simulation::AuthoritativeMovementDecision;
using SagaEngine::Server::Simulation::EntityId;
using SagaEngine::Server::Simulation::Vector3;
using SagaServer::Networking::ServerPacketNormalizationStatus;
using SagaServer::Networking::ZoneServer;
using SagaServer::Networking::ZoneServerConfig;

std::filesystem::path TestRoot()
{
    return std::filesystem::temp_directory_path() /
           "saga_stress_arena_tests";
}

SagaStressArena::StressScenarioConfig SmokeConfig(
    const std::filesystem::path& reportDirectory)
{
    auto config = SagaStressArena::DefaultStressScenarioConfig();
    config.actors = 3;
    config.ticks = 10;
    config.maxDurationSec = 10;
    config.reportDirectory = reportDirectory;
    return config;
}

SagaStressArena::StressScenarioConfig PacketChaosSmokeConfig(
    const std::filesystem::path& reportDirectory)
{
    auto config = SmokeConfig(reportDirectory);
    config.scenario = SagaStressArena::kDirectZonePacketChaosSmokeScenario;
    return config;
}

nlohmann::json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path);
    EXPECT_TRUE(input.is_open());
    return nlohmann::json::parse(input);
}

std::optional<double> FindMetric(const nlohmann::json& report,
                                 const std::string& name)
{
    for (const auto& metric : report.at("healthMetrics"))
    {
        if (metric.at("name") == name)
        {
            return metric.at("value").get<double>();
        }
    }
    return std::nullopt;
}

std::map<std::string, double> MetricMap(const nlohmann::json& report)
{
    std::map<std::string, double> metrics;
    for (const auto& metric : report.at("healthMetrics"))
    {
        metrics[metric.at("name").get<std::string>()] =
            metric.at("value").get<double>();
    }
    return metrics;
}

std::vector<std::string> HealthRuleViolationNames(const nlohmann::json& report)
{
    std::vector<std::string> names;
    for (const auto& violation : report.at("healthRuleViolations"))
    {
        names.push_back(violation.at("ruleName").get<std::string>());
    }
    return names;
}

std::vector<std::uint8_t> MakeFrame(PacketType packetType,
                                    const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> frame(
        SagaServer::Networking::kServerPacketFrameHeaderSize + payload.size());

    const std::uint16_t magic =
        SagaServer::Networking::kServerPacketFrameMagic;
    const std::uint32_t bodyLength =
        static_cast<std::uint32_t>(payload.size());

    std::memcpy(frame.data(), &magic, sizeof(magic));
    frame[2] = static_cast<std::uint8_t>(packetType);
    frame[3] = 0;
    std::memcpy(frame.data() + 4, &bodyLength, sizeof(bodyLength));
    if (!payload.empty())
    {
        std::memcpy(
            frame.data() + SagaServer::Networking::kServerPacketFrameHeaderSize,
            payload.data(),
            payload.size());
    }

    return frame;
}

InputCommand NetworkCommand(std::uint32_t sequence,
                            float moveX,
                            float moveY,
                            std::uint32_t simulationTick = 1)
{
    auto command = MakeInputCommand(sequence, simulationTick, 1234);
    command.moveX = FixedFromFloat(moveX);
    command.moveY = FixedFromFloat(moveY);
    return command;
}

std::vector<std::uint8_t> SerializeCommand(const InputCommand& command)
{
    const auto bytes = InputCommandSerializer::Serialize(command);
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

ZoneServerConfig TestConfig()
{
    ZoneServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 0;
    config.zoneId = 52;
    config.zoneName = "saga-stress-arena-test";
    return config;
}

} // namespace

TEST(SagaStressArenaTests, DefaultConfigUsesSafeSmokeTier)
{
    const auto result = SagaStressArena::ResolveStressScenarioConfig(
        SagaStressArena::DefaultStressScenarioConfig());

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.config.scenario,
              SagaStressArena::kDirectZoneDiagnosticsSmokeScenario);
    EXPECT_EQ(result.config.tier, SagaStressArena::StressTier::Smoke);
    EXPECT_EQ(result.config.actors, 5u);
    EXPECT_EQ(result.config.ticks, 50u);
    EXPECT_EQ(result.config.maxDurationSec, 10u);
    EXPECT_FALSE(result.config.manualOnly);
}

TEST(SagaStressArenaTests, StressTierBoundsAreExplicitAndSafe)
{
    const auto smoke = SagaStressArena::DefinitionForTier(
        SagaStressArena::StressTier::Smoke);
    const auto miniSoak = SagaStressArena::DefinitionForTier(
        SagaStressArena::StressTier::MiniSoak);
    const auto heavy = SagaStressArena::DefinitionForTier(
        SagaStressArena::StressTier::Heavy);

    EXPECT_LE(smoke.maxActors, 10u);
    EXPECT_LE(smoke.maxTicks, 100u);
    EXPECT_FALSE(smoke.manualOnly);
    EXPECT_GT(miniSoak.maxActors, smoke.maxActors);
    EXPECT_GT(miniSoak.maxTicks, smoke.maxTicks);
    EXPECT_FALSE(miniSoak.manualOnly);
    EXPECT_TRUE(heavy.manualOnly);
    EXPECT_LE(heavy.maxActors, 250u);
    EXPECT_LE(heavy.maxTicks, 10000u);
}

TEST(SagaStressArenaTests, HeavyTierIsManualOnlyAndDoesNotExecute)
{
    auto config = SagaStressArena::DefaultStressScenarioConfig();
    config.tier = SagaStressArena::StressTier::Heavy;
    config.reportDirectory = TestRoot() / "heavy";

    const auto result = SagaStressArena::ScenarioRunner::Run(config);

    EXPECT_EQ(result.status, SagaStressArena::ScenarioRunStatus::ManualTierOnly);
    EXPECT_FALSE(std::filesystem::exists(
        result.reportPaths.operationalSnapshot));
}

TEST(SagaStressArenaTests, InvalidScenarioFailsDeterministically)
{
    auto config = SagaStressArena::DefaultStressScenarioConfig();
    config.scenario = "network_bot_swarm";

    const auto result = SagaStressArena::ScenarioRunner::Run(config);

    EXPECT_EQ(result.status, SagaStressArena::ScenarioRunStatus::InvalidConfig);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics[0], "Unknown stress scenario.");
}

TEST(SagaStressArenaTests, SmokeScenarioCreatesReportArtifacts)
{
    const auto root = TestRoot() / "smoke_artifacts";
    std::filesystem::remove_all(root);

    const auto result = SagaStressArena::ScenarioRunner::Run(
        SmokeConfig(root));

    ASSERT_TRUE(result.Succeeded()) << result.message;
    EXPECT_TRUE(std::filesystem::exists(
        result.reportPaths.operationalSnapshot));
    EXPECT_TRUE(std::filesystem::exists(result.reportPaths.reliabilityReport));
    EXPECT_TRUE(std::filesystem::exists(result.reportPaths.lifetimeLeaks));
}

TEST(SagaStressArenaTests, OperationalReportContainsServerMetrics)
{
    const auto root = TestRoot() / "operational_metrics";
    std::filesystem::remove_all(root);

    const auto result = SagaStressArena::ScenarioRunner::Run(
        SmokeConfig(root));
    ASSERT_TRUE(result.Succeeded()) << result.message;

    const auto report = ReadJson(result.reportPaths.operationalSnapshot);
    EXPECT_EQ(report.at("reportKind"), "operational_snapshot");
    EXPECT_EQ(report.at("metadata").at("scenario"),
              SagaStressArena::kDirectZoneDiagnosticsSmokeScenario);
    EXPECT_EQ(report.at("metadata").at("tier"), "smoke");
    EXPECT_EQ(FindMetric(report, "server.tick.count"), 10.0);
    EXPECT_EQ(FindMetric(report, "server.tick.ms"), 20.0);
    EXPECT_EQ(FindMetric(report, "world.entities.active"), 3.0);
    EXPECT_EQ(FindMetric(report, "server.movement.accepted_inputs"), 30.0);
    EXPECT_EQ(FindMetric(report, "server.movement.rejected_inputs"), 1.0);
    EXPECT_EQ(FindMetric(report, "server.packets.rejected"), 1.0);
    EXPECT_GE(report.at("memoryRecords").size(), 3u);
    EXPECT_EQ(report.at("resourceSummary").at("totalActive"), 1u);
    EXPECT_EQ(report.at("activeResources")[0].at("debugName"),
              "direct-zone-harness");
    EXPECT_GE(report.at("recentLogEvents").size(), 1u);
}

TEST(SagaStressArenaTests, ReliabilityReportContainsHealthRulesAndLeaks)
{
    const auto root = TestRoot() / "reliability";
    std::filesystem::remove_all(root);

    const auto result = SagaStressArena::ScenarioRunner::Run(
        SmokeConfig(root));
    ASSERT_TRUE(result.Succeeded()) << result.message;

    const auto report = ReadJson(result.reportPaths.reliabilityReport);
    EXPECT_EQ(report.at("reportKind"), "reliability_failure_report");
    EXPECT_EQ(FindMetric(report, "server.tick.count"), 10.0);

    const auto names = HealthRuleViolationNames(report);
    EXPECT_NE(std::find(names.begin(), names.end(), "stress-slow-tick"),
              names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "stress-budget-overrun"),
              names.end());
    EXPECT_EQ(report.at("lifetimeLeakSummary").at("totalActive"), 3u);
    EXPECT_EQ(report.at("lifetimeLeakSummary").at("byOwnerSystem")
                  .at("SagaStressArena"),
              3u);
    EXPECT_GE(report.at("memoryRecords").size(), 3u);
    EXPECT_EQ(report.at("resourceSummary").at("byOwnerSystem")
                  .at("SagaStressArena"),
              1u);
}

TEST(SagaStressArenaTests, SmokeScenarioStableFieldsAreDeterministic)
{
    const auto root = TestRoot() / "deterministic";
    std::filesystem::remove_all(root);

    const auto first = SagaStressArena::ScenarioRunner::Run(
        SmokeConfig(root / "first"));
    const auto second = SagaStressArena::ScenarioRunner::Run(
        SmokeConfig(root / "second"));
    ASSERT_TRUE(first.Succeeded()) << first.message;
    ASSERT_TRUE(second.Succeeded()) << second.message;

    const auto firstReport = ReadJson(first.reportPaths.operationalSnapshot);
    const auto secondReport = ReadJson(second.reportPaths.operationalSnapshot);

    EXPECT_EQ(firstReport.at("reportKind"), secondReport.at("reportKind"));
    EXPECT_EQ(firstReport.at("metadata").at("scenario"),
              secondReport.at("metadata").at("scenario"));
    EXPECT_EQ(firstReport.at("metadata").at("tier"),
              secondReport.at("metadata").at("tier"));
    EXPECT_EQ(MetricMap(firstReport), MetricMap(secondReport));
}

TEST(SagaStressArenaTests, PacketChaosSmokeReportContainsChaosMetrics)
{
    const auto root = TestRoot() / "packet_chaos";
    std::filesystem::remove_all(root);

    const auto result = SagaStressArena::ScenarioRunner::Run(
        PacketChaosSmokeConfig(root));
    ASSERT_TRUE(result.Succeeded()) << result.message;

    const auto report = ReadJson(result.reportPaths.operationalSnapshot);
    EXPECT_EQ(report.at("metadata").at("scenario"),
              SagaStressArena::kDirectZonePacketChaosSmokeScenario);
    EXPECT_EQ(FindMetric(report, "net.chaos.frames_seen"), 30.0);
    EXPECT_GT(FindMetric(report, "net.chaos.frames_dropped").value_or(0.0), 0.0);
    EXPECT_GT(FindMetric(report, "net.chaos.frames_duplicated").value_or(0.0), 0.0);
    EXPECT_GT(FindMetric(report, "net.chaos.frames_deferred").value_or(0.0), 0.0);
    EXPECT_GT(FindMetric(report, "net.chaos.frames_released").value_or(0.0), 0.0);
    EXPECT_EQ(FindMetric(report, "net.chaos.queue_depth"), 0.0);
}

TEST(SagaStressArenaTests, InvalidReportDirectoryFailsDeterministically)
{
    const auto root = TestRoot() / "invalid_report_dir";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto fileParent = root / "not-a-directory";
    {
        std::ofstream marker(fileParent);
        marker << "file";
    }

    auto config = SmokeConfig(fileParent / "reports");
    const auto result = SagaStressArena::ScenarioRunner::Run(config);

    EXPECT_EQ(result.status, SagaStressArena::ScenarioRunStatus::ReportWriteFailed);
    EXPECT_FALSE(result.diagnostics.empty());
    EXPECT_FALSE(std::filesystem::exists(
        result.reportPaths.operationalSnapshot));
}

TEST(SagaStressArenaTests, DirectHarnessMovementMutatesOnlyDuringTick)
{
    constexpr ClientId client = 91;
    constexpr EntityId entity = 9101;
    ZoneServer server(TestConfig());
    ASSERT_EQ(server.RegisterControlledActor(
                  client,
                  entity,
                  Vector3{0.0f, 0.0f, 0.0f}),
              ActorOwnershipResult::Registered);

    const auto frame = MakeFrame(
        PacketType::InputCommand,
        SerializeCommand(NetworkCommand(1, 0.0f, 1.0f)));
    EXPECT_EQ(server.ProcessRawInboundPacketForTesting(
                  client,
                  frame.data(),
                  frame.size()),
              ServerPacketNormalizationStatus::Success);

    const auto beforeTick = server.GetControlledActorPositionForTesting(entity);
    ASSERT_TRUE(beforeTick.has_value());
    EXPECT_FLOAT_EQ(beforeTick->z, 0.0f);

    const auto tickReport = server.TickMovementForTesting(1, 1.0);
    ASSERT_EQ(tickReport.results.size(), 1u);
    EXPECT_EQ(tickReport.results[0].decision,
              AuthoritativeMovementDecision::Accepted);
    EXPECT_TRUE(tickReport.results[0].mutated);

    const auto afterTick = server.GetControlledActorPositionForTesting(entity);
    ASSERT_TRUE(afterTick.has_value());
    EXPECT_FLOAT_EQ(afterTick->z, 6.0f);
}
