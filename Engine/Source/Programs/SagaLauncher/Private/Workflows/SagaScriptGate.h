/// @file SagaScriptGate.h
/// @brief Product-owned SagaScript validation gate orchestration.

#pragma once

#include "Projects/SagaSessionModel.h"

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace SagaProduct
{

/// Standard SagaScript paths for a project validation run.
struct SagaScriptGatePaths
{
    std::filesystem::path sourceRoot;
    std::filesystem::path manifestOutputDirectory;
    std::filesystem::path artifactOutputDirectory;
    std::filesystem::path diagnosticsOutputPath;
};

/// Product request for validating SagaScript source through Forge's generic gate.
struct SagaScriptGateRequest
{
    std::filesystem::path projectManifest;
    std::filesystem::path forgeExecutable = "forge";
    std::filesystem::path sagaScriptExecutable = "sagascript";
};

/// Generic process request used by product-owned tool orchestration.
struct SagaToolProcessRequest
{
    std::filesystem::path executablePath;
    std::vector<std::string> arguments;
    std::filesystem::path workingDirectory;
};

/// Generic process result used by product-owned tool orchestration.
struct SagaToolProcessResult
{
    bool started = false;
    int exitCode = -1;
    std::string standardOutput;
    std::string standardError;
    std::string startError;
};

/// Process boundary for invoking product workflow tools.
class ISagaToolProcessRunner
{
public:
    virtual ~ISagaToolProcessRunner() = default;

    [[nodiscard]] virtual SagaToolProcessResult Run(
        const SagaToolProcessRequest& request) = 0;
};

/// Qt-backed implementation for product workflow tool processes.
class SagaToolProcessRunner final : public ISagaToolProcessRunner
{
public:
    [[nodiscard]] SagaToolProcessResult Run(
        const SagaToolProcessRequest& request) override;
};

/// Result of a SagaScript validation gate run.
struct SagaScriptGateResult
{
    bool ok = false;
    bool started = false;
    int exitCode = -1;
    SagaScriptGatePaths paths;
    SagaToolProcessRequest processRequest;
    std::vector<SagaProductDiagnostic> diagnostics;
};

/// Builds and runs the product-side SagaScript validation workflow.
class SagaScriptGate final
{
public:
    SagaScriptGate();
    explicit SagaScriptGate(std::unique_ptr<ISagaToolProcessRunner> runner);

    [[nodiscard]] SagaScriptGateResult ValidateProject(
        const SagaScriptGateRequest& request,
        std::ostream& out,
        std::ostream& err);

    [[nodiscard]] static SagaScriptGatePaths PathsForManifest(
        const std::filesystem::path& projectManifest);

    [[nodiscard]] static SagaToolProcessRequest BuildProcessRequest(
        const SagaScriptGateRequest& request);

private:
    std::unique_ptr<ISagaToolProcessRunner> m_runner;
};

} // namespace SagaProduct
