/// @file DiagnosticReliabilityTests.cpp
/// @brief Verifies crash context reports, health rules, and leak summaries.

#include <SagaEngine/Diagnostics/DiagnosticSystem.hpp>
#include <SagaEngine/Diagnostics/Health/HealthMonitor.hpp>
#include <SagaEngine/Diagnostics/Report/DiagnosticReportWriter.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace
{

using SagaEngine::Core::Log::Level;
using SagaEngine::Diagnostics::CrashReport;
using SagaEngine::Diagnostics::DiagnosticReportWriter;
using SagaEngine::Diagnostics::DiagnosticSystem;
using SagaEngine::Diagnostics::HealthMonitor;
using SagaEngine::Diagnostics::HealthRule;
using SagaEngine::Diagnostics::HealthRuleType;
using SagaEngine::Diagnostics::HealthSeverity;
namespace CrashReportKinds = SagaEngine::Diagnostics::CrashReportKinds;
namespace DiagnosticReportWriterDiagnostics =
    SagaEngine::Diagnostics::DiagnosticReportWriterDiagnostics;

std::filesystem::path TestRoot()
{
    return std::filesystem::temp_directory_path() /
           "saga_diagnostic_reliability_tests";
}

std::string WriteToString(const CrashReport& report)
{
    std::ostringstream output;
    const auto result = DiagnosticReportWriter::WriteJson(report, output);
    EXPECT_TRUE(result.Succeeded());
    return output.str();
}

nlohmann::json WriteToJson(const CrashReport& report)
{
    return nlohmann::json::parse(WriteToString(report));
}

HealthRule Rule(std::string ruleName,
                std::string metricName,
                HealthRuleType type,
                double threshold,
                HealthSeverity severity = HealthSeverity::Warning)
{
    HealthRule rule;
    rule.ruleName = std::move(ruleName);
    rule.metricName = std::move(metricName);
    rule.type = type;
    rule.threshold = threshold;
    rule.severity = severity;
    return rule;
}

} // namespace

TEST(DiagnosticReliabilityTests, CrashReportDefaultConstructs)
{
    const CrashReport report;

    EXPECT_EQ(report.schemaVersion, 1u);
    EXPECT_TRUE(report.reportKind.empty());
    EXPECT_TRUE(report.reason.empty());
    EXPECT_EQ(report.generationSequence, 0u);
    EXPECT_FALSE(report.threadId.has_value());
    EXPECT_EQ(report.summary.healthMetricCount, 0u);
    EXPECT_TRUE(report.healthMetrics.empty());
    EXPECT_TRUE(report.healthRuleViolations.empty());
    EXPECT_TRUE(report.lifetimeLeaks.empty());
    EXPECT_EQ(report.lifetimeLeakSummary.totalActive, 0u);
    EXPECT_TRUE(report.recentLogEvents.empty());
    EXPECT_TRUE(report.metadata.empty());
}

TEST(DiagnosticReliabilityTests, BuildCrashReportIncludesHealthMetrics)
{
    DiagnosticSystem diagnostics;
    diagnostics.Health().SetGauge("world.entities.active", 9.0);
    diagnostics.Health().RecordTiming("server.tick.ms", 2.5);

    const auto report = diagnostics.BuildCrashReport("manual capture");

    EXPECT_EQ(report.reportKind, CrashReportKinds::ManualCrashReport);
    EXPECT_EQ(report.reason, "manual capture");
    EXPECT_EQ(report.generationSequence, 1u);
    ASSERT_EQ(report.healthMetrics.size(), 2u);
    EXPECT_EQ(report.healthMetrics[0].name, "server.tick.ms");
    EXPECT_EQ(report.healthMetrics[0].type, "timing");
    EXPECT_EQ(report.healthMetrics[1].name, "world.entities.active");
    EXPECT_EQ(report.summary.healthMetricCount, 2u);
}

