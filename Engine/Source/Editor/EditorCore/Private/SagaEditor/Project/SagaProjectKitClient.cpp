// SPDX-FileCopyrightText: 2026 Arda Koyuncu
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

std::filesystem::path DiscoverEngineRoot()
{
    if (const auto configured = EnvironmentPath("SAGA_ENGINE_ROOT"); !configured.empty())
        if (const auto found = FindEngineRoot(configured); !found.empty()) return found;
    if (const auto found = FindEngineRoot(std::filesystem::current_path()); !found.empty())
        return found;
#ifndef _WIN32
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error)
        if (const auto found = FindEngineRoot(executable); !found.empty()) return found;
#endif
    return {};
}

std::filesystem::path ToolPath(const std::filesystem::path& root,
                               std::string_view relative,
                               const char* overrideName)
{
    if (const auto configured = EnvironmentPath(overrideName); !configured.empty())
        return configured;
    auto candidate = root / relative;
#ifdef _WIN32
    candidate += ".exe";
#endif
    return candidate;
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
}

ProjectTemplateRequest MakeStarterArenaTemplateRequest(
    std::filesystem::path destination, std::string displayName)
{
    ProjectTemplateRequest request;
    request.destination = std::move(destination);
    request.displayName = std::move(displayName);
    request.engineRoot = DiscoverEngineRoot();
    request.seed = request.engineRoot / "Samples/StarterArena";
    request.sagaPackagerExecutable = ToolPath(request.engineRoot,
        "Tools/SagaPackager/bin/Release/net10.0/sagapack", "SAGA_PACKAGE_TOOL");
    request.runtimeBridge = EnvironmentPath("SAGA_RUNTIME_BRIDGE");
    if (request.runtimeBridge.empty())
        request.runtimeBridge = request.engineRoot /
            "Engine/Managed/SagaScript.RuntimeBridge/bin/Release/net10.0/SagaScript.RuntimeBridge.dll";
    return request;
}

SagaProjectKitClient::SagaProjectKitClient(std::filesystem::path executable,
                                           std::chrono::milliseconds timeout)
    : executable_(std::move(executable)), timeout_(timeout) {}

ProjectToolResult SagaProjectKitClient::RunReportCommand(
    std::vector<std::string> arguments,
    const std::filesystem::path& reportPath) const
{
    ToolProcessRequest request;
    request.executable = executable_;
    request.arguments = std::move(arguments);
    request.timeout = timeout_;
    const auto process = ToolProcessRunner{}.Run(request);
    auto result = ParseReport(reportPath, process);
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
    const auto plan = TemporaryReport("template-plan");
    ToolProcessRequest planRequest;
    planRequest.executable = executable_;
    planRequest.arguments = {"template-plan", "--destination", request.destination.string(),
        "--display-name", request.displayName, "--template", request.seed.string(),
        "--out", plan.string()};
    planRequest.timeout = timeout_;
    auto planProcess = ToolProcessRunner{}.Run(planRequest);
    if (!planProcess.Succeeded() || !std::filesystem::is_regular_file(plan))
    {
        std::error_code ignored;
        std::filesystem::remove(plan, ignored);
        return ProcessFailure(std::move(planProcess), "Project.Template.PlanFailed", plan);
    }

    const auto createReport = TemporaryReport("template-create");
    ToolProcessRequest createRequest;
    createRequest.executable = executable_;
    createRequest.arguments = {"template-create", "--plan", plan.string(),
        "--sagapack", request.sagaPackagerExecutable.string(),
        "--engine-root", request.engineRoot.string(),
        "--runtime-bridge", request.runtimeBridge.string(),
        "--out", createReport.string()};
    createRequest.timeout = timeout_;
    auto result = ParseTemplateCreateReport(createReport,
        ToolProcessRunner{}.Run(createRequest));
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
