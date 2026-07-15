/// @file ScenarioRunner.cpp
/// @brief Implements bounded direct ZoneServer diagnostics scenarios.

#include "SagaStressArena/ScenarioRunner.hpp"

#include "SagaEngine/Diagnostics/Crash/CrashContext.hpp"
#include "SagaEngine/Diagnostics/DiagnosticSystem.hpp"
#include "SagaEngine/Diagnostics/Health/HealthRule.hpp"
#include "SagaEngine/Diagnostics/Report/DiagnosticReportWriter.hpp"
#include "SagaEngine/Input/Commands/InputCommandSerializer.h"
#include "SagaEngine/Networking/NetworkChaosLayer.h"
#include "SagaEngine/ServerAuthority/ZoneServer.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace SagaStressArena
{
namespace
{

using SagaEngine::Diagnostics::DiagnosticReportWriter;
using SagaEngine::Diagnostics::DiagnosticSystem;
using SagaEngine::Diagnostics::HealthRule;
using SagaEngine::Diagnostics::HealthRuleType;
using SagaEngine::Diagnostics::HealthSeverity;
using SagaEngine::Diagnostics::ResourceType;
using SagaEngine::Input::FixedFromFloat;
using SagaEngine::Input::InputCommand;
using SagaEngine::Input::InputCommandSerializer;
using SagaEngine::Input::MakeInputCommand;
using SagaEngine::Networking::ClientId;
using SagaEngine::Networking::PacketType;
using SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult;
using SagaEngine::ServerAuthority::Simulation::EntityId;
using SagaEngine::ServerAuthority::Simulation::Vector3;
using SagaEngine::Networking::NetworkChaosConfig;
using SagaEngine::Networking::NetworkChaosFrame;
using SagaEngine::Networking::NetworkChaosLayer;
using SagaEngine::Networking::ServerPacketNormalizationStatus;
using SagaEngine::ServerAuthority::ZoneServer;
using SagaEngine::ServerAuthority::ZoneServerConfig;
namespace CrashReportKinds = SagaEngine::Diagnostics::CrashReportKinds;

constexpr int kExitSuccess = 0;
constexpr int kExitInvalidConfig = 4;
constexpr int kExitManualTier = 5;
constexpr int kExitDurationExceeded = 6;
constexpr int kExitReportWriteFailed = 7;

[[nodiscard]] HealthRule Rule(std::string ruleName,
                              std::string metricName,
                              HealthRuleType type,
                              double threshold,
                              HealthSeverity severity)
{
    HealthRule rule;
    rule.ruleName = std::move(ruleName);
    rule.metricName = std::move(metricName);
    rule.type = type;
    rule.threshold = threshold;
    rule.severity = severity;
    return rule;
}

void RegisterStressHealthRules(DiagnosticSystem& diagnostics,
                               const ResolvedStressScenarioConfig& config)
{
    diagnostics.Health().RegisterRule(Rule(
        "stress-slow-tick",
        "server.tick.ms",
        HealthRuleType::TimingGreaterThan,
        16.0,
        HealthSeverity::Warning));
    diagnostics.Health().RegisterRule(Rule(
        "stress-budget-overrun",
        "server.tick.budget_overruns",
        HealthRuleType::CounterGreaterThan,
        0.0,
        HealthSeverity::Error));
    diagnostics.Health().RegisterRule(Rule(
        "stress-entity-count-cap",
        "world.entities.active",
        HealthRuleType::GaugeGreaterThan,
        static_cast<double>(DefinitionForTier(config.tier).maxActors),
        HealthSeverity::Critical));
    diagnostics.Health().RegisterRule(Rule(
        "stress-rejected-input-cap",
        "server.movement.rejected_inputs",
        HealthRuleType::CounterGreaterThan,
        static_cast<double>(config.actors),
        HealthSeverity::Warning));
    diagnostics.Health().RegisterRule(Rule(
        "stress-tick-count-cap",
        "server.tick.count",
        HealthRuleType::CounterGreaterThan,
        static_cast<double>(DefinitionForTier(config.tier).maxTicks),
        HealthSeverity::Critical));
}

void RecordMemoryResourceProof(DiagnosticSystem& diagnostics,
                               const ResolvedStressScenarioConfig& config)
{
    (void)diagnostics.Memory().RecordAllocation("StressArena", 4096);
    (void)diagnostics.Memory().RecordAllocation(
        "ZoneServer",
        static_cast<std::uint64_t>(config.actors) * 512u);
    (void)diagnostics.Memory().AddBytes(
        "Movement",
        static_cast<std::uint64_t>(config.ticks) * 64u);
    (void)diagnostics.Memory().RecordFree("StressArena", 1024);

    const auto timer = diagnostics.Resource().RegisterResource(
        ResourceType::Timer,
        "SagaStressArena",
        "direct-zone-runner-duration-cap",
        0);
    (void)diagnostics.Resource().ReleaseResource(timer, 1);
    (void)diagnostics.Resource().RegisterResource(
        ResourceType::Other,
        "SagaStressArena",
        "direct-zone-harness",
        1);
}

[[nodiscard]] std::map<std::string, std::string> BuildMetadata(
    const ResolvedStressScenarioConfig& config,
    std::string status)
{
    return {
        {"actors", std::to_string(config.actors)},
        {"harness", "local_zone_server_direct"},
        {"scenario", config.scenario},
        {"status", std::move(status)},
        {"ticks", std::to_string(config.ticks)},
        {"tier", ToString(config.tier)},
        {"transport", "direct_no_socket"},
    };
}

[[nodiscard]] ZoneServerConfig MakeZoneConfig()
{
    ZoneServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 0;
    config.zoneId = 44;
    config.zoneName = "saga-stress-arena-direct-zone";
    config.enableDetailedPacketLog = false;
    config.maxTickBudgetUs = 14000;
    return config;
}

[[nodiscard]] NetworkChaosConfig MakeChaosConfig()
{
    NetworkChaosConfig config;
    config.enabled = true;
    config.seed = 0x5A6A2026u;
    config.dropPermille = 250;
    config.duplicatePermille = 250;
    config.deferPermille = 250;
    config.deferTicks = 1;
    config.maxDeferredFrames = 16;
    return config;
}

[[nodiscard]] bool IsPacketChaosScenario(
    const ResolvedStressScenarioConfig& config)
{
    return config.scenario == kDirectZonePacketChaosSmokeScenario;
}

[[nodiscard]] InputCommand NetworkCommand(std::uint32_t sequence,
                                          float moveX,
                                          float moveY,
                                          std::uint32_t simulationTick)
{
    auto command = MakeInputCommand(sequence, simulationTick, 1234);
    command.moveX = FixedFromFloat(moveX);
    command.moveY = FixedFromFloat(moveY);
    return command;
}

[[nodiscard]] std::vector<std::uint8_t> SerializeCommand(
    const InputCommand& command)
{
    const auto bytes = InputCommandSerializer::Serialize(command);
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

[[nodiscard]] std::vector<std::uint8_t> MakeFrame(
    PacketType packetType,
    const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> frame(
        SagaEngine::Networking::kServerPacketFrameHeaderSize + payload.size());

    const std::uint16_t magic =
        SagaEngine::Networking::kServerPacketFrameMagic;
    const std::uint32_t bodyLength =
        static_cast<std::uint32_t>(payload.size());

    std::memcpy(frame.data(), &magic, sizeof(magic));
    frame[2] = static_cast<std::uint8_t>(packetType);
    frame[3] = 0;
    std::memcpy(frame.data() + 4, &bodyLength, sizeof(bodyLength));
    if (!payload.empty())
    {
        std::memcpy(
            frame.data() + SagaEngine::Networking::kServerPacketFrameHeaderSize,
            payload.data(),
            payload.size());
    }

    return frame;
}

void AddWriteDiagnostics(ScenarioRunResult& result,
                         std::string reportName,
                         bool succeeded)
{
    if (!succeeded)
    {
        result.diagnostics.push_back(reportName + " write failed.");
    }
}

void DeliverFrameToServer(ZoneServer& server,
                          ScenarioRunResult& result,
                          ClientId clientId,
                          const std::vector<std::uint8_t>& frame)
{
    const auto status = server.ProcessRawInboundPacketForTesting(
        clientId,
        frame.data(),
        frame.size());
    if (status != ServerPacketNormalizationStatus::Success)
    {
        result.diagnostics.push_back("Movement input normalization failed.");
    }
}

void DeliverChaosFramesToServer(ZoneServer& server,
                                ScenarioRunResult& result,
                                const std::vector<NetworkChaosFrame>& frames)
{
    for (const auto& frame : frames)
    {
        DeliverFrameToServer(server, result, frame.clientId, frame.bytes);
    }
}

[[nodiscard]] bool WriteReports(DiagnosticSystem& diagnostics,
                                const ResolvedStressScenarioConfig& config,
                                const StressReportPaths& paths,
                                std::string status,
                                ScenarioRunResult& result)
{
    const auto metadata = BuildMetadata(config, std::move(status));

    auto operationalReport = diagnostics.BuildOperationalSnapshot();
    operationalReport.metadata = metadata;
    const auto operationalResult = DiagnosticReportWriter::WriteJsonToFile(
        operationalReport,
        paths.operationalSnapshot);
    AddWriteDiagnostics(
        result,
        "operational snapshot",
        operationalResult.Succeeded());
    if (!operationalResult.Succeeded() && config.failFast)
    {
        return false;
    }

    const auto reliabilityResult = diagnostics.WriteReliabilityReport(
        paths.reliabilityReport,
        CrashReportKinds::ReliabilityFailureReport,
        "SagaStressArena direct ZoneServer diagnostics run",
        metadata);
    AddWriteDiagnostics(
        result,
        "reliability report",
        reliabilityResult.Succeeded());
    if (!reliabilityResult.Succeeded() && config.failFast)
    {
        return false;
    }

    auto lifetimeReport = diagnostics.BuildLifetimeLeakReport();
    lifetimeReport.metadata = metadata;
    const auto lifetimeResult = DiagnosticReportWriter::WriteJsonToFile(
        lifetimeReport,
        paths.lifetimeLeaks);
    AddWriteDiagnostics(
        result,
        "lifetime leak report",
        lifetimeResult.Succeeded());

    return operationalResult.Succeeded() &&
           reliabilityResult.Succeeded() &&
           lifetimeResult.Succeeded();
}

[[nodiscard]] ScenarioRunResult MakeInvalidConfigResult(
    StressScenarioConfigResult configResult)
{
    ScenarioRunResult result;
    result.status = ScenarioRunStatus::InvalidConfig;
    result.exitCode = kExitInvalidConfig;
    result.message = "Invalid SagaStressArena configuration.";
    result.config = std::move(configResult.config);
    result.reportPaths = BuildStressReportPaths(result.config.reportDirectory);
    result.diagnostics = std::move(configResult.diagnostics);
    return result;
}

} // namespace

const char* ToString(ScenarioRunStatus status) noexcept
{
    switch (status)
    {
        case ScenarioRunStatus::Succeeded:
            return "succeeded";
        case ScenarioRunStatus::InvalidConfig:
            return "invalid_config";
        case ScenarioRunStatus::ManualTierOnly:
            return "manual_tier_only";
        case ScenarioRunStatus::DurationExceeded:
            return "duration_exceeded";
        case ScenarioRunStatus::ReportWriteFailed:
            return "report_write_failed";
    }
    return "unknown";
}

ScenarioRunResult ScenarioRunner::Run(const StressScenarioConfig& config)
{
    auto configResult = ResolveStressScenarioConfig(config);
    if (!configResult.Succeeded())
    {
        return MakeInvalidConfigResult(std::move(configResult));
    }

    ScenarioRunResult result;
    result.config = configResult.config;
    result.reportPaths = BuildStressReportPaths(result.config.reportDirectory);

    if (result.config.manualOnly)
    {
        result.status = ScenarioRunStatus::ManualTierOnly;
        result.exitCode = kExitManualTier;
        result.message = "Heavy tier is documented manual-only in v1.";
        result.diagnostics.push_back(
            "Heavy tier is not executed by SagaStressArena v1.");
        return result;
    }

    DiagnosticSystem diagnostics;
    RegisterStressHealthRules(diagnostics, result.config);
    RecordMemoryResourceProof(diagnostics, result.config);

    ZoneServer server(MakeZoneConfig());
    server.SetDiagnostics(&diagnostics);
    const bool chaosScenario = IsPacketChaosScenario(result.config);
    NetworkChaosLayer chaosLayer(
        result.config.chaosConfig.value_or(MakeChaosConfig()));
    if (chaosScenario)
    {
        chaosLayer.SetDiagnostics(&diagnostics);
    }

    const auto startedAt = std::chrono::steady_clock::now();
    std::vector<std::pair<ClientId, EntityId>> actors;
    actors.reserve(result.config.actors);

    for (std::uint32_t index = 0; index < result.config.actors; ++index)
    {
        const ClientId clientId = static_cast<ClientId>(1000 + index);
        const EntityId entityId = static_cast<EntityId>(2000 + index);
        const auto registerResult = server.RegisterControlledActor(
            clientId,
            entityId,
            Vector3{static_cast<float>(index), 0.0f, 0.0f});
        if (registerResult != ActorOwnershipResult::Registered)
        {
            result.diagnostics.push_back("Controlled actor registration failed.");
            break;
        }

        actors.push_back({clientId, entityId});
        const auto lifetime = diagnostics.Lifetime().Register(
            "StressArenaActor",
            "SagaStressArena",
            entityId,
            0);
        (void)lifetime;
    }

    for (std::uint32_t tick = 1; tick <= result.config.ticks; ++tick)
    {
        for (std::uint32_t actorIndex = 0; actorIndex < actors.size();
             ++actorIndex)
        {
            const ClientId clientId = actors[actorIndex].first;
            const auto frame = MakeFrame(
                PacketType::InputCommand,
                SerializeCommand(NetworkCommand(
                    tick * 100 + actorIndex,
                    0.0f,
                    1.0f,
                    1)));

            if (chaosScenario)
            {
                const auto chaosResult = chaosLayer.ProcessFrame(
                    tick,
                    NetworkChaosFrame{clientId, frame});
                DeliverChaosFramesToServer(
                    server,
                    result,
                    chaosResult.frames);
            }
            else
            {
                DeliverFrameToServer(server, result, clientId, frame);
            }

            if (!result.diagnostics.empty() && result.config.failFast)
            {
                break;
            }
        }

        if (chaosScenario)
        {
            const auto released = chaosLayer.ReleaseDueFrames(tick);
            DeliverChaosFramesToServer(server, result, released);
            if (!result.diagnostics.empty() && result.config.failFast)
            {
                break;
            }
        }

        if (!result.diagnostics.empty() && result.config.failFast)
        {
            break;
        }

        (void)server.TickMovementForTesting(tick, 1.0);
        const std::uint64_t durationUs =
            tick == result.config.ticks ? 20000u : 12000u;
        server.RecordTickDurationForTesting(tick, durationUs);

        const auto elapsed = std::chrono::steady_clock::now() - startedAt;
        if (elapsed > std::chrono::seconds(result.config.maxDurationSec))
        {
            diagnostics.Log().Log(
                SagaEngine::Core::Log::Level::Error,
                "SagaStressArena",
                "Scenario duration cap exceeded",
                {{"scenario", result.config.scenario},
                 {"tier", ToString(result.config.tier)}});
            result.status = ScenarioRunStatus::DurationExceeded;
            result.exitCode = kExitDurationExceeded;
            result.message = "Scenario duration cap exceeded.";
            (void)WriteReports(
                diagnostics,
                result.config,
                result.reportPaths,
                ToString(result.status),
                result);
            return result;
        }
    }

    if (chaosScenario)
    {
        const auto released = chaosLayer.ReleaseDueFrames(
            static_cast<std::uint64_t>(result.config.ticks) +
            chaosLayer.Config().deferTicks);
        DeliverChaosFramesToServer(server, result, released);
    }

    if (result.diagnostics.empty() || !result.config.failFast)
    {
        const auto unknownClientFrame = MakeFrame(
            PacketType::InputCommand,
            SerializeCommand(NetworkCommand(900000, 1.0f, 0.0f, 1)));
        (void)server.ProcessRawInboundPacketForTesting(
            static_cast<ClientId>(999999),
            unknownClientFrame.data(),
            unknownClientFrame.size());

        auto badFrame = MakeFrame(PacketType::InputCommand, {0x01});
        badFrame[0] ^= 0xFF;
        (void)server.ProcessRawInboundPacketForTesting(
            static_cast<ClientId>(999998),
            badFrame.data(),
            badFrame.size());
    }

    diagnostics.Log().Log(
        SagaEngine::Core::Log::Level::Info,
        "SagaStressArena",
        "Scenario completed",
        {{"scenario", result.config.scenario},
         {"tier", ToString(result.config.tier)}});

    if (!WriteReports(
            diagnostics,
            result.config,
            result.reportPaths,
            "succeeded",
            result))
    {
        result.status = ScenarioRunStatus::ReportWriteFailed;
        result.exitCode = kExitReportWriteFailed;
        result.message = "One or more report artifacts could not be written.";
        return result;
    }

    result.status = ScenarioRunStatus::Succeeded;
    result.exitCode = kExitSuccess;
    result.message = "Scenario completed.";
    return result;
}

} // namespace SagaStressArena
