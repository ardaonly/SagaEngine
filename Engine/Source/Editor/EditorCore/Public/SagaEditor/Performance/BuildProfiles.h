// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SagaEditor
{
enum class BuildProfileKind
{
    Diagnostics,
    Benchmark,
};

struct BuildProfileDiagnostic final
{
    std::string code;
    std::string message;
};

struct BuildProfileDefinition final
{
    std::string id;
    BuildProfileKind kind = BuildProfileKind::Diagnostics;
    std::string scenarioId;
    int warmup = 0;
    int trials = 0;
    int measuredIterations = 0;
    double fixedStepSeconds = 0.0;
    std::uint64_t seed = 0;
    int workload = 0;
    std::vector<std::string> requestedCounters;
    std::string runtimeMode;
    std::filesystem::path evidencePath;
};

struct BuildProfilesDocument final
{
    static constexpr int CurrentSchemaVersion = 1;
    int schemaVersion = CurrentSchemaVersion;
    std::vector<BuildProfileDefinition> profiles;
    std::string canonicalJson;
    std::string contentHash;
};

struct BuildProfilesLoadResult final
{
    bool succeeded = false;
    BuildProfilesDocument document;
    std::vector<BuildProfileDiagnostic> diagnostics;
};

struct VerifiedBenchmarkRunInput final
{
    int schemaVersion = 1;
    std::string profileId;
    std::string profileHash;
    std::string scenarioId;
    int warmup = 0;
    int trials = 0;
    int measuredIterations = 0;
    double fixedStepSeconds = 0.0;
    std::uint64_t seed = 0;
    int workload = 0;
    std::vector<std::string> requestedCounters;
    std::string runtimeMode;
    std::filesystem::path evidencePath;
    std::string canonicalJson;
};

struct BenchmarkStatistics final
{
    double medianNanoseconds = 0.0;
    double p95Nanoseconds = 0.0;
    double p99Nanoseconds = 0.0;
    double varianceNanosecondsSquared = 0.0;
};

class BuildProfiles final
{
public:
    [[nodiscard]] static BuildProfilesLoadResult Load(
        const std::filesystem::path& profilePath,
        const std::filesystem::path& projectRoot);
    [[nodiscard]] static std::pair<VerifiedBenchmarkRunInput,
        std::vector<BuildProfileDiagnostic>> CreateRunInput(
        const BuildProfilesDocument& document, std::string_view profileId);
    [[nodiscard]] static BenchmarkStatistics CalculateStatistics(
        const std::vector<double>& rawNanoseconds);
};
}
