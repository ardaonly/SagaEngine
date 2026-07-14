/// @file SagaChaosLabTests.cpp
/// @brief Verifies bounded SagaChaosLab chaos profile execution.

#include "SagaChaosLab/ChaosProfile.hpp"
#include "SagaChaosLab/ChaosRunner.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

std::filesystem::path TestRoot()
{
    return std::filesystem::temp_directory_path() / "saga_chaos_lab_tests";
}

nlohmann::json ValidProfileJson(const std::filesystem::path& reportDirectory)
{
    return {
        {"schemaVersion", 1},
        {"profileName", "unit_profile"},
        {"scenario", "direct_zone_packet_chaos_smoke"},
        {"tier", "smoke"},
        {"manualOnly", false},
        {"seed", 12345},
        {"actors", 3},
        {"ticks", 10},
        {"maxDurationSec", 10},
        {"reportDirectory", reportDirectory.generic_string()},
        {"chaos",
         {
             {"dropPermille", 100},
             {"duplicatePermille", 200},
             {"deferPermille", 300},
             {"deferTicks", 2},
             {"maxDeferredFrames", 12},
         }},
    };
}

std::filesystem::path WriteProfile(const std::string& name,
                                   const nlohmann::json& value)
{
    const auto root = TestRoot() / "profiles";
    std::filesystem::create_directories(root);
    const auto path = root / name;
    std::ofstream output(path);
    output << value.dump(2) << '\n';
    return path;
}

nlohmann::json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path);
    EXPECT_TRUE(input.is_open());
    return nlohmann::json::parse(input);
}

bool HasDiagnostic(const std::vector<std::string>& diagnostics,
                   const std::string& expected)
{
    return std::find(diagnostics.begin(), diagnostics.end(), expected) !=
           diagnostics.end();
}

} // namespace

TEST(SagaChaosLabTests, DefaultSmokeProfileValidatesAndIsBounded)
{
    const auto profile = SagaChaosLab::DefaultSmokeChaosProfile();
    const auto result = SagaChaosLab::ValidateChaosProfile(profile);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(profile.schemaVersion, 1u);
    EXPECT_EQ(profile.scenario, "direct_zone_packet_chaos_smoke");
    EXPECT_EQ(profile.tier, SagaStressArena::StressTier::Smoke);
    EXPECT_FALSE(profile.manualOnly);
    EXPECT_LE(profile.actors, 10u);
    EXPECT_LE(profile.ticks, 100u);
    EXPECT_LE(profile.maxDurationSec, 10u);
    EXPECT_GT(profile.chaosConfig.dropPermille, 0u);
    EXPECT_GT(profile.chaosConfig.duplicatePermille, 0u);
    EXPECT_GT(profile.chaosConfig.deferPermille, 0u);
    EXPECT_LE(profile.chaosConfig.maxDeferredFrames, 40u);
}

TEST(SagaChaosLabTests, ValidProfileLoadsAndMapsChaosConfigExactly)
{
    const auto reportRoot = TestRoot() / "valid_profile";
    const auto path = WriteProfile(
        "valid_profile.json",
        ValidProfileJson(reportRoot));

    const auto loaded = SagaChaosLab::LoadChaosProfile(path);

    ASSERT_TRUE(loaded.Succeeded());
    EXPECT_EQ(loaded.profile.profileName, "unit_profile");
    EXPECT_EQ(loaded.profile.seed, 12345u);
    EXPECT_TRUE(loaded.profile.chaosConfig.enabled);
    EXPECT_EQ(loaded.profile.chaosConfig.seed, 12345u);
    EXPECT_EQ(loaded.profile.chaosConfig.dropPermille, 100u);
    EXPECT_EQ(loaded.profile.chaosConfig.duplicatePermille, 200u);
    EXPECT_EQ(loaded.profile.chaosConfig.deferPermille, 300u);
    EXPECT_EQ(loaded.profile.chaosConfig.deferTicks, 2u);
    EXPECT_EQ(loaded.profile.chaosConfig.maxDeferredFrames, 12u);
}

TEST(SagaChaosLabTests, InvalidSchemaVersionFailsDeterministically)
{
    auto json = ValidProfileJson(TestRoot() / "invalid_schema");
    json["schemaVersion"] = 2;

    const auto loaded = SagaChaosLab::LoadChaosProfile(
        WriteProfile("invalid_schema.json", json));

    ASSERT_FALSE(loaded.Succeeded());
    EXPECT_TRUE(HasDiagnostic(loaded.diagnostics, "schemaVersion must be 1."));
}

TEST(SagaChaosLabTests, MissingRequiredFieldsFailClearly)
{
    auto json = ValidProfileJson(TestRoot() / "missing_fields");
    json.erase("actors");
    json["chaos"].erase("deferTicks");

    const auto loaded = SagaChaosLab::LoadChaosProfile(
        WriteProfile("missing_fields.json", json));

    ASSERT_FALSE(loaded.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        loaded.diagnostics,
        "Missing required field: actors."));
    EXPECT_TRUE(HasDiagnostic(
        loaded.diagnostics,
        "Missing required field: deferTicks."));
}