TEST(DiagnosticReliabilityTests, BuildCrashReportIncludesAliveLeaksOnly)
{
    DiagnosticSystem diagnostics;
    const auto alive = diagnostics.Lifetime().Register(
        "PlayerEntity",
        "World",
        100,
        5);
    const auto destroyed = diagnostics.Lifetime().Register(
        "Projectile",
        "World",
        101,
        6);
    ASSERT_TRUE(diagnostics.Lifetime().MarkDestroyed(destroyed, 7));

    const auto report = diagnostics.BuildCrashReport("leak capture");

    ASSERT_EQ(report.lifetimeLeaks.size(), 1u);
    EXPECT_EQ(report.lifetimeLeaks[0].objectId, alive.objectId);
    EXPECT_EQ(report.lifetimeLeaks[0].typeName, "PlayerEntity");
    EXPECT_EQ(report.summary.lifetimeLeakCount, 1u);
    EXPECT_EQ(report.lifetimeLeakSummary.totalActive, 1u);
}

TEST(DiagnosticReliabilityTests, BuildCrashReportIncludesRecentLogEvents)
{
    DiagnosticSystem diagnostics;
    diagnostics.Log().Log(Level::Error,
                          "Runtime",
                          "fatal path reached",
                          {{"reason", "test"}});

    const auto report = diagnostics.BuildCrashReport("fatal path");

    ASSERT_EQ(report.recentLogEvents.size(), 1u);
    EXPECT_EQ(report.recentLogEvents[0].level, Level::Error);
    EXPECT_EQ(report.recentLogEvents[0].tag, "Runtime");
    EXPECT_EQ(report.recentLogEvents[0].message, "fatal path reached");
    ASSERT_EQ(report.recentLogEvents[0].context.size(), 1u);
    EXPECT_EQ(report.recentLogEvents[0].context[0].first, "reason");
}

TEST(DiagnosticReliabilityTests, HealthRulesEvaluateGaugeGreaterThan)
{
    HealthMonitor health;
    health.SetGauge("world.entities.active", 12.0);
    health.RegisterRule(Rule("entities-limit",
                             "world.entities.active",
                             HealthRuleType::GaugeGreaterThan,
                             10.0));
    health.RegisterRule(Rule("entities-safe",
                             "world.entities.active",
                             HealthRuleType::GaugeGreaterThan,
                             20.0));

    const auto results = health.EvaluateRules(health.Snapshot());

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].ruleName, "entities-limit");
    EXPECT_TRUE(results[0].evaluated);
    EXPECT_FALSE(results[0].passed);
    EXPECT_EQ(results[0].message, "Threshold exceeded.");
    EXPECT_EQ(results[1].ruleName, "entities-safe");
    EXPECT_TRUE(results[1].passed);
}

TEST(DiagnosticReliabilityTests, HealthRulesEvaluateGaugeLessThan)
{
    HealthMonitor health;
    health.SetGauge("net.sessions.active", 1.0);
    health.RegisterRule(Rule("session-min",
                             "net.sessions.active",
                             HealthRuleType::GaugeLessThan,
                             2.0));
    health.RegisterRule(Rule("session-floor",
                             "net.sessions.active",
                             HealthRuleType::GaugeLessThan,
                             1.0));

    const auto results = health.EvaluateRules(health.Snapshot());

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].ruleName, "session-floor");
    EXPECT_TRUE(results[0].passed);
    EXPECT_EQ(results[1].ruleName, "session-min");
    EXPECT_FALSE(results[1].passed);
    EXPECT_EQ(results[1].message, "Threshold undercut.");
}

TEST(DiagnosticReliabilityTests, HealthRulesEvaluateCounterGreaterThan)
{
    HealthMonitor health;
    health.IncrementCounter("server.packets.rejected", 3.0);
    health.RegisterRule(Rule("packet-reject-limit",
                             "server.packets.rejected",
                             HealthRuleType::CounterGreaterThan,
                             2.0,
                             HealthSeverity::Error));
    health.RegisterRule(Rule("packet-reject-safe",
                             "server.packets.rejected",
                             HealthRuleType::CounterGreaterThan,
                             4.0));

    const auto results = health.EvaluateRules(health.Snapshot());

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].severity, HealthSeverity::Error);
    EXPECT_FALSE(results[0].passed);
    EXPECT_TRUE(results[1].passed);
}

