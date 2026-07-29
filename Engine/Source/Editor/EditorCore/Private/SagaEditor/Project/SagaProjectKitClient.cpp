// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Project/SagaProjectKitClient.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string_view>

namespace SagaEditor
{
namespace
{
std::filesystem::path EnvironmentPath(const char* name)
{
    const auto* value = std::getenv(name);
    return value && *value ? std::filesystem::path(value) : std::filesystem::path{};
}

std::filesystem::path FindEngineRoot(std::filesystem::path cursor)
{
    std::error_code error;
    cursor = std::filesystem::absolute(cursor, error);
    if (error) return {};
    if (std::filesystem::is_regular_file(cursor, error)) cursor = cursor.parent_path();
    while (!cursor.empty())
    {
        if (std::filesystem::is_regular_file(cursor / "Samples/StarterArena/StarterArena.sagaproj", error) &&
            std::filesystem::is_directory(cursor / "Tools/SagaPackager", error))
            return cursor;
        const auto parent = cursor.parent_path();
        if (parent == cursor) break;
        cursor = parent;
    }
    return {};
}

std::filesystem::path CurrentExecutable()
{
#ifndef _WIN32
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) return executable;
#endif
    return {};
}

bool IsInstalledProductRoot(const std::filesystem::path& root)
{
    std::error_code error;
    return std::filesystem::is_directory(root / "samples/StarterArena", error) &&
        std::filesystem::is_regular_file(
            root / "managed-tool-runtime.json", error);
}

std::filesystem::path FindInstalledProductRoot(std::filesystem::path executable)
{
    std::error_code error;
    executable = std::filesystem::absolute(executable, error);
    if (error) return {};
    auto cursor = std::filesystem::is_regular_file(executable, error) ?
        executable.parent_path() : executable;
    while (!cursor.empty())
    {
        if (IsInstalledProductRoot(cursor)) return cursor;
        const auto parent = cursor.parent_path();
        if (parent == cursor) break;
        cursor = parent;
    }
    return {};
}

std::filesystem::path TemporaryReport(std::string_view name)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
        ("saga-editor-" + std::string(name) + "-" + std::to_string(stamp) + ".json");
}

ProjectToolResult ParseReport(const std::filesystem::path& path,
                              ToolProcessResult process)
{
    ProjectToolResult result;
    result.process = std::move(process);
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        result.diagnostics.push_back({
            "Project.ToolResultMissing",
            "SagaProjectKit did not publish its structured result.", path});
        return result;
    }
    try
    {
        const auto report = nlohmann::json::parse(input);
        if (!report.is_object() || report.value("schemaVersion", -1) != 1 ||
            report.value("tool", std::string{}) != "sagaproject" ||
            !report.contains("project") || !report["project"].is_object())
        {
            result.diagnostics.push_back({
                "Project.ToolResultMalformed",
                "SagaProjectKit result schema is invalid.", path});
            return result;
        }
        for (const auto& item : report.value("diagnostics", nlohmann::json::array()))
        {
            result.diagnostics.push_back({
                item.value("code", std::string{"Project.ValidationFailed"}),
                item.value("message", std::string{}),
                item.value("path", std::string{})});
        }
        const auto& project = report["project"];
        result.project.projectId = project.value("projectId", std::string{});
        result.project.displayName = project.value("displayName", std::string{});
        result.project.projectRoot = project.value("projectRoot", std::string{});
        result.project.manifestPath = project.value("manifestPath", std::string{});
        result.project.engineTargetVersion = project.value("engineTargetVersion", std::string{});
        result.project.schemaVersion = project.value("schemaVersion", 0U);
        for (const auto& item : report.value("resolvedPaths", nlohmann::json::array()))
        {
            if (!item.is_object() || !item.value("exists", false)) continue;
            result.project.sources.push_back({
                item.value("category", std::string{}),
                item.value("id", std::string{}),
                item.value("kind", std::string{}),
                item.value("relativePath", std::string{}),
                item.value("absolutePath", std::string{})});
        }
        result.passed = result.process.Succeeded() &&
            report.value("status", std::string{}) == "Passed" &&
            !result.project.projectId.empty() &&
            !result.project.displayName.empty() &&
            !result.project.manifestPath.empty();
        if (!result.passed && result.diagnostics.empty())
        {
            result.diagnostics.push_back({
                "Project.ValidationFailed",
                result.process.standardError.empty() ?
                    "SagaProjectKit rejected the project." : result.process.standardError,
                result.project.manifestPath});
        }
    }
    catch (const nlohmann::json::exception& error)
    {
        result.diagnostics.push_back({"Project.ToolResultMalformed", error.what(), path});
    }
    return result;
}

