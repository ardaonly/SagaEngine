/// @file ChaosRunner.cpp
/// @brief Implements SagaChaosLab bounded chaos profile execution.

#include "SagaChaosLab/ChaosRunner.hpp"

#include "SagaStressArena/StressScenarioConfig.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <system_error>

namespace SagaChaosLab
{
namespace
{

using Json = nlohmann::ordered_json;
namespace NetworkChaosMetrics = SagaEngine::Networking::NetworkChaosMetrics;

constexpr int kExitSuccess = 0;
constexpr int kExitInvalidProfile = 4;
constexpr int kExitManualOnlyBlocked = 5;
constexpr int kExitScenarioFailed = 6;
constexpr int kExitReportWriteFailed = 7;

const std::array<const char*, 8> kChaosMetricNames = {
    NetworkChaosMetrics::FramesSeen,
    NetworkChaosMetrics::FramesDelivered,
    NetworkChaosMetrics::FramesDropped,
    NetworkChaosMetrics::FramesDuplicated,
    NetworkChaosMetrics::FramesDeferred,
    NetworkChaosMetrics::FramesReleased,
    NetworkChaosMetrics::QueueDepth,
    NetworkChaosMetrics::QueueFullDrops};

[[nodiscard]] std::filesystem::path ChaosReportPath(
    const ChaosProfile& profile)
{
    return profile.reportDirectory / "chaos_report.json";
}

[[nodiscard]] SagaStressArena::StressScenarioConfig BuildScenarioConfig(
    const ChaosProfile& profile)
{
    SagaStressArena::StressScenarioConfig config =
        SagaStressArena::DefaultStressScenarioConfig();
    config.scenario = profile.scenario;
    config.tier = profile.tier;
    config.actors = profile.actors;
    config.ticks = profile.ticks;
    config.maxDurationSec = profile.maxDurationSec;
    config.reportDirectory = profile.reportDirectory;
    config.chaosConfig = profile.chaosConfig;
    return config;
}

[[nodiscard]] std::map<std::string, double> EmptyChaosMetrics()
{
    std::map<std::string, double> metrics;
    for (const char* name : kChaosMetricNames)
    {
        metrics[name] = 0.0;
    }
    return metrics;
}

[[nodiscard]] std::map<std::string, double> ExtractChaosMetrics(
    const std::filesystem::path& operationalReport,
    std::vector<std::string>& diagnostics)
{
    auto metrics = EmptyChaosMetrics();

    std::ifstream input(operationalReport);
    if (!input.is_open())
    {
        diagnostics.push_back(
            "Operational stress report could not be opened for chaos summary.");
        return metrics;
    }

    nlohmann::json root;
    try
    {
        input >> root;
    }
    catch (const nlohmann::json::exception&)
    {
        diagnostics.push_back(
            "Operational stress report could not be parsed for chaos summary.");
        return metrics;
    }

    if (!root.contains("healthMetrics") || !root.at("healthMetrics").is_array())
    {
        diagnostics.push_back(
            "Operational stress report does not contain healthMetrics.");
        return metrics;
    }

    for (const auto& metric : root.at("healthMetrics"))
    {
        if (!metric.is_object() || !metric.contains("name") ||
            !metric.contains("value") || !metric.at("name").is_string() ||
            !metric.at("value").is_number())
        {
            continue;
        }

        const std::string name = metric.at("name").get<std::string>();
        if (metrics.find(name) != metrics.end())
        {
            metrics[name] = metric.at("value").get<double>();
        }
    }

    return metrics;
}

[[nodiscard]] Json BuildReportJson(const ChaosRunResult& result)
{
    const auto& profile = result.profile;
    const auto& chaos = profile.chaosConfig;
    const auto& paths = result.scenarioResult.reportPaths;

    Json diagnostics = Json::array();
    for (const auto& diagnostic : result.diagnostics)
    {
        diagnostics.push_back(diagnostic);
    }

    Json metrics = Json::object();
    for (const auto& [name, value] : result.chaosMetrics)
    {
        metrics[name] = value;
    }

    return Json{
        {"schemaVersion", 1},
        {"tool", "SagaChaosLab"},
        {"profileName", profile.profileName},
        {"status", ToString(result.status)},
        {"message", result.message},
        {"profileSafety",
         Json{
             {"scenario", profile.scenario},
             {"tier", SagaStressArena::ToString(profile.tier)},
             {"manualOnly", profile.manualOnly},
             {"actors", profile.actors},
             {"ticks", profile.ticks},
             {"maxDurationSec", profile.maxDurationSec},
             {"reportDirectory", profile.reportDirectory.generic_string()},
         }},
        {"chaosConfig",
         Json{
             {"seed", chaos.seed},
             {"dropPermille", chaos.dropPermille},
             {"duplicatePermille", chaos.duplicatePermille},
             {"deferPermille", chaos.deferPermille},
             {"deferTicks", chaos.deferTicks},
             {"maxDeferredFrames", chaos.maxDeferredFrames},
         }},
        {"reports",
         Json{
             {"chaosReport", result.chaosReportPath.generic_string()},
             {"operationalSnapshot",
              paths.operationalSnapshot.generic_string()},
             {"reliabilityReport", paths.reliabilityReport.generic_string()},
             {"lifetimeLeaks", paths.lifetimeLeaks.generic_string()},
         }},
        {"stressArena",
         Json{
             {"status", SagaStressArena::ToString(
                            result.scenarioResult.status)},
             {"message", result.scenarioResult.message},
         }},
        {"diagnostics", std::move(diagnostics)},
        {"chaosMetrics", std::move(metrics)},
    };
}

[[nodiscard]] bool WriteChaosReport(const ChaosRunResult& result)
{
    const auto parent = result.chaosReportPath.parent_path();
    if (!parent.empty())
    {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error)
        {
            return false;
        }
    }

