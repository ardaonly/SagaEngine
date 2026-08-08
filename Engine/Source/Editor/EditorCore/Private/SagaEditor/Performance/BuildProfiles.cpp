// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Performance/BuildProfiles.h"

#include <nlohmann/json.hpp>
#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>

namespace SagaEditor
{
namespace
{
namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr std::uintmax_t MaxProfileBytes = 1024 * 1024;
constexpr int MaxTrials = 10000;
constexpr int MaxIterations = 10000000;
constexpr int MaxWorkload = 1000000;

std::string Hash(std::string_view bytes)
{
    std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
    SHA256(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), digest.data());
    std::ostringstream output;
    output << "sha256:";
    for (const auto byte : digest)
        output << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return output.str();
}

void Add(std::vector<BuildProfileDiagnostic>& diagnostics,
    std::string code, std::string message)
{
    diagnostics.push_back({std::move(code), std::move(message)});
}

bool SafeEvidencePath(const fs::path& root, const fs::path& relative,
    std::vector<BuildProfileDiagnostic>& diagnostics)
{
    const auto normalized = relative.lexically_normal();
    if (relative.empty() || relative.is_absolute() || normalized.empty() ||
        normalized.generic_string().starts_with("../"))
    {
        Add(diagnostics, "BuildProfiles.EvidencePathEscapesProject",
            "Evidence output must be a relative path inside the project.");
        return false;
    }
    const auto required = fs::path("Build/Reports/Benchmarks");
    const auto underRequired = normalized == required ||
        normalized.generic_string().starts_with(required.generic_string() + "/");
    if (!underRequired)
    {
        Add(diagnostics, "BuildProfiles.EvidencePathOutsideReports",
            "Benchmark evidence must be written below Build/Reports/Benchmarks.");
        return false;
    }
    std::error_code error;
    auto cursor = fs::weakly_canonical(root, error);
    if (error)
    {
        Add(diagnostics, "BuildProfiles.ProjectRootInvalid", error.message());
        return false;
    }
    for (const auto& component : normalized)
    {
        cursor /= component;
        const auto status = fs::symlink_status(cursor, error);
        if (!error && fs::is_symlink(status))
        {
            Add(diagnostics, "BuildProfiles.EvidencePathSymlink",
                "Evidence output may not traverse a symbolic link.");
            return false;
        }
        error.clear();
    }
    return true;
}

bool IsIdentifier(const std::string& value)
{
    if (value.empty() || value.size() > 128) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char character)
    {
        return std::isalnum(character) || character == '-' || character == '_' || character == '.';
    });
}

double Percentile(const std::vector<double>& sorted, double percentile)
{
    const auto rank = static_cast<std::size_t>(
        std::ceil(percentile * static_cast<double>(sorted.size())));
    return sorted[std::clamp<std::size_t>(rank, 1, sorted.size()) - 1];
}
}

BuildProfilesLoadResult BuildProfiles::Load(const fs::path& profilePath,
    const fs::path& projectRoot)
{
    BuildProfilesLoadResult result;
    std::error_code error;
    if (!fs::is_regular_file(profilePath, error) || error ||
        fs::file_size(profilePath, error) > MaxProfileBytes || error)
    {
        Add(result.diagnostics, "BuildProfiles.InputInvalid",
            "BuildProfiles.saga.json must be a regular file no larger than 1 MiB.");
        return result;
    }
    Json root;
    try
    {
        std::ifstream input(profilePath, std::ios::binary);
        root = Json::parse(input);
    }
    catch (const std::exception& exception)
    {
        Add(result.diagnostics, "BuildProfiles.JsonInvalid", exception.what());
        return result;
    }
    if (!root.is_object() || !root.contains("schemaVersion") ||
        !root["schemaVersion"].is_number_integer() ||
        root["schemaVersion"].get<int>() != BuildProfilesDocument::CurrentSchemaVersion ||
        !root.contains("profiles") || !root["profiles"].is_array())
    {
        Add(result.diagnostics, "BuildProfiles.SchemaUnsupported",
            "Build profile schema version 1 with a profiles array is required.");
        return result;
    }
    if (root["profiles"].empty() || root["profiles"].size() > 128)
        Add(result.diagnostics, "BuildProfiles.ProfileCountInvalid",
            "Build profiles must contain between 1 and 128 entries.");
    std::set<std::string> ids;
    std::set<std::string> benchmarkScenarios;
    for (const auto& value : root["profiles"])
    {
        if (!value.is_object())
        {
            Add(result.diagnostics, "BuildProfiles.ProfileInvalid", "Every profile must be an object.");
            continue;
        }
        try
        {
        BuildProfileDefinition profile;
        profile.id = value.value("id", "");
        const auto kind = value.value("kind", "");
        if (!IsIdentifier(profile.id) || !ids.insert(profile.id).second)
            Add(result.diagnostics, "BuildProfiles.ProfileIdInvalid",
                "Profile IDs must be unique bounded identifiers.");
        if (kind == "Diagnostics") profile.kind = BuildProfileKind::Diagnostics;
        else if (kind == "Benchmark") profile.kind = BuildProfileKind::Benchmark;
        else
        {
            Add(result.diagnostics, "BuildProfiles.KindInvalid",
                "Profile kind must be Diagnostics or Benchmark.");
            continue;
        }
        if (value.contains("requestedCounters") && value["requestedCounters"].is_array())
        {
            std::set<std::string> counters;
            for (const auto& counter : value["requestedCounters"])
                if (counter.is_string() && IsIdentifier(counter.get<std::string>()) &&
                    (counter == "update-time" || counter == "lifecycle-transitions" ||
                     counter == "allocations" || counter == "memory"))
                    counters.insert(counter.get<std::string>());
                else Add(result.diagnostics, "BuildProfiles.CounterInvalid",
                    "Requested counters must identify a supported measurement counter.");
            profile.requestedCounters.assign(counters.begin(), counters.end());
        }
        else Add(result.diagnostics, "BuildProfiles.CountersMissing",
            "Every profile must declare requestedCounters.");

        if (profile.kind == BuildProfileKind::Benchmark)
        {
            profile.scenarioId = value.value("scenarioId", "");
            profile.warmup = value.value("warmup", -1);
            profile.trials = value.value("trials", 0);
            profile.measuredIterations = value.value("measuredIterations", 0);
            profile.fixedStepSeconds = value.value("fixedStepSeconds", 0.0);
            profile.seed = value.value("seed", std::uint64_t{0});
            profile.workload = value.value("workload", 0);
            profile.runtimeMode = value.value("runtimeMode", "");
            profile.evidencePath = value.value("evidencePath", "");
            if (!IsIdentifier(profile.scenarioId) ||
                !benchmarkScenarios.insert(profile.scenarioId).second)
                Add(result.diagnostics, "BuildProfiles.ScenarioAmbiguous",
                    "Benchmark scenario IDs must be valid and unique.");
            if (profile.warmup < 0 || profile.warmup > MaxTrials ||
                profile.trials < 1 || profile.trials > MaxTrials ||
                profile.measuredIterations < 1 || profile.measuredIterations > MaxIterations ||
                profile.workload < 1 || profile.workload > MaxWorkload ||
                !std::isfinite(profile.fixedStepSeconds) || profile.fixedStepSeconds <= 0.0 ||
                profile.fixedStepSeconds > 1.0)
                Add(result.diagnostics, "BuildProfiles.RangeInvalid",
                    "Benchmark warmup, trials, iterations, workload, and fixed step are out of range.");
            if (profile.runtimeMode != "ManagedRuntime")
                Add(result.diagnostics, "BuildProfiles.RuntimeModeInvalid",
                    "Benchmark runtimeMode must be ManagedRuntime.");
            (void)SafeEvidencePath(projectRoot, profile.evidencePath, result.diagnostics);
        }
        result.document.profiles.push_back(std::move(profile));
        }
        catch (const Json::exception& exception)
        {
            Add(result.diagnostics, "BuildProfiles.ProfileInvalid", exception.what());
        }
    }
    if (!result.diagnostics.empty()) return result;
    result.document.canonicalJson = root.dump();
    result.document.contentHash = Hash(result.document.canonicalJson);
    result.succeeded = true;
    return result;
}

