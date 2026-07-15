/// @file SagaScriptGate.cpp
/// @brief Product-owned SagaScript validation gate orchestration.

#include "ProductIntegration/SagaScriptGate.h"
#include "Processes/SagaProcessService.h"

#include <ostream>
#include <utility>

namespace SagaProduct
{
namespace
{

constexpr const char* kProjectManifestFile = "saga.project.json";

[[nodiscard]] SagaProductDiagnostic MakeSagaScriptDiagnostic(
    const char* diagnosticId,
    std::string message,
    std::filesystem::path path = {})
{
    SagaProductDiagnostic diagnostic;
    diagnostic.stage = SagaProductDiagnosticStage::ProjectValidation;
    diagnostic.diagnosticId = diagnosticId;
    diagnostic.message = std::move(message);
    if (!path.empty())
    {
        diagnostic.path = std::move(path);
    }
    return diagnostic;
}

[[nodiscard]] std::filesystem::path AbsoluteProjectRoot(
    const std::filesystem::path& projectRoot)
{
    if (projectRoot.empty())
    {
        return {};
    }
    return std::filesystem::absolute(projectRoot);
}

} // namespace

SagaToolProcessResult SagaToolProcessRunner::Run(
    const SagaToolProcessRequest& request)
{
    SagaToolProcessResult result;

    SagaProductProcessRequest processRequest;
    processRequest.target = SagaProcessTargetId::Forge;
    processRequest.executable = request.executablePath;
    processRequest.arguments = request.arguments;
    processRequest.workingDirectory = request.workingDirectory;

    SagaProcessService service;
    const SagaProductProcessResult process = service.Run(processRequest);
    result.started = process.started;
    result.exitCode = process.exitCode;
    result.standardOutput = process.standardOutput;
    result.standardError = process.standardError;
    result.startError = process.error;
    return result;
}

SagaScriptGate::SagaScriptGate()
    : SagaScriptGate(std::make_unique<SagaToolProcessRunner>())
{}

SagaScriptGate::SagaScriptGate(std::unique_ptr<ISagaToolProcessRunner> runner)
    : m_runner(runner ? std::move(runner)
                      : std::make_unique<SagaToolProcessRunner>())
{}

SagaScriptGatePaths SagaScriptGate::PathsForProject(
    const std::filesystem::path& projectRoot)
{
    const std::filesystem::path root = AbsoluteProjectRoot(projectRoot);
    SagaScriptGatePaths paths;
    paths.sourceRoot = root / "Scripts";
    paths.manifestOutputDirectory = root / "Build" / "Manifests";
    paths.artifactOutputDirectory = root / "Build" / "Artifacts" / "Scripts";
    paths.diagnosticsOutputPath =
        root / "Build" / "Reports" / "sagascript_diagnostics.json";
    return paths;
}

SagaToolProcessRequest SagaScriptGate::BuildProcessRequest(
    const SagaScriptGateRequest& request)
{
    const std::filesystem::path root = AbsoluteProjectRoot(request.projectRoot);
    const SagaScriptGatePaths paths = PathsForProject(root);

    SagaToolProcessRequest process;
    process.executablePath = request.forgeExecutable;
    process.workingDirectory = root;
    process.arguments = {
        "gate",
        "run",
        "--name",
        "sagascript",
        "--tool",
        request.sagaScriptExecutable.string(),
        "--diagnostics",
        paths.diagnosticsOutputPath.string(),
        "--",
        "compile",
        "--source",
        paths.sourceRoot.string(),
        "--out",
        paths.manifestOutputDirectory.string(),
        "--artifacts-out",
        paths.artifactOutputDirectory.string(),
        "--project-root",
        root.string(),
        "--diagnostics",
        paths.diagnosticsOutputPath.string(),
    };
    return process;
}

SagaScriptGateResult SagaScriptGate::ValidateProject(
    const SagaScriptGateRequest& request,
    std::ostream& out,
    std::ostream& err)
{
    SagaScriptGateResult result;
    const std::filesystem::path root = AbsoluteProjectRoot(request.projectRoot);
    result.paths = PathsForProject(root);

    if (root.empty())
    {
        result.diagnostics.push_back(MakeSagaScriptDiagnostic(
            SagaProductDiagnostics::SagaScriptProjectMissing,
            "SagaScript validation requires a project root."));
        return result;
    }

    const std::filesystem::path manifestPath = root / kProjectManifestFile;
    if (!std::filesystem::exists(manifestPath))
    {
        result.diagnostics.push_back(MakeSagaScriptDiagnostic(
            SagaProductDiagnostics::SagaScriptManifestMissing,
            "SagaScript validation requires saga.project.json: " +
                manifestPath.string(),
            manifestPath));
        return result;
    }

    if (!std::filesystem::exists(result.paths.sourceRoot) ||
        !std::filesystem::is_directory(result.paths.sourceRoot))
    {
        result.diagnostics.push_back(MakeSagaScriptDiagnostic(
            SagaProductDiagnostics::SagaScriptSourceMissing,
            "SagaScript source root is missing: " +
                result.paths.sourceRoot.string(),
            result.paths.sourceRoot));
        return result;
    }

    std::error_code ec;
    std::filesystem::create_directories(result.paths.manifestOutputDirectory, ec);
    if (!ec)
    {
        std::filesystem::create_directories(
            result.paths.artifactOutputDirectory, ec);
    }
    if (!ec)
    {
        std::filesystem::create_directories(
            result.paths.diagnosticsOutputPath.parent_path(), ec);
    }
    if (ec)
    {
        result.diagnostics.push_back(MakeSagaScriptDiagnostic(
            SagaProductDiagnostics::SagaScriptGateFailed,
            "SagaScript validation could not create Build output folders: " +
                ec.message(),
            root / "Build"));
        return result;
    }

    SagaScriptGateRequest normalizedRequest = request;
    normalizedRequest.projectRoot = root;
    result.processRequest = BuildProcessRequest(normalizedRequest);

    const SagaToolProcessResult processResult =
        m_runner->Run(result.processRequest);
    result.started = processResult.started;
    result.exitCode = processResult.exitCode;
    out << processResult.standardOutput;
    err << processResult.standardError;

    if (!processResult.started)
    {
        result.diagnostics.push_back(MakeSagaScriptDiagnostic(
            SagaProductDiagnostics::SagaScriptGateStartFailed,
            "SagaScript validation gate failed to start: " +
                processResult.startError,
            result.processRequest.executablePath));
        return result;
    }

    if (processResult.exitCode != 0)
    {
        result.diagnostics.push_back(MakeSagaScriptDiagnostic(
            SagaProductDiagnostics::SagaScriptGateFailed,
            "SagaScript validation failed with exit code " +
                std::to_string(processResult.exitCode),
            result.paths.diagnosticsOutputPath));
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace SagaProduct