    std::ofstream output(result.chaosReportPath);
    if (!output.is_open())
    {
        return false;
    }

    output << BuildReportJson(result).dump(2) << '\n';
    return output.good();
}

[[nodiscard]] ChaosRunResult MakeInvalidProfileResult(
    ChaosProfileResult profileResult)
{
    ChaosRunResult result;
    result.status = ChaosRunStatus::InvalidProfile;
    result.exitCode = kExitInvalidProfile;
    result.message = "Invalid SagaChaosLab profile.";
    result.profile = profileResult.profile;
    result.diagnostics = std::move(profileResult.diagnostics);
    result.chaosReportPath = ChaosReportPath(result.profile);
    return result;
}

} // namespace

ChaosRunResult ChaosRunner::Run(const ChaosProfile& profile,
                                const ChaosRunnerOptions& options)
{
    auto profileResult = ValidateChaosProfile(profile);
    if (!profileResult.Succeeded())
    {
        return MakeInvalidProfileResult(std::move(profileResult));
    }

    ChaosRunResult result;
    result.profile = profile;
    result.chaosReportPath = ChaosReportPath(profile);

    if ((profile.manualOnly ||
         profile.tier == SagaStressArena::StressTier::Heavy) &&
        !options.allowManual)
    {
        result.status = ChaosRunStatus::ManualOnlyBlocked;
        result.exitCode = kExitManualOnlyBlocked;
        result.message = "Manual-only chaos profile blocked.";
        result.diagnostics.push_back(
            "Manual-only or heavy chaos profiles require --allow-manual.");
        return result;
    }

    result.scenarioResult =
        SagaStressArena::ScenarioRunner::Run(BuildScenarioConfig(profile));
    result.diagnostics.insert(
        result.diagnostics.end(),
        result.scenarioResult.diagnostics.begin(),
        result.scenarioResult.diagnostics.end());
    result.chaosMetrics = ExtractChaosMetrics(
        result.scenarioResult.reportPaths.operationalSnapshot,
        result.diagnostics);

    if (!result.scenarioResult.Succeeded())
    {
        result.status = ChaosRunStatus::ScenarioFailed;
        result.exitCode = kExitScenarioFailed;
        result.message = "SagaStressArena scenario failed.";
    }
    else
    {
        result.status = ChaosRunStatus::Succeeded;
        result.exitCode = kExitSuccess;
        result.message = "Chaos profile completed.";
    }

    if (!WriteChaosReport(result))
    {
        result.status = ChaosRunStatus::ReportWriteFailed;
        result.exitCode = kExitReportWriteFailed;
        result.message = "SagaChaosLab report could not be written.";
        result.diagnostics.push_back("chaos_report.json write failed.");
    }

    return result;
}

} // namespace SagaChaosLab