ProjectToolResult ParseTemplateCreateReport(const std::filesystem::path& path,
                                            ToolProcessResult process)
{
    ProjectToolResult result;
    result.process = std::move(process);
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        result.diagnostics.push_back({"Project.ToolResultMissing",
            "SagaProjectKit did not publish its template result.", path});
        return result;
    }
    try
    {
        const auto report = nlohmann::json::parse(input);
        if (!report.is_object() || report.value("schemaVersion", -1) != 1 ||
            report.value("tool", std::string{}) != "sagaproject" ||
            report.value("command", std::string{}) != "template-create")
        {
            result.diagnostics.push_back({"Project.ToolResultMalformed",
                "SagaProjectKit template result schema is invalid.", path});
            return result;
        }
        for (const auto& item : report.value("diagnostics", nlohmann::json::array()))
        {
            result.diagnostics.push_back({
                item.value("code", std::string{"Project.Template.CreateFailed"}),
                item.value("message", std::string{}),
                item.value("path", std::string{})});
        }
        result.passed = result.process.Succeeded() &&
            report.value("status", std::string{}) == "Passed" &&
            !report.value("projectId", std::string{}).empty() &&
            !report.value("projectHash", std::string{}).empty();
        if (!result.passed && result.diagnostics.empty())
        {
            result.diagnostics.push_back({"Project.Template.CreateFailed",
                result.process.standardError.empty() ?
                    "SagaProjectKit rejected project creation." : result.process.standardError,
                report.value("destination", std::string{})});
        }
    }
    catch (const nlohmann::json::exception& error)
    {
        result.diagnostics.push_back({"Project.ToolResultMalformed", error.what(), path});
    }
    return result;
}

ProjectToolResult ProcessFailure(ToolProcessResult process,
                                 std::string code,
                                 const std::filesystem::path& path)
{
    ProjectToolResult result;
    result.process = std::move(process);
    result.diagnostics.push_back({std::move(code),
        result.process.standardError.empty() ?
            "SagaProjectKit process failed." : result.process.standardError,
        path});
    return result;
}

ProjectToolResult MissingResource(std::string code,
                                  std::string message,
                                  const std::filesystem::path& path)
{
    ProjectToolResult result;
    result.diagnostics.push_back(
        {std::move(code), std::move(message), path});
    return result;
}
}

ProjectTemplateRequest MakeStarterArenaTemplateRequest(
    std::filesystem::path destination, std::string displayName,
    const std::filesystem::path& productExecutable)
{
    return MakeProjectTemplateRequest(ProjectTemplateId::StarterArena,
        std::move(destination), std::move(displayName), productExecutable);
}

ProjectTemplateRequest MakeProjectTemplateRequest(
    ProjectTemplateId templateId, std::filesystem::path destination,
    std::string displayName, const std::filesystem::path& productExecutable)
{
    const auto toolchain = ResolveEditorToolchain(productExecutable);
    ProjectTemplateRequest request;
    request.templateId = templateId;
    request.destination = std::move(destination);
    request.displayName = std::move(displayName);
    request.engineRoot = toolchain.engineRoot;
    request.seed = templateId == ProjectTemplateId::ArenaRush ?
        toolchain.arenaRushSeed : toolchain.starterArenaSeed;
    request.runtimeBridge = toolchain.runtimeBridge;
    request.managedRuntime = toolchain.managedRuntime;
    return request;
}

EditorToolchainPaths ResolveEditorToolchain(
    const std::filesystem::path& productExecutable)
{
    EditorToolchainPaths result;
    const auto anchor = productExecutable.empty() ?
        CurrentExecutable() : productExecutable;
    auto developerRoot = FindEngineRoot(anchor);
    if (const auto configured = EnvironmentPath("SAGA_ENGINE_ROOT");
        !configured.empty())
    {
        result.engineRoot = std::filesystem::absolute(configured);
        developerRoot = FindEngineRoot(configured);
    }
    result.productRoot = FindInstalledProductRoot(anchor);
    if (const auto configured = EnvironmentPath("SAGA_PRODUCT_ROOT");
        !configured.empty())
    {
        result.productRoot = std::filesystem::absolute(configured);
        developerRoot.clear();
        result.engineRoot.clear();
    }
    if (result.engineRoot.empty())
        result.engineRoot = developerRoot.empty() ?
            result.productRoot : developerRoot;

    result.starterArenaSeed = !developerRoot.empty() ?
        developerRoot / "Samples/StarterArena" :
        result.productRoot / "samples/StarterArena";
    result.arenaRushSeed = !developerRoot.empty() ?
        developerRoot / "Samples/ArenaRush" :
        result.productRoot / "samples/ArenaRush";
    result.managedRuntime.productExecutable = anchor;
    result.runtimeBridge = EnvironmentPath("SAGA_RUNTIME_BRIDGE");
    if (result.runtimeBridge.empty())
    {
        if (!developerRoot.empty())
        {
            const auto buildBridge = anchor.parent_path().parent_path() /
                "Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll";
            std::error_code error;
            result.runtimeBridge = std::filesystem::is_regular_file(
                buildBridge, error) ? buildBridge :
                developerRoot /
                    "Engine/Managed/SagaScript.RuntimeBridge/bin/Release/net10.0/"
                    "SagaScript.RuntimeBridge.dll";
        }
        else
        {
            result.runtimeBridge = result.productRoot /
                "Engine/Managed/SagaScript.RuntimeBridge/"
                "SagaScript.RuntimeBridge.dll";
        }
    }
    return result;
}

