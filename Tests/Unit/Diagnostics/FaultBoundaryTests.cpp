/// @file FaultBoundaryTests.cpp
/// @brief Verifies minimal fault boundary diagnostics contracts.

#include <SagaEngine/Diagnostics/DiagnosticConfig.hpp>
#include <SagaEngine/Diagnostics/DiagnosticSystem.hpp>
#include <SagaEngine/Diagnostics/Fault/FaultBoundary.hpp>
#include <SagaEngine/Diagnostics/Report/DiagnosticReportWriter.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <sstream>
#include <string>

namespace
{

using SagaEngine::Diagnostics::DiagnosticConfig;
using SagaEngine::Diagnostics::DiagnosticReport;
using SagaEngine::Diagnostics::DiagnosticReportWriter;
using SagaEngine::Diagnostics::DiagnosticSystem;
using SagaEngine::Diagnostics::FaultBoundary;
using SagaEngine::Diagnostics::FaultPolicy;
using SagaEngine::Diagnostics::FaultSeverity;
using SagaEngine::Diagnostics::RecoveryAction;

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

TEST(FaultBoundaryTests, PolicyClassifiesRecommendedActions)
{
    EXPECT_EQ(FaultBoundary::Recommend(FaultPolicy::Recoverable),
              RecoveryAction::RetryAllowed);
    EXPECT_EQ(FaultBoundary::Recommend(FaultPolicy::NonRecoverable),
              RecoveryAction::ShutdownRequested);
    EXPECT_EQ(FaultBoundary::Recommend(FaultPolicy::DeferToCaller),
              RecoveryAction::None);
    EXPECT_EQ(FaultBoundary::Recommend(FaultPolicy::ReportOnly),
              RecoveryAction::ReportOnly);
    EXPECT_EQ(FaultBoundary::Recommend(FaultPolicy::BlockStartup),
              RecoveryAction::BlockStartup);
}

TEST(FaultBoundaryTests, BuildReportSortsMetadataAndReportsAction)
{
    const auto report = FaultBoundary::BuildReport(
        "Runtime.Startup.ManifestMissing",
        "RuntimeStartupPreflight",
        FaultSeverity::Error,
        FaultPolicy::BlockStartup,
        "startup manifest missing",
        "Runtime.StartupPreflight.PackageManifestMissing",
        {{"zeta", "last"}, {"alpha", "first"}});

    EXPECT_EQ(report.faultId, "Runtime.Startup.ManifestMissing");
    EXPECT_EQ(report.recommendedAction, RecoveryAction::BlockStartup);
    ASSERT_EQ(report.metadata.size(), 2u);
    EXPECT_EQ(report.metadata[0].first, "alpha");
    EXPECT_EQ(report.metadata[1].first, "zeta");
}

TEST(FaultBoundaryTests, DiagnosticsReceivesFaultEvent)
{
    DiagnosticSystem diagnostics;
    ASSERT_TRUE(diagnostics.Faults().RecordFault(FaultBoundary::BuildReport(
        "Server.Session.Disconnect",
        "ZoneServer",
        FaultSeverity::Warning,
        FaultPolicy::ReportOnly,
        "session disconnected during diagnostics smoke",
        "Server.Session.Disconnect")));

    const auto snapshot = diagnostics.Faults().Snapshot();

    ASSERT_EQ(snapshot.reports.size(), 1u);
    EXPECT_EQ(snapshot.reports[0].sequence, 1u);
    EXPECT_EQ(snapshot.reports[0].subsystem, "ZoneServer");
    EXPECT_EQ(snapshot.summary.faultCount, 1u);
    EXPECT_EQ(snapshot.summary.droppedFaultCount, 0u);
}

TEST(FaultBoundaryTests, OperationalReportSerializesFaults)
{
    DiagnosticSystem diagnostics;
    ASSERT_TRUE(diagnostics.Faults().RecordFault(FaultBoundary::BuildReport(
        "Runtime.Service.TickFailure",
        "RuntimeServiceRegistry",
        FaultSeverity::Error,
        FaultPolicy::DeferToCaller,
        "service tick failed",
        "Runtime.Service.TickFailure")));

    const auto json = WriteToJson(diagnostics.BuildOperationalSnapshot());

    ASSERT_TRUE(json.contains("faults"));
    ASSERT_EQ(json["faults"]["reports"].size(), 1u);
    EXPECT_EQ(json["faults"]["reports"][0]["sequence"], 1u);
    EXPECT_EQ(json["faults"]["reports"][0]["severity"], "Error");
    EXPECT_EQ(json["faults"]["reports"][0]["policy"], "DeferToCaller");
    EXPECT_EQ(json["faults"]["reports"][0]["recommendedAction"], "None");
    EXPECT_EQ(json["faults"]["summary"]["faultCount"], 1u);
    EXPECT_EQ(json["summary"]["faultReportCount"], 1u);
}

TEST(FaultBoundaryTests, EmptyFaultSectionSerializesDeterministically)
{
    DiagnosticSystem diagnostics;

    const auto json = WriteToJson(diagnostics.BuildOperationalSnapshot());

    ASSERT_TRUE(json.contains("faults"));
    EXPECT_TRUE(json["faults"]["reports"].empty());
    EXPECT_EQ(json["faults"]["summary"]["faultCount"], 0u);
    EXPECT_EQ(json["faults"]["summary"]["droppedFaultCount"], 0u);
    EXPECT_EQ(json["summary"]["faultReportCount"], 0u);
}

TEST(FaultBoundaryTests, FaultTrackerIsBounded)
{
    DiagnosticConfig config;
    config.maxFaultReports = 1;
    DiagnosticSystem diagnostics(config);

    EXPECT_TRUE(diagnostics.Faults().RecordFault(FaultBoundary::BuildReport(
        "Fault.One",
        "Runtime",
        FaultSeverity::Warning,
        FaultPolicy::ReportOnly,
        "first")));
    EXPECT_FALSE(diagnostics.Faults().RecordFault(FaultBoundary::BuildReport(
        "Fault.Two",
        "Runtime",
        FaultSeverity::Warning,
        FaultPolicy::ReportOnly,
        "second")));

    const auto snapshot = diagnostics.Faults().Snapshot();
    ASSERT_EQ(snapshot.reports.size(), 1u);
    EXPECT_EQ(snapshot.summary.droppedFaultCount, 1u);
}

TEST(FaultBoundaryTests, ReportConstructionIsDeterministicAndDoesNotExecuteRecovery)
{
    bool recoveryExecuted = false;
    DiagnosticSystem diagnostics;
    ASSERT_TRUE(diagnostics.Faults().RecordFault(FaultBoundary::BuildReport(
        "Runtime.Service.Failure",
        "RuntimeServiceRegistry",
        FaultSeverity::Critical,
        FaultPolicy::NonRecoverable,
        "service failed")));

    const auto report = diagnostics.BuildOperationalSnapshot();
    const std::string first = WriteToString(report);
    const std::string second = WriteToString(report);

    EXPECT_FALSE(recoveryExecuted);
    EXPECT_EQ(first, second);
    EXPECT_NE(first.find("\"faults\""), std::string::npos);
    EXPECT_NE(second.find("\"faults\""), std::string::npos);
    const auto snapshot = diagnostics.Faults().Snapshot();
    ASSERT_EQ(snapshot.reports.size(), 1u);
    EXPECT_EQ(snapshot.reports[0].sequence, 1u);
    EXPECT_EQ(snapshot.summary.droppedFaultCount, 0u);
}