TEST(SagaChaosLabTests, InvalidProbabilitiesAndSumFail)
{
    auto invalidProbability = ValidProfileJson(TestRoot() / "bad_probability");
    invalidProbability["chaos"]["dropPermille"] = 1001;

    const auto probabilityResult = SagaChaosLab::LoadChaosProfile(
        WriteProfile("bad_probability.json", invalidProbability));
    ASSERT_FALSE(probabilityResult.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        probabilityResult.diagnostics,
        "chaos probability fields must be between 0 and 1000."));

    auto invalidSum = ValidProfileJson(TestRoot() / "bad_sum");
    invalidSum["chaos"]["dropPermille"] = 400;
    invalidSum["chaos"]["duplicatePermille"] = 400;
    invalidSum["chaos"]["deferPermille"] = 300;

    const auto sumResult = SagaChaosLab::LoadChaosProfile(
        WriteProfile("bad_sum.json", invalidSum));
    ASSERT_FALSE(sumResult.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        sumResult.diagnostics,
        "chaos probability sum must be <= 1000."));
}

TEST(SagaChaosLabTests, UnboundedExecutionAndDeferQueueFail)
{
    auto json = ValidProfileJson(TestRoot() / "unbounded");
    json["actors"] = 11;
    json["ticks"] = 101;
    json["maxDurationSec"] = 11;
    json["chaos"]["deferTicks"] = 101;
    json["chaos"]["maxDeferredFrames"] = 41;

    const auto loaded = SagaChaosLab::LoadChaosProfile(
        WriteProfile("unbounded.json", json));

    ASSERT_FALSE(loaded.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        loaded.diagnostics,
        "actors must be nonzero and within tier bounds."));
    EXPECT_TRUE(HasDiagnostic(
        loaded.diagnostics,
        "ticks must be nonzero and within tier bounds."));
    EXPECT_TRUE(HasDiagnostic(
        loaded.diagnostics,
        "maxDurationSec must be nonzero and within tier bounds."));
    EXPECT_TRUE(HasDiagnostic(
        loaded.diagnostics,
        "chaos.deferTicks must be nonzero and within tier bounds."));
    EXPECT_TRUE(HasDiagnostic(
        loaded.diagnostics,
        "chaos.maxDeferredFrames must be nonzero and within tier bounds."));
}

TEST(SagaChaosLabTests, UnsupportedFieldsFail)
{
    auto json = ValidProfileJson(TestRoot() / "unsupported");
    json["socketChaos"] = true;
    json["chaos"]["latencyMs"] = 120;

    const auto loaded = SagaChaosLab::LoadChaosProfile(
        WriteProfile("unsupported.json", json));

    ASSERT_FALSE(loaded.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        loaded.diagnostics,
        "Unsupported profile field: socketChaos."));
    EXPECT_TRUE(HasDiagnostic(
        loaded.diagnostics,
        "Unsupported chaos field: latencyMs."));
}

TEST(SagaChaosLabTests, ManualHeavyProfileRejectedWithoutAllowManual)
{
    auto profile = SagaChaosLab::DefaultSmokeChaosProfile();
    profile.profileName = "manual_heavy";
    profile.tier = SagaStressArena::StressTier::Heavy;
    profile.manualOnly = true;
    profile.actors = 100;
    profile.ticks = 5000;
    profile.maxDurationSec = 300;
    profile.reportDirectory = TestRoot() / "manual_heavy";

    const auto result = SagaChaosLab::ChaosRunner::Run(profile);

    EXPECT_EQ(result.status, SagaChaosLab::ChaosRunStatus::ManualOnlyBlocked);
    EXPECT_EQ(result.exitCode, 5);
    EXPECT_FALSE(std::filesystem::exists(result.chaosReportPath));
}

TEST(SagaChaosLabTests, SmokeRunWritesChaosReportWithDeterministicSummary)
{
    const auto root = TestRoot() / "smoke_run";
    std::filesystem::remove_all(root);

    auto profile = SagaChaosLab::DefaultSmokeChaosProfile();
    profile.reportDirectory = root;

    const auto result = SagaChaosLab::ChaosRunner::Run(profile);

    ASSERT_TRUE(result.Succeeded()) << result.message;
    EXPECT_TRUE(std::filesystem::exists(result.chaosReportPath));
    EXPECT_EQ(result.chaosMetrics.at("net.chaos.frames_seen"), 30.0);

    const auto report = ReadJson(result.chaosReportPath);
    EXPECT_EQ(report.at("schemaVersion"), 1);
    EXPECT_EQ(report.at("tool"), "SagaChaosLab");
    EXPECT_EQ(report.at("status"), "succeeded");
    EXPECT_EQ(report.at("profileName"), "default_smoke");
    EXPECT_EQ(report.at("profileSafety").at("scenario"),
              "direct_zone_packet_chaos_smoke");
    EXPECT_EQ(report.at("chaosConfig").at("seed"), profile.seed);
    EXPECT_EQ(report.at("chaosMetrics").at("net.chaos.frames_seen"), 30.0);
    EXPECT_GT(
        report.at("chaosMetrics").at("net.chaos.frames_dropped").get<double>(),
        0.0);
    EXPECT_TRUE(std::filesystem::exists(
        result.scenarioResult.reportPaths.operationalSnapshot));
}