SagaProjectKitClient::SagaProjectKitClient(ManagedToolRuntimeOptions runtime,
                                           std::chrono::milliseconds timeout)
    : runtime_(std::move(runtime)), timeout_(timeout) {}

ProjectToolResult SagaProjectKitClient::RunReportCommand(
    std::vector<std::string> arguments,
    const std::filesystem::path& reportPath) const
{
    ToolProcessRequest options;
    options.timeout = timeout_;
    const auto managed = runtime_.Run(
        ManagedToolId::ProjectKit, std::move(arguments), std::move(options));
    auto result = ParseReport(reportPath, managed.process);
    for (const auto& diagnostic : managed.preflightDiagnostics)
        result.diagnostics.push_back(
            {diagnostic.code, diagnostic.message, diagnostic.path});
    std::error_code ignored;
    std::filesystem::remove(reportPath, ignored);
    return result;
}

ProjectToolResult SagaProjectKitClient::Validate(
    const std::filesystem::path& manifest) const
{
    const auto report = TemporaryReport("project-validation");
    return RunReportCommand({"resolve", "--project", manifest.string(),
                             "--out", report.string()}, report);
}

ProjectToolResult SagaProjectKitClient::CreateStarterArena(
    const ProjectTemplateRequest& request) const
{
    auto compatible = request;
    compatible.templateId = ProjectTemplateId::StarterArena;
    return CreateProject(compatible);
}

ProjectToolResult SagaProjectKitClient::CreateProject(
    const ProjectTemplateRequest& request) const
{
    std::error_code error;
    if (!std::filesystem::is_directory(request.seed, error))
        return MissingResource("Project.Template.SeedMissing",
            "The selected installed project template is missing.",
            request.seed);
    if (!std::filesystem::is_directory(
            request.engineRoot / "Engine/Distribution", error))
        return MissingResource("Project.Toolchain.EngineDistributionMissing",
            "The installed engine package distribution is missing.",
            request.engineRoot / "Engine/Distribution");
    if (!std::filesystem::is_regular_file(request.runtimeBridge, error))
        return MissingResource("Project.Toolchain.RuntimeBridgeMissing",
            "The SagaScript runtime bridge required for project creation is missing.",
            request.runtimeBridge);

    const auto plan = TemporaryReport("template-plan");
    ToolProcessRequest planOptions;
    planOptions.timeout = timeout_;
    auto planProcess = runtime_.Run(ManagedToolId::ProjectKit,
        {"template-plan", "--destination", request.destination.string(),
        "--display-name", request.displayName, "--template-id",
        request.templateId == ProjectTemplateId::ArenaRush ?
            "arena-rush" : "starter-arena",
        "--template", request.seed.string(),
        "--out", plan.string()}, std::move(planOptions));
    if (!planProcess.Succeeded() || !std::filesystem::is_regular_file(plan))
    {
        std::error_code ignored;
        std::filesystem::remove(plan, ignored);
        return ProcessFailure(std::move(planProcess.process),
            planProcess.preflightDiagnostics.empty() ?
                "Project.Template.PlanFailed" :
                planProcess.preflightDiagnostics.front().code, plan);
    }

    const auto createReport = TemporaryReport("template-create");
    const auto packagerInvocation =
        runtime_.PrepareInvocation(ManagedToolId::Packager, {});
    if (!packagerInvocation.Ready())
    {
        std::error_code ignored;
        std::filesystem::remove(plan, ignored);
        const auto& diagnostic = packagerInvocation.diagnostics.front();
        return MissingResource(diagnostic.code, diagnostic.message,
            diagnostic.path);
    }
    const auto& packager = packagerInvocation.descriptor;
    ToolProcessRequest createOptions;
    createOptions.timeout = timeout_;
    auto createProcess = runtime_.Run(ManagedToolId::ProjectKit,
        {"template-create", "--plan", plan.string(),
        "--dotnet-host", packager.host.string(),
        "--sagapack-dll", packager.dll.string(),
        "--engine-root", request.engineRoot.string(),
        "--runtime-bridge", request.runtimeBridge.string(),
        "--out", createReport.string()}, std::move(createOptions));
    auto result = ParseTemplateCreateReport(createReport,
        std::move(createProcess.process));
    for (const auto& diagnostic : createProcess.preflightDiagnostics)
        result.diagnostics.push_back(
            {diagnostic.code, diagnostic.message, diagnostic.path});
    std::error_code ignored;
    std::filesystem::remove(plan, ignored);
    std::filesystem::remove(createReport, ignored);
    if (!result.passed) return result;

    std::filesystem::path manifest;
    for (const auto& entry : std::filesystem::directory_iterator(request.destination, ignored))
        if (entry.is_regular_file() && entry.path().extension() == ".sagaproj") manifest = entry.path();
    if (manifest.empty())
    {
        result.passed = false;
        result.diagnostics.push_back({"Project.TemplateManifestMissing",
                                      "Created project does not contain one .sagaproj manifest.",
                                      request.destination});
        return result;
    }
    return Validate(manifest);
}

} // namespace SagaEditor