TEST(DiagnosticReliabilityTests, HealthRulesEvaluateTimingGreaterThan)
{
    HealthMonitor health;
    health.RecordTiming("server.tick.ms", 15.0);
    health.RegisterRule(Rule("slow-tick",
                             "server.tick.ms",
                             HealthRuleType::TimingGreaterThan,
                             10.0,
                             HealthSeverity::Critical));
    health.RegisterRule(Rule("tick-budget",
                             "server.tick.ms",
                             HealthRuleType::TimingGreaterThan,
                             20.0));

    const auto results = health.EvaluateRules(health.Snapshot());

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].ruleName, "slow-tick");
    EXPECT_EQ(results[0].severity, HealthSeverity::Critical);
    EXPECT_FALSE(results[0].passed);
    EXPECT_EQ(results[1].ruleName, "tick-budget");
    EXPECT_TRUE(results[1].passed);
}

TEST(DiagnosticReliabilityTests, HealthRulesMissingMetricIsDeterministic)
{
    HealthMonitor health;
    health.RegisterRule(Rule("missing",
                             "memory.process.mb",
                             HealthRuleType::GaugeGreaterThan,
                             512.0));

    const auto results = health.EvaluateRules(health.Snapshot());

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].evaluated);
    EXPECT_FALSE(results[0].passed);
    EXPECT_EQ(results[0].observedValue, 0.0);
    EXPECT_EQ(results[0].message, "Metric not present.");
}

TEST(DiagnosticReliabilityTests, HealthRuleViolationsAppearInCrashReport)
{
    DiagnosticSystem diagnostics;
    diagnostics.Health().RecordTiming("server.tick.ms", 22.0);
    diagnostics.Health().RegisterRule(Rule("slow-tick",
                                           "server.tick.ms",
                                           HealthRuleType::TimingGreaterThan,
                                           16.0,
                                           HealthSeverity::Critical));
    diagnostics.Health().RegisterRule(Rule("missing-memory",
                                           "memory.process.mb",
                                           HealthRuleType::GaugeGreaterThan,
                                           512.0));

    const auto report = diagnostics.BuildCrashReport("rule capture");

    ASSERT_EQ(report.healthRuleViolations.size(), 1u);
    EXPECT_EQ(report.healthRuleViolations[0].ruleName, "slow-tick");
    EXPECT_EQ(report.healthRuleViolations[0].severity, HealthSeverity::Critical);
    EXPECT_EQ(report.summary.healthRuleViolationCount, 1u);

    const auto json = WriteToJson(report);
    EXPECT_EQ(json["healthRuleViolations"][0]["type"], "timing_greater_than");
    EXPECT_EQ(json["healthRuleViolations"][0]["severity"], "critical");
}

TEST(DiagnosticReliabilityTests, LifetimeLeakSummariesAreDeterministic)
{
    DiagnosticSystem diagnostics;
    const auto first = diagnostics.Lifetime().Register(
        "PlayerEntity",
        "World",
        10,
        1);
    const auto second = diagnostics.Lifetime().Register(
        "PlayerEntity",
        "World",
        11,
        2);
    const auto third = diagnostics.Lifetime().Register(
        "Session",
        "Networking",
        12,
        3);
    (void)first;
    (void)second;
    (void)third;

    const auto report = diagnostics.BuildCrashReport("summary capture");

    EXPECT_EQ(report.lifetimeLeakSummary.totalActive, 3u);
    EXPECT_EQ(report.lifetimeLeakSummary.byType.at("PlayerEntity"), 2u);
    EXPECT_EQ(report.lifetimeLeakSummary.byType.at("Session"), 1u);
    EXPECT_EQ(report.lifetimeLeakSummary.byOwnerSystem.at("World"), 2u);
    EXPECT_EQ(report.lifetimeLeakSummary.byOwnerSystem.at("Networking"), 1u);

    const auto json = WriteToJson(report);
    EXPECT_EQ(json["lifetimeLeakSummary"]["totalActive"], 3u);
    EXPECT_EQ(json["lifetimeLeakSummary"]["byType"]["PlayerEntity"], 2u);
    EXPECT_EQ(json["lifetimeLeakSummary"]["byOwnerSystem"]["World"], 2u);
}

