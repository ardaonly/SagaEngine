// SPDX-License-Identifier: MPL-2.0
/// @file SagaScriptGate.cpp
/// @brief Product-owned SagaScript validation gate orchestration.

#include "Workflows/SagaScriptGate.h"
#include "Projects/SagaProjectCatalog.h"

#include <ostream>
#include <utility>

namespace SagaProduct
{
namespace
{

[[nodiscard]] SagaProductDiagnostic MakeSagaScriptDiagnostic(
    std::string diagnosticId,
    std::string message,
    std::filesystem::path path = {})
{
    SagaProductDiagnostic diagnostic;
    diagnostic.stage = SagaProductDiagnosticStage::ProjectValidation;
    diagnostic.diagnosticId = std::move(diagnosticId);
    diagnostic.message = std::move(message);
    if (!path.empty())
    {
        diagnostic.path = std::move(path);
    }
    return diagnostic;
}

[[nodiscard]] std::filesystem::path CanonicalProjectRoot(
    const std::filesystem::path& projectManifest)
{
    if (projectManifest.empty() || projectManifest.extension() != ".sagaproj")
    {
        return {};
    }
    std::error_code error;
    const auto manifest = std::filesystem::weakly_canonical(projectManifest, error);
    return error ? std::filesystem::path{} : manifest.parent_path();
}

} // namespace

SagaToolProcessResult SagaToolProcessRunner::Run(
    const SagaToolProcessRequest& request)
{
    SagaToolProcessResult result;
    SagaEditor::ToolProcessRequest processRequest;
    processRequest.executable = request.executablePath;
    processRequest.arguments = request.arguments;
    processRequest.workingDirectory = request.workingDirectory;
    processRequest.environment = request.environment;
    const SagaEditor::ToolProcessResult process =
        SagaEditor::ToolProcessRunner{}.Run(processRequest);
    result.started = process.classification !=
        SagaEditor::ToolProcessExitClassification::NotStarted;
    result.exitCode = process.exitCode;
    result.standardOutput = process.standardOutput;
    result.standardError = process.standardError;
    result.startError = process.diagnosticMessage;
    return result;
}

SagaScriptGate::SagaScriptGate()
    : SagaScriptGate(std::make_unique<SagaToolProcessRunner>())
{}

SagaScriptGate::SagaScriptGate(std::unique_ptr<ISagaToolProcessRunner> runner)
    : m_runner(runner ? std::move(runner)
                      : std::make_unique<SagaToolProcessRunner>())
{}

SagaScriptGatePaths SagaScriptGate::PathsForManifest(
    const std::filesystem::path& projectManifest)
{
    const std::filesystem::path root = CanonicalProjectRoot(projectManifest);
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
    const std::filesystem::path root = CanonicalProjectRoot(request.projectManifest);
    const SagaScriptGatePaths paths = PathsForManifest(request.projectManifest);

    SagaEditor::ToolProcessRequest managedOptions;
    managedOptions.workingDirectory = root;
    if (!request.runtimeBridge.empty())
        managedOptions.environment["SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY"] =
            request.runtimeBridge.string();
    const auto invocation =
        SagaEditor::ManagedToolRuntime(request.managedRuntime)
            .PrepareInvocation(
                SagaEditor::ManagedToolId::Script,
                {
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
                },
                std::move(managedOptions));

    SagaToolProcessRequest process;
    process.workingDirectory = root;
    if (invocation.process)
    {
        process.executablePath = invocation.process->executable;
        process.arguments = invocation.process->arguments;
        process.environment = invocation.process->environment;
    }
    return process;
}

SagaScriptGateResult SagaScriptGate::ValidateProject(
    const SagaScriptGateRequest& request,
    std::ostream& out,
    std::ostream& err)
{
    SagaScriptGateResult result;
    const std::filesystem::path manifestPath = request.projectManifest;
    const SagaLauncherProjectSummary project =
        SagaProjectCatalog{}.OpenProject(manifestPath);
    const std::filesystem::path root = project.canonicalRoot;
    result.paths = PathsForManifest(project.canonicalManifestPath);

    if (manifestPath.empty() || manifestPath.extension() != ".sagaproj" ||
        !std::filesystem::is_regular_file(manifestPath))
    {
        result.diagnostics.push_back(MakeSagaScriptDiagnostic(
            SagaProductDiagnostics::SagaScriptProjectMissing,
            "SagaScript validation requires a .sagaproj manifest.",
            manifestPath));
        return result;
    }

    if (!project.valid)
    {
        const std::string message = project.diagnostics.empty()
            ? "SagaScript validation rejected the .sagaproj manifest."
            : project.diagnostics.front().message;
        result.diagnostics.push_back(MakeSagaScriptDiagnostic(
            SagaProductDiagnostics::SagaScriptManifestMissing,
            message,
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

    const auto managedInvocation =
        SagaEditor::ManagedToolRuntime(request.managedRuntime)
            .PrepareInvocation(SagaEditor::ManagedToolId::Script, {});
    if (!managedInvocation.Ready())
    {
        const auto& diagnostic = managedInvocation.diagnostics.front();
        result.diagnostics.push_back(MakeSagaScriptDiagnostic(
            diagnostic.code, diagnostic.message, diagnostic.path));
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
    normalizedRequest.projectManifest = project.canonicalManifestPath;
    result.processRequest = BuildProcessRequest(normalizedRequest);
    if (result.processRequest.executablePath.empty())
    {
        result.diagnostics.push_back(MakeSagaScriptDiagnostic(
            SagaProductDiagnostics::SagaScriptGateFailed,
            "SagaScript validation could not prepare the managed tool invocation."));
        return result;
    }

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
            "SagaScript validation failed to start: " +
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
