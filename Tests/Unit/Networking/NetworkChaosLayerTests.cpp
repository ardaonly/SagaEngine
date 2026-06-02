/// @file NetworkChaosLayerTests.cpp
/// @brief Tests deterministic direct-frame NetworkChaos policy behavior.

#include "SagaServer/Networking/Core/NetworkChaosLayer.h"

#include "SagaEngine/Diagnostics/DiagnosticSystem.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace
{

using SagaServer::Networking::NetworkChaosConfig;
using SagaServer::Networking::NetworkChaosDecisionKind;
using SagaServer::Networking::NetworkChaosFrame;
using SagaServer::Networking::NetworkChaosLayer;
namespace Metrics = SagaServer::Networking::NetworkChaosMetrics;

NetworkChaosFrame Frame(std::uint64_t clientId, std::uint8_t marker)
{
    return NetworkChaosFrame{
        clientId,
        std::vector<std::uint8_t>{0x53, marker, 0x10}};
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

std::vector<NetworkChaosDecisionKind> DecisionsFor(
    NetworkChaosConfig config,
    std::uint32_t frames)
{
    NetworkChaosLayer layer(config);
    std::vector<NetworkChaosDecisionKind> decisions;
    decisions.reserve(frames);
    for (std::uint32_t index = 0; index < frames; ++index)
    {
        decisions.push_back(
            layer.ProcessFrame(1, Frame(100 + index, static_cast<std::uint8_t>(index)))
                .decision.kind);
    }
    return decisions;
}

} // namespace

TEST(NetworkChaosLayerTests, DisabledPolicyDeliversAllFrames)
{
    NetworkChaosLayer layer;

    const auto result = layer.ProcessFrame(7, Frame(11, 0x01));

    EXPECT_EQ(result.decision.kind, NetworkChaosDecisionKind::Deliver);
    ASSERT_EQ(result.frames.size(), 1u);
    EXPECT_EQ(result.frames[0].clientId, 11u);
    EXPECT_EQ(result.frames[0].bytes, (std::vector<std::uint8_t>{0x53, 0x01, 0x10}));
    EXPECT_EQ(layer.DeferredFrameCount(), 0u);
}

TEST(NetworkChaosLayerTests, DropPolicyIsDeterministicForSameSeed)
{
    NetworkChaosConfig config;
    config.enabled = true;
    config.seed = 12345;
    config.dropPermille = 400;
    config.duplicatePermille = 250;
    config.deferPermille = 0;

    const auto first = DecisionsFor(config, 24);
    const auto second = DecisionsFor(config, 24);

    EXPECT_EQ(first, second);
    EXPECT_NE(std::find(first.begin(), first.end(), NetworkChaosDecisionKind::Drop),
              first.end());
}

TEST(NetworkChaosLayerTests, DuplicateOncePolicyReturnsTwoFramesDeterministically)
{
    NetworkChaosConfig config;
    config.enabled = true;
    config.seed = 9;
    config.duplicatePermille = 1000;

    NetworkChaosLayer layer(config);
    const auto result = layer.ProcessFrame(3, Frame(22, 0xA0));

    EXPECT_EQ(result.decision.kind, NetworkChaosDecisionKind::DuplicateOnce);
    ASSERT_EQ(result.frames.size(), 2u);
    EXPECT_EQ(result.frames[0].clientId, 22u);
    EXPECT_EQ(result.frames[0].bytes, result.frames[1].bytes);
}

TEST(NetworkChaosLayerTests, DeferredFramesReleaseOnExplicitTick)
{
    NetworkChaosConfig config;
    config.enabled = true;
    config.seed = 77;
    config.deferPermille = 1000;
    config.deferTicks = 2;
    config.maxDeferredFrames = 4;

    NetworkChaosLayer layer(config);
    const auto result = layer.ProcessFrame(10, Frame(33, 0xB0));

    EXPECT_EQ(result.decision.kind, NetworkChaosDecisionKind::Defer);
    EXPECT_EQ(result.decision.releaseTick, 12u);
    EXPECT_TRUE(result.frames.empty());
    EXPECT_EQ(layer.DeferredFrameCount(), 1u);

    EXPECT_TRUE(layer.ReleaseDueFrames(11).empty());
    auto released = layer.ReleaseDueFrames(12);
    ASSERT_EQ(released.size(), 1u);
    EXPECT_EQ(released[0].clientId, 33u);
    EXPECT_EQ(released[0].bytes, (std::vector<std::uint8_t>{0x53, 0xB0, 0x10}));
    EXPECT_EQ(layer.DeferredFrameCount(), 0u);
}

TEST(NetworkChaosLayerTests, QueueCapDropsDeferredFramesWithoutGrowth)
{
    NetworkChaosConfig config;
    config.enabled = true;
    config.deferPermille = 1000;
    config.deferTicks = 10;
    config.maxDeferredFrames = 1;

    NetworkChaosLayer layer(config);

    const auto first = layer.ProcessFrame(1, Frame(44, 0xC0));
    const auto second = layer.ProcessFrame(1, Frame(45, 0xC1));

    EXPECT_EQ(first.decision.kind, NetworkChaosDecisionKind::Defer);
    EXPECT_EQ(second.decision.kind, NetworkChaosDecisionKind::QueueFullDrop);
    EXPECT_EQ(layer.DeferredFrameCount(), 1u);
}

TEST(NetworkChaosLayerTests, MetricsEmitThroughDiagnostics)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    NetworkChaosConfig config;
    config.enabled = true;
    config.dropPermille = 1000;
    config.maxDeferredFrames = 1;

    NetworkChaosLayer layer(config);
    layer.SetDiagnostics(&diagnostics);

    const auto result = layer.ProcessFrame(1, Frame(55, 0xD0));

    EXPECT_EQ(result.decision.kind, NetworkChaosDecisionKind::Drop);
    EXPECT_EQ(MetricValue(diagnostics, Metrics::FramesSeen), 1.0);
    EXPECT_EQ(MetricValue(diagnostics, Metrics::FramesDropped), 1.0);
    EXPECT_EQ(MetricValue(diagnostics, Metrics::QueueDepth), 0.0);
}

TEST(NetworkChaosLayerTests, MissingDiagnosticsDoesNotChangeDecisions)
{
    NetworkChaosConfig config;
    config.enabled = true;
    config.seed = 2026;
    config.dropPermille = 300;
    config.duplicatePermille = 300;
    config.deferPermille = 300;
    config.maxDeferredFrames = 4;

    NetworkChaosLayer withoutDiagnostics(config);
    NetworkChaosLayer withDiagnostics(config);
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;
    withDiagnostics.SetDiagnostics(&diagnostics);

    for (std::uint32_t index = 0; index < 12; ++index)
    {
        const auto without = withoutDiagnostics.ProcessFrame(
            index,
            Frame(70 + index, static_cast<std::uint8_t>(index)));
        const auto with = withDiagnostics.ProcessFrame(
            index,
            Frame(70 + index, static_cast<std::uint8_t>(index)));
        EXPECT_EQ(without.decision.kind, with.decision.kind);
        EXPECT_EQ(without.frames.size(), with.frames.size());
    }
}
