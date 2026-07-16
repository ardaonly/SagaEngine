/// @file DiagnosticFoundationTests.cpp
/// @brief Verifies the initial SagaDiagnostics logging, health, and lifetime foundation.

#include <SagaEngine/Diagnostics/DiagnosticSystem.hpp>
#include <SagaEngine/Core/Log/Logger.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
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
    SagaEngine::Core::Log::Logger logger(2);
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
    EXPECT_EQ(sink->events[0].message, "Client disconnected unexpectedly");
    ASSERT_EQ(sink->events[0].context.size(), 2u);
    EXPECT_EQ(sink->events[0].context[0].first, "session_id");
    EXPECT_EQ(sink->events[0].context[0].second, "42");
    EXPECT_EQ(sink->events[0].context[1].first, "reason");
    EXPECT_EQ(sink->events[0].context[1].second, "timeout");

    logger.Log(SagaEngine::Core::Log::Level::Error, "Net.Session", "retry failed");
    logger.Log(SagaEngine::Core::Log::Level::Fatal, "Net.Session", "session aborted");

    const auto buffered = logger.SnapshotRecentEvents();
    ASSERT_EQ(buffered.size(), 2u);
    EXPECT_EQ(buffered[0].message, "retry failed");
    EXPECT_EQ(buffered[1].level, SagaEngine::Core::Log::Level::Fatal);
    EXPECT_STREQ(SagaEngine::Core::Log::ToString(SagaEngine::Core::Log::Level::Fatal),
                 "Fatal");
}

TEST(SagaDiagnosticsFoundationTests, DiagnosticConfigDefaultValuesAreLocalSafe)
{
    const SagaEngine::Diagnostics::DiagnosticConfig config;

    EXPECT_TRUE(config.enableLogging);
    EXPECT_TRUE(config.enableHealth);
    EXPECT_TRUE(config.enableLifetime);
    EXPECT_TRUE(config.enableConsoleLog);
    EXPECT_FALSE(config.enableFileLog);
    EXPECT_EQ(config.minimumLogLevel, SagaEngine::Core::Log::Level::Info);
    EXPECT_TRUE(config.logFilePath.empty());
    EXPECT_EQ(config.reportDirectory, "diagnostics/reports");
    EXPECT_EQ(config.maxBufferedLogEvents, 1024u);
    EXPECT_EQ(config.maxFaultReports, 128u);
}

TEST(SagaDiagnosticsFoundationTests, DiagnosticSystemConstructsWithDefaultConfig)
{
    SagaEngine::Diagnostics::DiagnosticSystem diagnostics;

    EXPECT_FALSE(diagnostics.IsRunning());
    EXPECT_TRUE(diagnostics.Config().enableLogging);
    EXPECT_EQ(diagnostics.Log().MinimumLevel(), SagaEngine::Core::Log::Level::Info);
}

TEST(SagaDiagnosticsFoundationTests, DiagnosticSystemConstructsWithCustomConfig)
{
    SagaEngine::Diagnostics::DiagnosticConfig config;
    config.enableLogging = false;
    config.enableConsoleLog = false;
    config.enableHealth = false;
    config.enableLifetime = false;
    config.minimumLogLevel = SagaEngine::Core::Log::Level::Warn;
    config.reportDirectory = "Build/Reports/Diagnostics";
    config.maxBufferedLogEvents = 2;

    SagaEngine::Diagnostics::DiagnosticSystem diagnostics(config);

    EXPECT_FALSE(diagnostics.Config().enableLogging);
    EXPECT_FALSE(diagnostics.Config().enableHealth);
    EXPECT_FALSE(diagnostics.Config().enableLifetime);
    EXPECT_EQ(diagnostics.Config().reportDirectory, "Build/Reports/Diagnostics");
    EXPECT_EQ(diagnostics.Log().MinimumLevel(), SagaEngine::Core::Log::Level::Warn);

    diagnostics.Log().Log(SagaEngine::Core::Log::Level::Info, "Diagnostics", "ignored");
    diagnostics.Log().Log(SagaEngine::Core::Log::Level::Warn, "Diagnostics", "kept");

    const auto buffered = diagnostics.Log().SnapshotRecentEvents();
    ASSERT_EQ(buffered.size(), 1u);
    EXPECT_EQ(buffered[0].message, "kept");
}

