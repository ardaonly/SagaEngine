/// @file DiagnosticReportTests.cpp
/// @brief Verifies Engine diagnostics report generation and JSON writing.

#include <SagaEngine/Diagnostics/DiagnosticSystem.hpp>
#include <SagaEngine/Diagnostics/Report/DiagnosticReportWriter.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{

using SagaEngine::Core::Log::Level;
using SagaEngine::Diagnostics::DiagnosticReport;
using SagaEngine::Diagnostics::DiagnosticReportWriter;
using SagaEngine::Diagnostics::DiagnosticSystem;
namespace DiagnosticReportKinds = SagaEngine::Diagnostics::DiagnosticReportKinds;
namespace DiagnosticReportWriterDiagnostics =
    SagaEngine::Diagnostics::DiagnosticReportWriterDiagnostics;

std::filesystem::path TestRoot()
{
    return std::filesystem::temp_directory_path() /
           "saga_diagnostic_report_tests";
}

nlohmann::json WriteToJson(const DiagnosticReport& report)
{
    std::ostringstream output;
    const auto result = DiagnosticReportWriter::WriteJson(report, output);
    EXPECT_TRUE(result.Succeeded());
    return nlohmann::json::parse(output.str());
}

std::string WriteToString(const DiagnosticReport& report)
{
    std::ostringstream output;
    const auto result = DiagnosticReportWriter::WriteJson(report, output);
    EXPECT_TRUE(result.Succeeded());
    return output.str();
}

} // namespace

TEST(DiagnosticReportTests, DefaultReportConstructionIsMinimal)
{
    const DiagnosticReport report;

    EXPECT_EQ(report.schemaVersion, 1u);
    EXPECT_TRUE(report.reportKind.empty());
    EXPECT_EQ(report.generationSequence, 0u);
    EXPECT_EQ(report.summary.healthMetricCount, 0u);
    EXPECT_TRUE(report.healthMetrics.empty());
    EXPECT_TRUE(report.lifetimeLeaks.empty());
    EXPECT_TRUE(report.recentLogEvents.empty());
    EXPECT_TRUE(report.metadata.empty());
}

TEST(DiagnosticReportTests, OperationalSnapshotIncludesHealthMetrics)
{
    DiagnosticSystem diagnostics;
    diagnostics.Health().SetGauge("world.entities.active", 3.0);
    diagnostics.Health().IncrementCounter("server.tick.count", 2.0);

    const auto report = diagnostics.BuildOperationalSnapshot();

    EXPECT_EQ(report.reportKind, DiagnosticReportKinds::OperationalSnapshot);
    EXPECT_EQ(report.generationSequence, 1u);
    ASSERT_EQ(report.healthMetrics.size(), 2u);
    EXPECT_EQ(report.healthMetrics[0].name, "server.tick.count");
    EXPECT_EQ(report.healthMetrics[0].type, "counter");
    EXPECT_DOUBLE_EQ(report.healthMetrics[0].value, 2.0);
    EXPECT_EQ(report.healthMetrics[1].name, "world.entities.active");
    EXPECT_EQ(report.summary.healthMetricCount, 2u);
}

TEST(DiagnosticReportTests, LifetimeLeakReportIncludesAliveRecordsOnly)
{
    DiagnosticSystem diagnostics;
    const auto alive = diagnostics.Lifetime().Register(
        "PlayerEntity",
        "World",
        1001,
        7);
    const auto destroyed = diagnostics.Lifetime().Register(
        "Projectile",
        "World",
        1002,
        8);
    ASSERT_TRUE(diagnostics.Lifetime().MarkDestroyed(destroyed, 9));

    const auto report = diagnostics.BuildLifetimeLeakReport();

    EXPECT_EQ(report.reportKind, DiagnosticReportKinds::LifetimeLeaks);
    ASSERT_EQ(report.lifetimeLeaks.size(), 1u);
    EXPECT_EQ(report.lifetimeLeaks[0].objectId, alive.objectId);
    EXPECT_EQ(report.lifetimeLeaks[0].externalId, 1001u);
    EXPECT_EQ(report.lifetimeLeaks[0].typeName, "PlayerEntity");
    EXPECT_EQ(report.summary.lifetimeLeakCount, 1u);
}