TEST(DiagnosticReliabilityTests, ReliabilityReportUsesRequestedKindAndMetadata)
{
    DiagnosticSystem diagnostics;

    const auto report = diagnostics.BuildReliabilityReport(
        CrashReportKinds::ReliabilityFailureReport,
        "manual reliability capture",
        {{"zone", "alpha"}, {"reason_code", "manual"}});

    EXPECT_EQ(report.reportKind, CrashReportKinds::ReliabilityFailureReport);
    EXPECT_EQ(report.reason, "manual reliability capture");
    EXPECT_EQ(report.metadata.at("reason_code"), "manual");
    EXPECT_EQ(report.metadata.at("zone"), "alpha");
}

TEST(DiagnosticReliabilityTests, CrashReportJsonOutputOrderIsDeterministic)
{
    CrashReport report;
    report.reportKind = CrashReportKinds::SlowTickReport;
    report.reason = "slow tick";
    report.generationSequence = 7;
    report.threadId = "test-thread";
    report.metadata["zone"] = "alpha";
    report.metadata["build"] = "debug";
    report.healthMetrics.push_back({"server.tick.ms", "timing", 20.0});
    report.healthRuleViolations.push_back({
        "slow-tick",
        "server.tick.ms",
        HealthRuleType::TimingGreaterThan,
        HealthSeverity::Warning,
        16.0,
        20.0,
        true,
        false,
        "Threshold exceeded."});
    report.summary.healthMetricCount = 1;
    report.summary.healthRuleViolationCount = 1;

    const auto first = WriteToString(report);
    const auto second = WriteToString(report);

    EXPECT_EQ(first, second);
    EXPECT_LT(first.find("\"schemaVersion\""), first.find("\"reportKind\""));
    EXPECT_LT(first.find("\"healthMetrics\""),
              first.find("\"healthRuleViolations\""));
    EXPECT_LT(first.find("\"build\""), first.find("\"zone\""));
}

TEST(DiagnosticReliabilityTests, InvalidCrashReportOutputPathFailureIsDeterministic)
{
    const auto root = TestRoot();
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto fileParent = root / "not-a-directory";
    {
        std::ofstream marker(fileParent);
        marker << "file";
    }

    CrashReport report;
    report.reportKind = CrashReportKinds::ManualCrashReport;
    report.reason = "manual";

    const auto result = DiagnosticReportWriter::WriteJsonToFile(
        report,
        fileParent / "crash.json");

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              DiagnosticReportWriterDiagnostics::OutputDirectoryFailed);
    ASSERT_TRUE(result.diagnostics[0].outputPath.has_value());
    EXPECT_EQ(*result.diagnostics[0].outputPath, fileParent / "crash.json");
}

TEST(DiagnosticReliabilityTests, WriteCrashReportWritesManualCrashReport)
{
    const auto root = TestRoot();
    std::filesystem::remove_all(root);

    DiagnosticSystem diagnostics;
    diagnostics.Health().SetGauge("world.entities.active", 4.0);

    const auto outputPath = root / "reports" / "crash.json";
    const auto result = diagnostics.WriteCrashReport(
        outputPath,
        "manual write",
        {{"source", "test"}});

    ASSERT_TRUE(result.Succeeded());
    std::ifstream input(outputPath);
    ASSERT_TRUE(input.is_open());

    const auto json = nlohmann::json::parse(input);
    EXPECT_EQ(json["reportKind"], CrashReportKinds::ManualCrashReport);
    EXPECT_EQ(json["reason"], "manual write");
    EXPECT_EQ(json["metadata"]["source"], "test");
    EXPECT_EQ(json["healthMetrics"][0]["name"], "world.entities.active");
}