std::pair<VerifiedBenchmarkRunInput, std::vector<BuildProfileDiagnostic>>
BuildProfiles::CreateRunInput(const BuildProfilesDocument& document, std::string_view profileId)
{
    VerifiedBenchmarkRunInput output;
    std::vector<BuildProfileDiagnostic> diagnostics;
    const auto iterator = std::find_if(document.profiles.begin(), document.profiles.end(),
        [profileId](const auto& profile) { return profile.id == profileId; });
    if (iterator == document.profiles.end() || iterator->kind != BuildProfileKind::Benchmark)
    {
        Add(diagnostics, "BuildProfiles.BenchmarkMissing",
            "The selected profile is not a validated benchmark profile.");
        return {output, diagnostics};
    }
    output.profileId = iterator->id;
    output.profileHash = document.contentHash;
    output.scenarioId = iterator->scenarioId;
    output.warmup = iterator->warmup;
    output.trials = iterator->trials;
    output.measuredIterations = iterator->measuredIterations;
    output.fixedStepSeconds = iterator->fixedStepSeconds;
    output.seed = iterator->seed;
    output.workload = iterator->workload;
    output.requestedCounters = iterator->requestedCounters;
    output.runtimeMode = iterator->runtimeMode;
    output.evidencePath = iterator->evidencePath;
    const Json value{{"schemaVersion", 1}, {"profileId", output.profileId},
        {"profileHash", output.profileHash}, {"scenarioId", output.scenarioId},
        {"warmup", output.warmup}, {"trials", output.trials},
        {"measuredIterations", output.measuredIterations},
        {"fixedStepSeconds", output.fixedStepSeconds}, {"seed", output.seed},
        {"workload", output.workload}, {"requestedCounters", output.requestedCounters},
        {"runtimeMode", output.runtimeMode},
        {"evidencePath", output.evidencePath.generic_string()}};
    output.canonicalJson = value.dump();
    return {output, diagnostics};
}

BenchmarkStatistics BuildProfiles::CalculateStatistics(const std::vector<double>& raw)
{
    BenchmarkStatistics statistics;
    if (raw.empty() || std::any_of(raw.begin(), raw.end(), [](double value)
        { return !std::isfinite(value) || value < 0.0; })) return statistics;
    auto sorted = raw;
    std::sort(sorted.begin(), sorted.end());
    const auto middle = sorted.size() / 2;
    statistics.medianNanoseconds = sorted.size() % 2 == 0
        ? (sorted[middle - 1] + sorted[middle]) / 2.0 : sorted[middle];
    statistics.p95Nanoseconds = Percentile(sorted, 0.95);
    statistics.p99Nanoseconds = Percentile(sorted, 0.99);
    const auto mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) /
        static_cast<double>(sorted.size());
    statistics.varianceNanosecondsSquared = std::accumulate(sorted.begin(), sorted.end(), 0.0,
        [mean](double total, double value) { return total + (value - mean) * (value - mean); }) /
        static_cast<double>(sorted.size());
    return statistics;
}
}
