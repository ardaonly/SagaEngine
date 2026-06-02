/// @file ZoneServerDiagnosticsTests.cpp
/// @brief Verifies optional ZoneServer diagnostics integration.

#include "SagaServer/Networking/Server/ZoneServer.h"

#include "SagaEngine/Diagnostics/DiagnosticSystem.hpp"
#include "SagaEngine/Input/Commands/InputCommandSerializer.h"

#include <gtest/gtest.h>

#include <cstring>
#include <cstdint>
#include <optional>
#include <string_view>
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
using SagaEngine::Server::Simulation::AuthoritativeMovementCommandIntakeDecision;
using SagaEngine::Server::Simulation::AuthoritativeMovementDecision;
using SagaEngine::Server::Simulation::EntityId;
using SagaEngine::Server::Simulation::Vector3;
using SagaServer::Networking::ServerPacketNormalizationStatus;
using SagaServer::Networking::ZoneServer;
using SagaServer::Networking::ZoneServerConfig;

constexpr ClientId kClient = 51;
constexpr EntityId kEntity = 9001;

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

ZoneServerConfig TestConfig()
{
    ZoneServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 0;
    config.zoneId = 21;
    config.zoneName = "zone-server-diagnostics-test";
    config.enableDetailedPacketLog = false;
    return config;
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

std::optional<double> MetricValue(
    const SagaEngine::Diagnostics::DiagnosticSystem& diagnostics,
    std::string_view name)
{
    const auto metric = diagnostics.Health().Snapshot().FindMetric(name);
    if (!metric.has_value())
    {
        return std::nullopt;
    }
    return metric->value;
}

void ExpectPosition(const Vector3& position, float x, float y, float z)
{
    EXPECT_FLOAT_EQ(position.x, x);
    EXPECT_FLOAT_EQ(position.y, y);
    EXPECT_FLOAT_EQ(position.z, z);
}

} // namespace

TEST(ZoneServerDiagnosticsTests, MovementBehaviorWorksWithDiagnosticsAbsent)
{
    ZoneServer server(TestConfig());
    ASSERT_EQ(server.RegisterControlledActor(
                  kClient,
                  kEntity,
                  Vector3{0.0f, 0.0f, 0.0f}),
              ActorOwnershipResult::Registered);

    const auto frame = MakeFrame(
        PacketType::InputCommand,
        SerializeCommand(NetworkCommand(1, 0.0f, 1.0f)));

    EXPECT_EQ(server.ProcessRawInboundPacketForTesting(
                  kClient,
                  frame.data(),
                  frame.size()),
              ServerPacketNormalizationStatus::Success);

    const auto beforeTick = server.GetControlledActorPositionForTesting(kEntity);
    ASSERT_TRUE(beforeTick.has_value());
    ExpectPosition(*beforeTick, 0.0f, 0.0f, 0.0f);

    const auto report = server.TickMovementForTesting(1, 1.0);
    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].decision, AuthoritativeMovementDecision::Accepted);

    const auto afterTick = server.GetControlledActorPositionForTesting(kEntity);
    ASSERT_TRUE(afterTick.has_value());
    ExpectPosition(*afterTick, 0.0f, 0.0f, 6.0f);
}

TEST(ZoneServerDiagnosticsTests, ActorRegistrationUpdatesActiveEntityGauge)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);

    EXPECT_EQ(MetricValue(diagnostics, "world.entities.active"), 0.0);
    ASSERT_EQ(server.RegisterControlledActor(
                  kClient,
                  kEntity,
                  Vector3{1.0f, 2.0f, 3.0f}),
              ActorOwnershipResult::Registered);
    EXPECT_EQ(MetricValue(diagnostics, "world.entities.active"), 1.0);

    EXPECT_EQ(server.UnregisterControlledActor(kClient),
              ActorOwnershipResult::Unregistered);
    EXPECT_EQ(MetricValue(diagnostics, "world.entities.active"), 0.0);
}

TEST(ZoneServerDiagnosticsTests, AcceptedMovementInputIncrementsDiagnosticsCounter)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);
    ASSERT_EQ(server.RegisterControlledActor(
                  kClient,
                  kEntity,
                  Vector3{}),
              ActorOwnershipResult::Registered);

    const auto frame = MakeFrame(
        PacketType::InputCommand,
        SerializeCommand(NetworkCommand(3, 1.0f, 0.0f)));

    EXPECT_EQ(server.ProcessRawInboundPacketForTesting(
                  kClient,
                  frame.data(),
                  frame.size()),
              ServerPacketNormalizationStatus::Success);

    EXPECT_EQ(MetricValue(diagnostics, "server.movement.accepted_inputs"), 1.0);
}

