/// @file DiagnosticFoundationTests.cpp
/// @brief Verifies the initial SagaDiagnostics logging, health, and lifetime foundation.

#include <SagaEngine/Diagnostics/DiagnosticSystem.hpp>
#include <SagaEngine/Core/Log/Logger.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace
{

/// Test sink that stores events for assertions.
class CapturingSink final : public SagaEngine::Core::Log::LogSink
{
public:
    void Write(const SagaEngine::Core::Log::LogEvent& event) override
    {
        events.push_back(event);
    }

    std::vector<SagaEngine::Core::Log::LogEvent> events;
};

} // namespace

TEST(SagaDiagnosticsFoundationTests, LoggerBuffersAndDispatchesStructuredEvents)
{
    SagaEngine::Core::Log::Logger logger(4);
    logger.SetMinimumLevel(SagaEngine::Core::Log::Level::Debug);

    auto sink = std::make_shared<CapturingSink>();
    logger.AddSink(sink);

    logger.Log(SagaEngine::Core::Log::Level::Trace, "Ignored", "below threshold");
    logger.Log(SagaEngine::Core::Log::Level::Warn,
               "Net.Session",
               "Client disconnected unexpectedly",
               {{"session_id", "42"}, {"reason", "timeout"}});

    ASSERT_EQ(sink->events.size(), 1u);
    EXPECT_EQ(sink->events[0].level, SagaEngine::Core::Log::Level::Warn);
    EXPECT_EQ(sink->events[0].tag, "Net.Session");
    ASSERT_EQ(sink->events[0].context.size(), 2u);

    const auto buffered = logger.SnapshotRecentEvents();
    ASSERT_EQ(buffered.size(), 1u);
    EXPECT_EQ(buffered[0].message, "Client disconnected unexpectedly");
}

TEST(SagaDiagnosticsFoundationTests, HealthMonitorCapturesGaugeCounterAndTiming)
{
    SagaEngine::Diagnostics::HealthMonitor health;

    health.SetGauge("world.entities.active", 17.0);
    health.IncrementCounter("net.sessions.connected");
    health.IncrementCounter("net.sessions.connected", 2.0);
    health.RecordTiming("server.tick.ms", 4.5);

    const auto snapshot = health.Snapshot();

    const auto entities = snapshot.FindMetric("world.entities.active");
    ASSERT_TRUE(entities.has_value());
    EXPECT_EQ(entities->type, SagaEngine::Diagnostics::HealthMetricType::Gauge);
    EXPECT_DOUBLE_EQ(entities->value, 17.0);

    const auto sessions = snapshot.FindMetric("net.sessions.connected");
    ASSERT_TRUE(sessions.has_value());
    EXPECT_EQ(sessions->type, SagaEngine::Diagnostics::HealthMetricType::Counter);
    EXPECT_DOUBLE_EQ(sessions->value, 3.0);

    const auto tick = snapshot.FindMetric("server.tick.ms");
    ASSERT_TRUE(tick.has_value());
    EXPECT_EQ(tick->type, SagaEngine::Diagnostics::HealthMetricType::Timing);
    EXPECT_DOUBLE_EQ(tick->value, 4.5);
}

TEST(SagaDiagnosticsFoundationTests, LifetimeTrackerReportsActiveLogicalObjects)
{
    SagaEngine::Diagnostics::LifetimeTracker lifetime;

    const auto zone = lifetime.Register("Zone", "World", 100, 10);
    const auto entity = lifetime.Register("PlayerEntity", "World", 200, 11);

    ASSERT_TRUE(zone.IsValid());
    ASSERT_TRUE(entity.IsValid());
    EXPECT_TRUE(lifetime.SetOwner(entity, zone));

    auto active = lifetime.SnapshotActive();
    EXPECT_EQ(active.size(), 2u);

    EXPECT_TRUE(lifetime.MarkDestroyed(entity, 20));
    active = lifetime.SnapshotActive();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_EQ(active[0].objectId, zone.objectId);

    const auto leakedChildren = lifetime.FindUndestroyedByOwner(zone.objectId);
    EXPECT_TRUE(leakedChildren.empty());
}

TEST(SagaDiagnosticsFoundationTests, DiagnosticSystemStartsWithoutSdeOrServerDependencies)
{
    SagaEngine::Diagnostics::DiagnosticConfig config;
    config.enableConsoleLog = false;
    config.enableFileLog = false;

    SagaEngine::Diagnostics::DiagnosticSystem diagnostics(config);

    EXPECT_TRUE(diagnostics.Start());
    EXPECT_TRUE(diagnostics.IsRunning());

    diagnostics.Health().SetGauge("server.uptime.sec", 1.0);
    EXPECT_TRUE(diagnostics.Health().HasMetric("server.uptime.sec"));

    diagnostics.Shutdown();
    EXPECT_FALSE(diagnostics.IsRunning());
}