TEST(DiagnosticReportTests, RecentLogEventsPreserveStructuredFields)
{
    DiagnosticSystem diagnostics;
    diagnostics.Log().Log(Level::Warn,
                          "Net.Session",
                          "Client disconnected",
                          {{"client_id", "42"}, {"reason", "timeout"}});

    const auto report = diagnostics.BuildOperationalSnapshot();

    ASSERT_EQ(report.recentLogEvents.size(), 1u);
    EXPECT_EQ(report.recentLogEvents[0].level, Level::Warn);
    EXPECT_EQ(report.recentLogEvents[0].tag, "Net.Session");
    EXPECT_EQ(report.recentLogEvents[0].message, "Client disconnected");
    ASSERT_EQ(report.recentLogEvents[0].context.size(), 2u);
    EXPECT_EQ(report.recentLogEvents[0].context[0].first, "client_id");
    EXPECT_EQ(report.recentLogEvents[0].context[1].second, "timeout");

    const auto json = WriteToJson(report);
    EXPECT_EQ(json["recentLogEvents"][0]["level"], "Warn");
    EXPECT_EQ(json["recentLogEvents"][0]["context"][0]["key"], "client_id");
}

TEST(DiagnosticReportTests, JsonOutputOrderIsDeterministic)
{
    DiagnosticReport report;
    report.reportKind = DiagnosticReportKinds::OperationalSnapshot;
    report.generationSequence = 5;
    report.metadata["zone"] = "alpha";
    report.metadata["build"] = "debug";
    report.healthMetrics.push_back({"server.tick.count", "counter", 4.0});
    report.summary.healthMetricCount = 1;

    const std::string first = WriteToString(report);
    const std::string second = WriteToString(report);

    EXPECT_EQ(first, second);
    EXPECT_LT(first.find("\"schemaVersion\""), first.find("\"reportKind\""));
    EXPECT_LT(first.find("\"healthMetrics\""), first.find("\"lifetimeLeaks\""));
    EXPECT_LT(first.find("\"build\""), first.find("\"zone\""));
}

TEST(DiagnosticReportTests, InvalidOutputPathFailureIsDeterministic)
{
    const auto root = TestRoot();
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto fileParent = root / "not-a-directory";
    {
        std::ofstream marker(fileParent);
        marker << "file";
    }

    DiagnosticReport report;
    report.reportKind = DiagnosticReportKinds::HealthSnapshot;

    const auto result = DiagnosticReportWriter::WriteJsonToFile(
        report,
        fileParent / "report.json");

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              DiagnosticReportWriterDiagnostics::OutputDirectoryFailed);
    ASSERT_TRUE(result.diagnostics[0].outputPath.has_value());
    EXPECT_EQ(*result.diagnostics[0].outputPath, fileParent / "report.json");
}

TEST(DiagnosticReportTests, WriteOperationalReportWritesOperationalSnapshot)
{
    const auto root = TestRoot();
    std::filesystem::remove_all(root);

    DiagnosticSystem diagnostics;
    diagnostics.Health().SetGauge("world.entities.active", 2.0);

    const auto outputPath = root / "reports" / "operational.json";
    const auto result = diagnostics.WriteOperationalReport(outputPath);

    ASSERT_TRUE(result.Succeeded());
    std::ifstream input(outputPath);
    ASSERT_TRUE(input.is_open());

    const auto json = nlohmann::json::parse(input);
    EXPECT_EQ(json["reportKind"], DiagnosticReportKinds::OperationalSnapshot);
    EXPECT_EQ(json["generationSequence"], 1u);
    EXPECT_EQ(json["summary"]["healthMetricCount"], 1u);
    EXPECT_EQ(json["healthMetrics"][0]["name"], "world.entities.active");
}