TEST(SagaDiagnosticsFoundationTests, DiagnosticSystemLifecycleIsIdempotent)
{
    SagaEngine::Diagnostics::DiagnosticConfig config;
    config.enableLogging = false;
    config.enableConsoleLog = false;

    SagaEngine::Diagnostics::DiagnosticSystem diagnostics(config);

    EXPECT_TRUE(diagnostics.Start());
    EXPECT_TRUE(diagnostics.Start());
    EXPECT_TRUE(diagnostics.IsRunning());

    diagnostics.Shutdown();
    EXPECT_FALSE(diagnostics.IsRunning());

    diagnostics.Shutdown();
    EXPECT_FALSE(diagnostics.IsRunning());
}

TEST(SagaDiagnosticsFoundationTests, DiagnosticSystemAllowsInjectedSinkWhenConfiguredSinksAreDisabled)
{
    SagaEngine::Diagnostics::DiagnosticConfig config;
    config.enableLogging = false;
    config.enableConsoleLog = false;

    SagaEngine::Diagnostics::DiagnosticSystem diagnostics(config);
    ASSERT_TRUE(diagnostics.Start());

    auto sink = std::make_shared<CapturingSink>();
    diagnostics.Log().AddSink(sink);
    diagnostics.Log().Log(SagaEngine::Core::Log::Level::Error,
                          "Diagnostics",
                          "explicit sink");

    ASSERT_EQ(sink->events.size(), 1u);
    EXPECT_EQ(sink->events[0].message, "explicit sink");
}

TEST(SagaDiagnosticsFoundationTests, HealthMonitorCapturesGaugeCounterAndTiming)
{
    SagaEngine::Diagnostics::HealthMonitor health;

    health.SetGauge("memory.process.mb", 256.0);
    health.SetGauge("world.zones.loaded", 3.0);
    health.SetGauge("world.entities.active", 17.0);
    health.SetGauge("net.sessions.active", 4.0);
    health.SetGauge("server.uptime.sec", 12.0);
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

    ASSERT_GE(snapshot.Metrics().size(), 7u);
    EXPECT_EQ(snapshot.Metrics()[0].name, "memory.process.mb");
    EXPECT_EQ(snapshot.Metrics()[1].name, "net.sessions.active");
    EXPECT_TRUE(snapshot.FindMetric("server.uptime.sec").has_value());
    EXPECT_TRUE(snapshot.FindMetric("world.zones.loaded").has_value());
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
    EXPECT_EQ(active[0].objectId, zone.objectId);
    EXPECT_EQ(active[1].objectId, entity.objectId);

    EXPECT_TRUE(lifetime.MarkDestroyed(entity, 20));
    EXPECT_FALSE(lifetime.MarkDestroyed(entity, 21));

    const auto all = lifetime.SnapshotAll();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[1].objectId, entity.objectId);
    EXPECT_TRUE(all[1].destroyed);
    EXPECT_EQ(all[1].destroyedTick, 20u);

    active = lifetime.SnapshotActive();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_EQ(active[0].objectId, zone.objectId);

    const auto leakedChildren = lifetime.FindUndestroyedByOwner(zone.objectId);
    EXPECT_TRUE(leakedChildren.empty());
}

TEST(SagaDiagnosticsFoundationTests, LifetimeTrackerRejectsInvalidHandles)
{
    SagaEngine::Diagnostics::LifetimeTracker lifetime;

    const SagaEngine::Diagnostics::LifetimeHandle invalid;
    const auto object = lifetime.Register("Zone", "World", 100, 10);

    EXPECT_FALSE(invalid.IsValid());
    EXPECT_FALSE(lifetime.MarkDestroyed(invalid, 20));
    EXPECT_FALSE(lifetime.SetOwner(invalid, object));
    EXPECT_FALSE(lifetime.SetOwner(object, invalid));
    const auto active = lifetime.SnapshotActive();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_EQ(active.front().objectId, object.objectId);
}

TEST(SagaDiagnosticsFoundationTests, DiagnosticSystemStartsWithoutSdeOrServerDependencies)
{
    SagaEngine::Diagnostics::DiagnosticConfig config;
    config.enableLogging = false;
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