TEST(ZoneServerDiagnosticsTests, RejectedMovementInputIncrementsDiagnosticsCounter)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);

    const auto unknownClientFrame = MakeFrame(
        PacketType::InputCommand,
        SerializeCommand(NetworkCommand(4, 1.0f, 0.0f)));
    EXPECT_EQ(server.ProcessRawInboundPacketForTesting(
                  kClient,
                  unknownClientFrame.data(),
                  unknownClientFrame.size()),
              ServerPacketNormalizationStatus::Success);

    const auto malformedInputFrame = MakeFrame(PacketType::InputCommand, {0x01});
    EXPECT_EQ(server.ProcessRawInboundPacketForTesting(
                  kClient,
                  malformedInputFrame.data(),
                  malformedInputFrame.size()),
              ServerPacketNormalizationStatus::Success);

    const auto intake = server.GetLastMovementIntakeResultForTesting();
    ASSERT_TRUE(intake.has_value());
    EXPECT_EQ(intake->decision,
              AuthoritativeMovementCommandIntakeDecision::MalformedInput);
    EXPECT_EQ(MetricValue(diagnostics, "server.movement.rejected_inputs"), 2.0);
}

TEST(ZoneServerDiagnosticsTests, BadFrameNormalizationIncrementsRejectedPacketCounter)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);

    auto frame = MakeFrame(PacketType::InputCommand, {0x01});
    frame[0] ^= 0xFF;

    EXPECT_EQ(server.ProcessRawInboundPacketForTesting(
                  kClient,
                  frame.data(),
                  frame.size()),
              ServerPacketNormalizationStatus::BadMagic);
    EXPECT_EQ(MetricValue(diagnostics, "server.packets.rejected"), 1.0);
}

TEST(ZoneServerDiagnosticsTests, MovementTickEmitsTickCounter)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);

    const auto report = server.TickMovementForTesting(7, 1.0);

    EXPECT_TRUE(report.results.empty());
    EXPECT_EQ(MetricValue(diagnostics, "server.tick.count"), 1.0);
}

TEST(ZoneServerDiagnosticsTests, TickDurationEmitsTimingMetric)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);

    server.RecordTickDurationForTesting(9, 12500);

    EXPECT_EQ(MetricValue(diagnostics, "server.tick.ms"), 12.5);
    EXPECT_FALSE(
        MetricValue(diagnostics, "server.tick.budget_overruns").has_value());
}

TEST(ZoneServerDiagnosticsTests, SlowTickEmitsBudgetCounterAndWarningLog)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);

    server.RecordTickDurationForTesting(10, 20000);

    EXPECT_EQ(MetricValue(diagnostics, "server.tick.ms"), 20.0);
    EXPECT_EQ(MetricValue(diagnostics, "server.tick.budget_overruns"), 1.0);

    const auto events = diagnostics.Log().SnapshotRecentEvents();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[1].tag, "ZoneServer");
    EXPECT_EQ(events[1].message, "Tick budget overrun");
    ASSERT_EQ(events[1].context.size(), 4u);
    EXPECT_EQ(events[1].context[1].first, "tick");
    EXPECT_EQ(events[1].context[1].second, "10");
}

TEST(ZoneServerDiagnosticsTests, DiagnosticsAttachEmitsStructuredLogEvent)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());

    server.SetDiagnostics(&diagnostics);

    const auto events = diagnostics.Log().SnapshotRecentEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].tag, "ZoneServer");
    EXPECT_EQ(events[0].message, "Diagnostics attached to zone server");
    ASSERT_EQ(events[0].context.size(), 2u);
    EXPECT_EQ(events[0].context[0].first, "zone_id");
    EXPECT_EQ(events[0].context[0].second, "21");
}

TEST(ZoneServerDiagnosticsTests, DiagnosticsDoNotChangeMovementMutationTiming)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    ZoneServer server(TestConfig());
    server.SetDiagnostics(&diagnostics);
    ASSERT_EQ(server.RegisterControlledActor(
                  kClient,
                  kEntity,
                  Vector3{0.0f, 0.0f, 0.0f}),
              ActorOwnershipResult::Registered);

    const auto frame = MakeFrame(
        PacketType::InputCommand,
        SerializeCommand(NetworkCommand(8, 0.0f, 1.0f)));
    EXPECT_EQ(server.ProcessRawInboundPacketForTesting(
                  kClient,
                  frame.data(),
                  frame.size()),
              ServerPacketNormalizationStatus::Success);

    const auto beforeTick = server.GetControlledActorPositionForTesting(kEntity);
    ASSERT_TRUE(beforeTick.has_value());
    ExpectPosition(*beforeTick, 0.0f, 0.0f, 0.0f);

    const auto tickReport = server.TickMovementForTesting(8, 1.0);
    ASSERT_EQ(tickReport.results.size(), 1u);
    EXPECT_TRUE(tickReport.results[0].mutated);

    const auto afterTick = server.GetControlledActorPositionForTesting(kEntity);
    ASSERT_TRUE(afterTick.has_value());
    ExpectPosition(*afterTick, 0.0f, 0.0f, 6.0f);
}
