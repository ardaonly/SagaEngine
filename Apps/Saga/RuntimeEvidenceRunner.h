/// @file RuntimeEvidenceRunner.h
/// @brief Product Shell subprocess runner for first-playable runtime evidence.

#pragma once

#include "RuntimeEvidenceReport.h"

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SagaProduct
{

enum class RuntimeEvidenceProfile
{
    StarterArenaSmoke,
    StarterArenaLifecycle,
    StarterArenaGameplay,
    StarterArenaVisibleSynthetic,
    StarterArenaVisibleGameplay,
};

[[nodiscard]] const char* ToString(RuntimeEvidenceProfile profile) noexcept;

struct EvidenceProcessRequest
{
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path workingDirectory;
    std::map<std::string, std::string> environment;
    std::chrono::milliseconds timeout{30000};
};

struct EvidenceProcessResult
{
    bool started = false;
    bool timedOut = false;
    int exitCode = -1;
    std::chrono::milliseconds duration{0};
    std::string standardOutput;
    std::string standardError;
    std::string startError;
};

class IEvidenceProcessRunner
{
public:
    virtual ~IEvidenceProcessRunner() = default;
    [[nodiscard]] virtual EvidenceProcessResult Run(
        const EvidenceProcessRequest& request) = 0;
};

class QtEvidenceProcessRunner final : public IEvidenceProcessRunner
{
public:
    [[nodiscard]] EvidenceProcessResult Run(
        const EvidenceProcessRequest& request) override;
};

struct RuntimeEvidenceRunRequest
{
    std::filesystem::path projectManifest;
    std::filesystem::path outputDirectory;
    std::filesystem::path runtimeExecutable;
    std::filesystem::path sagaScriptExecutable;
    std::filesystem::path runtimeBridgeAssembly;
    std::chrono::milliseconds timeout{30000};
};

struct RuntimeEvidenceProfileResult
{
    RuntimeEvidenceProfile profile = RuntimeEvidenceProfile::StarterArenaSmoke;
    EvidenceStatus status = EvidenceStatus::Incomplete;
    std::filesystem::path reportPath;
    std::filesystem::path stdoutPath;
    std::filesystem::path stderrPath;
    EvidenceProcessResult process;
    RuntimeEvidenceReport report;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

struct RuntimeEvidenceRunResult
{
    bool prepared = false;
    std::filesystem::path projectManifest;
    std::filesystem::path executionProjectManifest;
    std::filesystem::path outputDirectory;
    std::filesystem::path scriptManifest;
    std::filesystem::path scriptArtifacts;
    std::vector<RuntimeEvidenceProfileResult> profiles;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

class RuntimeEvidenceRunner final
{
public:
    RuntimeEvidenceRunner();
    explicit RuntimeEvidenceRunner(std::unique_ptr<IEvidenceProcessRunner> runner);

    [[nodiscard]] RuntimeEvidenceRunResult Run(
        const RuntimeEvidenceRunRequest& request);

    [[nodiscard]] static std::vector<std::string> BuildProfileArguments(
        RuntimeEvidenceProfile profile,
        const RuntimeEvidenceRunResult& prepared,
        const std::filesystem::path& reportPath);

private:
    std::unique_ptr<IEvidenceProcessRunner> m_runner;
};

} // namespace SagaProduct
