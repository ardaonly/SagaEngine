/// @file RuntimeEvidenceRunner.cpp
/// @brief Product Shell subprocess runner for first-playable runtime evidence.

#include "FirstPlayable/RuntimeEvidenceRunner.h"
#include "Processes/SagaProcessService.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <system_error>
#include <utility>

namespace SagaProduct
{
namespace
{

void WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

[[nodiscard]] bool IsWithin(const std::filesystem::path& child,
                            const std::filesystem::path& parent)
{
    std::error_code ec;
    const auto normalizedChild = std::filesystem::weakly_canonical(child, ec);
    if (ec) return false;
    const auto normalizedParent = std::filesystem::weakly_canonical(parent, ec);
    if (ec) return false;
    auto childIt = normalizedChild.begin();
    for (auto parentIt = normalizedParent.begin(); parentIt != normalizedParent.end();
         ++parentIt, ++childIt)
    {
        if (childIt == normalizedChild.end() || *childIt != *parentIt)
            return false;
    }
    return true;
}

void AddDiagnostic(std::vector<FirstPlayableDiagnostic>& diagnostics,
                   const std::string& code,
                   std::string message,
                   const std::filesystem::path& path = {},
                   const std::string& profile = {})
{
    FirstPlayableDiagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.message = std::move(message);
    if (!path.empty()) diagnostic.path = path;
    if (!profile.empty()) diagnostic.profileId = profile;
    diagnostics.push_back(std::move(diagnostic));
}

[[nodiscard]] bool IsPrivatePath(const std::filesystem::path& path)
{
    for (const auto& component : path)
        if (component == ".saga-private") return true;
    return false;
}

[[nodiscard]] std::filesystem::path FindRepositoryRoot(
    std::filesystem::path path)
{
    for (path = std::filesystem::absolute(path); !path.empty();
         path = path.parent_path())
    {
        if (std::filesystem::exists(path / ".git")) return path;
        if (path == path.root_path()) break;
    }
    return {};
}

[[nodiscard]] std::vector<RuntimeEvidenceProfile> Profiles()
{
    return {
        RuntimeEvidenceProfile::StarterArenaSmoke,
        RuntimeEvidenceProfile::StarterArenaLifecycle,
        RuntimeEvidenceProfile::StarterArenaGameplay,
        RuntimeEvidenceProfile::StarterArenaVisibleSynthetic,
        RuntimeEvidenceProfile::StarterArenaVisibleGameplay,
    };
}

} // namespace

const char* ToString(RuntimeEvidenceProfile profile) noexcept
{
    switch (profile)
    {
        case RuntimeEvidenceProfile::StarterArenaSmoke:
            return "starter-arena-smoke";
        case RuntimeEvidenceProfile::StarterArenaLifecycle:
            return "starter-arena-lifecycle";
        case RuntimeEvidenceProfile::StarterArenaGameplay:
            return "starter-arena-gameplay";
        case RuntimeEvidenceProfile::StarterArenaVisibleSynthetic:
            return "starter-arena-visible-synthetic";
        case RuntimeEvidenceProfile::StarterArenaVisibleGameplay:
            return "starter-arena-visible-gameplay";
    }
    return "unknown";
}

EvidenceProcessResult QtEvidenceProcessRunner::Run(
    const EvidenceProcessRequest& request)
{
    SagaProductProcessRequest processRequest;
    processRequest.target = request.executable.filename() == "sagascript" ?
        SagaProcessTargetId::SagaScript : SagaProcessTargetId::Runtime;
    processRequest.executable = request.executable;
    processRequest.arguments = request.arguments;
    processRequest.workingDirectory = request.workingDirectory;
    processRequest.environment = request.environment;
    processRequest.timeout = request.timeout;
    processRequest.stopToken = request.stopToken;

    SagaProcessService service;
    const SagaProductProcessResult process = service.Run(processRequest);

    EvidenceProcessResult result;
    result.started = process.started;
    result.timedOut = process.timedOut;
    result.cancelled = process.cancelled;
    result.exitCode = process.exitCode;
    result.duration = process.duration;
    result.standardOutput = process.standardOutput;
    result.standardError = process.standardError;
    result.startError = process.error;
    return result;
}

RuntimeEvidenceRunner::RuntimeEvidenceRunner()
    : RuntimeEvidenceRunner(std::make_unique<QtEvidenceProcessRunner>())
{}

RuntimeEvidenceRunner::RuntimeEvidenceRunner(
    std::unique_ptr<IEvidenceProcessRunner> runner)
    : m_runner(runner ? std::move(runner)
                      : std::make_unique<QtEvidenceProcessRunner>())
{}

std::vector<std::string> RuntimeEvidenceRunner::BuildProfileArguments(
    RuntimeEvidenceProfile profile,
    const RuntimeEvidenceRunResult& prepared,
    const std::filesystem::path& reportPath)
{
    std::vector<std::string> arguments = {
        "--project", prepared.executionProjectManifest.string(),
        "--fixed-dt", "0.016",
    };
    const bool visible = profile == RuntimeEvidenceProfile::StarterArenaVisibleSynthetic ||
        profile == RuntimeEvidenceProfile::StarterArenaVisibleGameplay;
    const bool scripts = profile == RuntimeEvidenceProfile::StarterArenaLifecycle ||
        profile == RuntimeEvidenceProfile::StarterArenaGameplay ||
        profile == RuntimeEvidenceProfile::StarterArenaVisibleGameplay;
    if (visible)
    {
        arguments.insert(arguments.end(), {
            "--starter-arena-playable", "--playable-frames", "30",
            "--playable-input-source", "synthetic",
            "--playable-input-script",
            (prepared.executionProjectManifest.parent_path() / "Input" /
             "playable.synthetic-input.json").string(),
            "--playable-report-out", reportPath.string(),
        });
    }
    else
    {
        arguments.insert(arguments.end(), {
            "--headless", "--starter-arena-smoke", "--smoke-frames", "30",
            "--smoke-report-out", reportPath.string(),
        });
    }
    if (scripts)
    {
        arguments.insert(arguments.end(), {
            "--script-manifest", prepared.scriptManifest.string(),
            "--script-artifacts", prepared.scriptArtifacts.string(),
            "--run-starter-arena-script-lifecycle",
        });
    }
    if (profile == RuntimeEvidenceProfile::StarterArenaLifecycle)
        arguments.push_back("--invoke-starter-arena-script");
    if (profile == RuntimeEvidenceProfile::StarterArenaGameplay ||
        profile == RuntimeEvidenceProfile::StarterArenaVisibleGameplay)
        arguments.push_back("--run-starter-arena-gameplay");
    return arguments;
}

RuntimeEvidenceRunResult RuntimeEvidenceRunner::Run(
    const RuntimeEvidenceRunRequest& request)
{
    RuntimeEvidenceRunResult result;
    result.projectManifest = std::filesystem::absolute(request.projectManifest);
    result.outputDirectory = std::filesystem::absolute(request.outputDirectory);
    const auto projectRoot = result.projectManifest.parent_path();
    const auto repositoryRoot = FindRepositoryRoot(projectRoot);

    if (!std::filesystem::is_regular_file(result.projectManifest))
        AddDiagnostic(result.diagnostics,
            "ProductShell.FirstPlayable.ProjectMissing",
            "StarterArena project manifest is missing", result.projectManifest);
    if (!std::filesystem::is_regular_file(request.runtimeExecutable))
        AddDiagnostic(result.diagnostics,
            "ProductShell.FirstPlayable.RuntimeExecutableMissing",
            "SagaRuntime executable is missing", request.runtimeExecutable);
    if (request.sagaScriptExecutable.has_parent_path() &&
        !std::filesystem::is_regular_file(request.sagaScriptExecutable))
        AddDiagnostic(result.diagnostics,
            "ProductShell.FirstPlayable.ScriptToolMissing",
            "SagaScript executable is missing", request.sagaScriptExecutable);
    if (!std::filesystem::is_regular_file(request.runtimeBridgeAssembly))
        AddDiagnostic(result.diagnostics,
            "ProductShell.FirstPlayable.RuntimeBridgeMissing",
            "SagaScript runtime bridge assembly is missing",
            request.runtimeBridgeAssembly);
    if (IsWithin(result.outputDirectory, projectRoot) ||
        (!repositoryRoot.empty() &&
         IsWithin(result.outputDirectory, repositoryRoot)) ||
        IsPrivatePath(result.outputDirectory))
        AddDiagnostic(result.diagnostics,
            "ProductShell.FirstPlayable.GeneratedOutputInsideSourceTree",
            "generated evidence output must be outside the project and .saga-private",
            result.outputDirectory);
    if (!result.diagnostics.empty()) return result;

    try
    {
        std::ifstream projectInput(result.projectManifest);
        nlohmann::json project;
        projectInput >> project;
        if (project.value("projectId", "") != "starter-arena")
        {
            AddDiagnostic(result.diagnostics,
                "ProductShell.FirstPlayable.UnsupportedProject",
                "first-playable workflow supports projectId starter-arena",
                result.projectManifest);
            return result;
        }
    }
    catch (const std::exception& error)
    {
        AddDiagnostic(result.diagnostics,
            "ProductShell.FirstPlayable.UnsupportedProject",
            std::string("project manifest is invalid: ") + error.what(),
            result.projectManifest);
        return result;
    }

    std::error_code ec;
    const auto workspaceRoot = result.outputDirectory / "workspace";
    std::filesystem::create_directories(workspaceRoot / "Scripts", ec);
    if (!ec) std::filesystem::create_directories(workspaceRoot / "Scenes", ec);
    if (!ec) std::filesystem::create_directories(workspaceRoot / "Input", ec);
    if (!ec) std::filesystem::create_directories(workspaceRoot / "Diagnostics", ec);
    if (!ec) std::filesystem::create_directories(workspaceRoot / "Build" /
        "Reports", ec);
    if (!ec) std::filesystem::create_directories(workspaceRoot / "Build" /
        "Manifests", ec);
    if (!ec) std::filesystem::create_directories(workspaceRoot / "Build" /
        "Artifacts" / "Scripts", ec);
    if (!ec) std::filesystem::create_directories(result.outputDirectory /
        "script", ec);
    if (ec)
    {
        AddDiagnostic(result.diagnostics,
            "ProductShell.FirstPlayable.ReportDirectoryInvalid",
            "could not create generated report directory: " + ec.message(),
            result.outputDirectory);
        return result;
    }

    const auto copyInput = [&](const std::filesystem::path& relative)
    {
        std::filesystem::copy_file(projectRoot / relative,
            workspaceRoot / relative,
            std::filesystem::copy_options::overwrite_existing, ec);
    };
    copyInput(result.projectManifest.filename());
    if (!ec) copyInput(std::filesystem::path("Scripts") / "GameRules.cs");
    if (!ec) copyInput(std::filesystem::path("Scenes") / "arena.scene.json");
    if (!ec) copyInput(std::filesystem::path("Input") /
                       "playable.synthetic-input.json");
    if (ec)
    {
        AddDiagnostic(result.diagnostics,
            "ProductShell.FirstPlayable.ProjectMissing",
            "could not create generated StarterArena workspace: " + ec.message(),
            workspaceRoot);
        return result;
    }
    result.executionProjectManifest = workspaceRoot /
        result.projectManifest.filename();

    result.scriptManifest = workspaceRoot / "Build" / "Manifests" /
        "script_bindings.json";
    result.scriptArtifacts = workspaceRoot / "Build" / "Manifests" /
        "script_artifacts.json";
    const auto compilerDiagnostics = result.outputDirectory / "script" /
        "sagascript_diagnostics.json";
    EvidenceProcessRequest compile;
    compile.executable = request.sagaScriptExecutable.has_parent_path() ?
        std::filesystem::absolute(request.sagaScriptExecutable) :
        request.sagaScriptExecutable;
    compile.workingDirectory = workspaceRoot;
    compile.timeout = request.timeout;
    compile.stopToken = request.stopToken;
    compile.environment["SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY"] =
        std::filesystem::absolute(request.runtimeBridgeAssembly).string();
    compile.arguments = {
        "compile", "--source", (workspaceRoot / "Scripts").string(),
        "--out", (workspaceRoot / "Build" / "Manifests").string(),
        "--artifacts-out", (workspaceRoot / "Build" / "Artifacts" /
            "Scripts").string(),
        "--project-root", workspaceRoot.string(),
        "--assembly-name", "StarterArenaScripts",
        "--diagnostics", compilerDiagnostics.string(), "--json",
    };
    const EvidenceProcessResult compilation = m_runner->Run(compile);
    WriteText(result.outputDirectory / "script" / "sagascript.stdout.txt",
              compilation.standardOutput);
    WriteText(result.outputDirectory / "script" / "sagascript.stderr.txt",
              compilation.standardError);
    if (!compilation.started || compilation.timedOut || compilation.cancelled || compilation.exitCode != 0 ||
        !std::filesystem::exists(result.scriptManifest) ||
        !std::filesystem::exists(result.scriptArtifacts))
    {
        AddDiagnostic(result.diagnostics,
            compilation.cancelled ? "ProductShell.FirstPlayable.ProcessCancelled" :
            compilation.timedOut ? "ProductShell.FirstPlayable.ProcessTimedOut" :
            "ProductShell.FirstPlayable.ProcessExitFailed",
            "SagaScript evidence preparation failed", compilerDiagnostics);
        return result;
    }
    result.prepared = true;

    for (RuntimeEvidenceProfile profile : Profiles())
    {
        RuntimeEvidenceProfileResult profileResult;
        profileResult.profile = profile;
        const std::string profileId = ToString(profile);
        const auto directory = result.outputDirectory / "profiles" / profileId;
        std::filesystem::create_directories(directory, ec);
        profileResult.reportPath = directory / "report.json";
        profileResult.stdoutPath = directory / "stdout.txt";
        profileResult.stderrPath = directory / "stderr.txt";
        EvidenceProcessRequest process;
        process.executable = std::filesystem::absolute(request.runtimeExecutable);
        process.workingDirectory = workspaceRoot;
        process.timeout = request.timeout;
        process.stopToken = request.stopToken;
        process.environment["SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY"] =
            std::filesystem::absolute(request.runtimeBridgeAssembly).string();
        process.arguments = BuildProfileArguments(profile, result,
                                                   profileResult.reportPath);
        profileResult.process = m_runner->Run(process);
        WriteText(profileResult.stdoutPath, profileResult.process.standardOutput);
        WriteText(profileResult.stderrPath, profileResult.process.standardError);
        if (!profileResult.process.started)
            AddDiagnostic(profileResult.diagnostics,
                "ProductShell.FirstPlayable.ProcessLaunchFailed",
                "SagaRuntime failed to start", process.executable, profileId);
        else if (profileResult.process.cancelled)
            AddDiagnostic(profileResult.diagnostics,
                "ProductShell.FirstPlayable.ProcessCancelled",
                "SagaRuntime profile was cancelled", profileResult.stderrPath, profileId);
        else if (profileResult.process.timedOut)
            AddDiagnostic(profileResult.diagnostics,
                "ProductShell.FirstPlayable.ProcessTimedOut",
                "SagaRuntime profile timed out", profileResult.stderrPath, profileId);
        else if (profileResult.process.exitCode != 0)
            AddDiagnostic(profileResult.diagnostics,
                "ProductShell.FirstPlayable.ProcessExitFailed",
                "SagaRuntime exited with code " +
                    std::to_string(profileResult.process.exitCode),
                profileResult.stderrPath, profileId);

        profileResult.report = ParseRuntimeEvidenceReport(
            profileResult.reportPath, profileId);
        profileResult.diagnostics.insert(profileResult.diagnostics.end(),
            profileResult.report.diagnostics.begin(),
            profileResult.report.diagnostics.end());
        profileResult.status = profileResult.diagnostics.empty() ?
            EvidenceStatus::Passed : EvidenceStatus::Failed;
        result.profiles.push_back(std::move(profileResult));
        if (request.stopToken.stop_requested()) break;
    }
    return result;
}

} // namespace SagaProduct
