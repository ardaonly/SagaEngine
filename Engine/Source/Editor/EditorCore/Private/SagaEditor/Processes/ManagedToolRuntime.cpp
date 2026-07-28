// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Processes/ManagedToolRuntime.h"

#include "SagaManagedToolRuntimeConfig.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <tuple>
#include <string_view>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::json;

std::filesystem::path EnvironmentPath(const char* name)
{
    const auto* value = std::getenv(name);
    return value && *value ? std::filesystem::path(value) : std::filesystem::path{};
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

std::filesystem::path HostPath(std::filesystem::path root)
{
#ifdef _WIN32
    return root / "dotnet.exe";
#else
    return root / "dotnet";
#endif
}

std::filesystem::path Canonical(const std::filesystem::path& path)
{
    std::error_code error;
    const auto value = std::filesystem::weakly_canonical(path, error);
    return error ? std::filesystem::path{} : value;
}

bool Contained(const std::filesystem::path& root, const std::filesystem::path& path)
{
    const auto canonicalRoot = Canonical(root);
    const auto canonicalPath = Canonical(path);
    if (canonicalRoot.empty() || canonicalPath.empty()) return false;
    const auto relative = canonicalPath.lexically_relative(canonicalRoot);
    return relative.empty() || (!relative.is_absolute() &&
        *relative.begin() != std::filesystem::path(".."));
}

bool IsStage(const std::filesystem::path& root)
{
    std::error_code error;
    return std::filesystem::is_regular_file(root / "managed-tool-runtime.json", error) &&
        std::filesystem::is_regular_file(HostPath(root / "runtime/dotnet"), error);
}

std::filesystem::path FindInstalledRoot(std::filesystem::path anchor)
{
    std::error_code error;
    anchor = std::filesystem::absolute(anchor, error);
    if (error) return {};
    auto cursor = std::filesystem::is_regular_file(anchor, error) ?
        anchor.parent_path() : anchor;
    while (!cursor.empty())
    {
        if (IsStage(cursor) && std::filesystem::is_directory(cursor / "bin", error))
            return cursor;
        const auto parent = cursor.parent_path();
        if (parent == cursor) break;
        cursor = parent;
    }
    return {};
}

std::filesystem::path FindRepositoryRoot(std::filesystem::path anchor)
{
    std::error_code error;
    anchor = std::filesystem::absolute(anchor, error);
    if (error) return {};
    auto cursor = std::filesystem::is_regular_file(anchor, error) ?
        anchor.parent_path() : anchor;
    while (!cursor.empty())
    {
        if (std::filesystem::is_regular_file(
                cursor / "Config/managed-tool-runtime.json", error) &&
            std::filesystem::is_directory(cursor / "Tools/SagaScript", error))
            return cursor;
        const auto parent = cursor.parent_path();
        if (parent == cursor) break;
        cursor = parent;
    }
    return {};
}

struct ToolIdentity
{
    const char* directory;
    const char* assembly;
};

ToolIdentity Identity(ManagedToolId id)
{
    switch (id)
    {
    case ManagedToolId::ProjectKit:
        return {SAGA_MANAGED_PROJECTKIT_DIRECTORY, SAGA_MANAGED_PROJECTKIT_ASSEMBLY};
    case ManagedToolId::Script:
        return {SAGA_MANAGED_SCRIPT_DIRECTORY, SAGA_MANAGED_SCRIPT_ASSEMBLY};
    case ManagedToolId::Packager:
        return {SAGA_MANAGED_PACKAGER_DIRECTORY, SAGA_MANAGED_PACKAGER_ASSEMBLY};
    }
    return {};
}

ManagedToolDiagnostic Diagnostic(std::string code, std::string message,
                                 std::filesystem::path path)
{
    return {std::move(code), std::move(message), std::move(path), {}};
}

bool ExecutableFile(const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) return false;
#ifndef _WIN32
    const auto permissions = std::filesystem::status(path, error).permissions();
    if (error || (permissions & (std::filesystem::perms::owner_exec |
                                 std::filesystem::perms::group_exec |
                                 std::filesystem::perms::others_exec)) ==
                    std::filesystem::perms::none)
        return false;
#endif
    return true;
}

std::tuple<int, int, int> VersionTuple(std::string_view version)
{
    int values[3]{};
    std::size_t begin = 0;
    for (int& value : values)
    {
        const auto end = version.find('.', begin);
        const auto token = version.substr(begin, end == std::string_view::npos
            ? version.size() - begin : end - begin);
        if (token.empty()) throw std::invalid_argument("empty version component");
        std::size_t consumed = 0;
        value = std::stoi(std::string(token), &consumed);
        if (consumed != token.size()) throw std::invalid_argument("invalid version component");
        if (end == std::string_view::npos)
        {
            begin = version.size();
            break;
        }
        begin = end + 1;
    }
    return {values[0], values[1], values[2]};
}

std::vector<ManagedToolDiagnostic> Preflight(const ManagedToolDescriptor& descriptor)
{
    std::vector<ManagedToolDiagnostic> diagnostics;
    if (descriptor.trustedRoot.empty() || descriptor.host.empty())
    {
        diagnostics.push_back(Diagnostic(
            "Product.Toolchain.RuntimeMissing",
            "Saga's private managed-tool runtime could not be resolved.",
            descriptor.trustedRoot));
        return diagnostics;
    }
    const auto requiredPaths = {
        descriptor.manifest, descriptor.dll, descriptor.runtimeConfig, descriptor.deps};
    for (const auto& path : requiredPaths)
    {
        if (!Contained(descriptor.trustedRoot, path))
        {
            diagnostics.push_back(Diagnostic(
                "Product.Toolchain.RuntimeCorrupt",
                "A managed-tool path escapes the trusted Saga runtime root.", path));
            return diagnostics;
        }
    }
    if (descriptor.resolutionMode != ManagedToolResolutionMode::DeveloperHost &&
        (!Contained(descriptor.trustedRoot, descriptor.host) ||
         !Contained(descriptor.trustedRoot, descriptor.runtimeRoot)))
    {
        diagnostics.push_back(Diagnostic(
            "Product.Toolchain.RuntimeCorrupt",
            "Saga's managed runtime escapes the trusted product root.",
            descriptor.runtimeRoot));
        return diagnostics;
    }
    if (!ExecutableFile(descriptor.host))
    {
        diagnostics.push_back(Diagnostic(
            "Product.Toolchain.RuntimeMissing",
            "Saga's private dotnet host is missing or is not executable.",
            descriptor.host));
        return diagnostics;
    }
    const auto shared = descriptor.runtimeRoot / "shared" /
        descriptor.framework / descriptor.runtimeVersion;
    const auto fxr = descriptor.runtimeRoot / "host/fxr" / descriptor.runtimeVersion;
    std::error_code error;
    if (!std::filesystem::is_directory(shared, error) ||
        !std::filesystem::is_directory(fxr, error) ||
        !Contained(descriptor.runtimeRoot, shared) ||
        !Contained(descriptor.runtimeRoot, fxr))
    {
        diagnostics.push_back(Diagnostic(
            "Product.Toolchain.RuntimeCorrupt",
            "Saga's private runtime does not contain the exact pinned shared framework and hostfxr.",
            descriptor.runtimeRoot));
        return diagnostics;
    }
    if (!std::filesystem::is_regular_file(descriptor.dll, error))
    {
        diagnostics.push_back(Diagnostic(
            "Product.Toolchain.ToolMissing",
            "The managed tool assembly is missing.", descriptor.dll));
        return diagnostics;
    }
    if (!std::filesystem::is_regular_file(descriptor.runtimeConfig, error) ||
        !std::filesystem::is_regular_file(descriptor.deps, error) ||
        !std::filesystem::is_regular_file(descriptor.manifest, error))
    {
        diagnostics.push_back(Diagnostic(
            "Product.Toolchain.ToolMetadataMissing",
            "The managed tool runtimeconfig or deps metadata is missing.",
            descriptor.toolRoot));
        return diagnostics;
    }
    try
    {
        std::ifstream manifestInput(descriptor.manifest, std::ios::binary);
        const auto manifest = Json::parse(manifestInput);
        if (manifest.value("schemaVersion", 0) != 1 ||
            manifest.value("framework", std::string{}) != descriptor.framework ||
            manifest.value("targetFramework", std::string{}) !=
                SAGA_MANAGED_TARGET_FRAMEWORK ||
            manifest.value("minimumRuntimeVersion", std::string{}) !=
                descriptor.minimumVersion ||
            manifest.value("runtimeVersion", std::string{}) !=
                descriptor.runtimeVersion ||
            manifest.value("rid", std::string{}) != descriptor.rid)
        {
            diagnostics.push_back(Diagnostic(
                "Product.Toolchain.RuntimeIncompatible",
                "The managed runtime manifest does not match Saga's pinned runtime.",
                descriptor.manifest));
            return diagnostics;
        }
        const auto identity = Identity(descriptor.id);
        bool identityFound = false;
        for (const auto& tool : manifest.at("tools"))
        {
            if (tool.value("id", std::string{}) == ManagedToolName(descriptor.id) ||
                (tool.value("directory", std::string{}) == identity.directory &&
                 tool.value("assembly", std::string{}) == identity.assembly))
            {
                identityFound = true;
                break;
            }
        }
        if (!identityFound)
        {
            diagnostics.push_back(Diagnostic(
                "Product.Toolchain.ToolMetadataMissing",
                "The managed runtime manifest omits the requested tool identity.",
                descriptor.manifest));
            return diagnostics;
        }

        std::ifstream input(descriptor.runtimeConfig, std::ios::binary);
        const auto config = Json::parse(input);
        const auto& runtime = config.at("runtimeOptions");
        const auto& framework = runtime.at("framework");
        const auto family = framework.value("name", std::string{});
        const auto version = framework.value("version", std::string{});
        const auto tfm = runtime.value("tfm", std::string{});
        if (family != descriptor.framework || tfm != SAGA_MANAGED_TARGET_FRAMEWORK ||
            version.empty())
        {
            diagnostics.push_back(Diagnostic(
                "Product.Toolchain.RuntimeIncompatible",
                "The tool targets a different framework family or target framework.",
                descriptor.runtimeConfig));
            return diagnostics;
        }
        const auto requested = VersionTuple(version);
        const auto minimum = VersionTuple(descriptor.minimumVersion);
        const auto pinned = VersionTuple(descriptor.runtimeVersion);
        if (std::get<0>(requested) != std::get<0>(pinned) ||
            requested < minimum || pinned < requested)
        {
            diagnostics.push_back(Diagnostic(
                "Product.Toolchain.RuntimeIncompatible",
                "The tool runtime requirement is incompatible with Saga's pinned runtime.",
                descriptor.runtimeConfig));
            return diagnostics;
        }
    }
    catch (const std::exception& error)
    {
        auto diagnostic = Diagnostic(
            "Product.Toolchain.ToolMetadataMissing",
            "The managed tool runtimeconfig is malformed.",
            descriptor.runtimeConfig);
        diagnostic.metadata.emplace("detail",
            std::string(error.what()).substr(0, 256));
        diagnostics.push_back(std::move(diagnostic));
    }
    return diagnostics;
}

ToolProcessResult PreflightProcessFailure(
    const std::vector<ManagedToolDiagnostic>& diagnostics)
{
    ToolProcessResult process;
    if (!diagnostics.empty())
    {
        process.diagnosticCode = diagnostics.front().code;
        process.diagnosticMessage = diagnostics.front().message;
        process.standardError = diagnostics.front().message;
    }
    return process;
}
} // namespace

const char* ManagedToolName(ManagedToolId id) noexcept
{
    return Identity(id).assembly;
}

ManagedToolRuntime::ManagedToolRuntime(ManagedToolRuntimeOptions options)
    : options_(std::move(options))
{}

ManagedToolDescriptor ManagedToolRuntime::Resolve(ManagedToolId id) const
{
    ManagedToolDescriptor descriptor;
    descriptor.id = id;
    descriptor.framework = SAGA_MANAGED_FRAMEWORK;
    descriptor.minimumVersion = SAGA_MANAGED_MINIMUM_RUNTIME_VERSION;
    descriptor.runtimeVersion = SAGA_MANAGED_RUNTIME_VERSION;
    descriptor.rid = SAGA_MANAGED_RUNTIME_RID;

    const auto anchor = options_.productExecutable.empty() ?
        CurrentExecutable() : options_.productExecutable;
    std::filesystem::path stage;
    if (!options_.testStage.empty())
    {
        stage = options_.testStage;
        descriptor.resolutionMode = ManagedToolResolutionMode::TestStage;
    }
    else if (const auto installed = FindInstalledRoot(anchor); !installed.empty())
    {
        stage = installed;
        descriptor.resolutionMode = ManagedToolResolutionMode::Installed;
    }
    else if (const auto configured = EnvironmentPath("SAGA_MANAGED_TOOL_STAGE");
             !configured.empty())
    {
        stage = configured;
        descriptor.resolutionMode = ManagedToolResolutionMode::EnvironmentStage;
    }
    else if (!anchor.empty())
    {
        const auto candidate = Canonical(anchor).parent_path().parent_path() /
            SAGA_MANAGED_BUILD_STAGE_DIRECTORY;
        if (IsStage(candidate))
        {
            stage = candidate;
            descriptor.resolutionMode = ManagedToolResolutionMode::BuildStage;
        }
    }

    const auto identity = Identity(id);
    if (!stage.empty())
    {
        descriptor.trustedRoot = Canonical(stage);
        descriptor.runtimeRoot = descriptor.trustedRoot / "runtime/dotnet";
        descriptor.host = HostPath(descriptor.runtimeRoot);
        descriptor.toolRoot = descriptor.trustedRoot / "tools" / identity.directory;
    }
    else
    {
        auto host = options_.developerDotnetHost;
        if (host.empty()) host = EnvironmentPath("SAGA_DEVELOPER_DOTNET_HOST");
        const auto repository = FindRepositoryRoot(anchor);
        if (!host.empty() && !repository.empty())
        {
            descriptor.resolutionMode = ManagedToolResolutionMode::DeveloperHost;
            descriptor.trustedRoot = Canonical(repository);
            descriptor.manifest =
                descriptor.trustedRoot / "Config/managed-tool-runtime.json";
            descriptor.host = Canonical(host);
            descriptor.runtimeRoot = descriptor.host.parent_path();
            descriptor.toolRoot = descriptor.trustedRoot / "Tools" /
                identity.directory / "bin/Release/net10.0";
        }
    }
    if (!stage.empty())
        descriptor.manifest = descriptor.trustedRoot / "managed-tool-runtime.json";
    descriptor.dll = descriptor.toolRoot / (std::string(identity.assembly) + ".dll");
    descriptor.runtimeConfig = descriptor.toolRoot /
        (std::string(identity.assembly) + ".runtimeconfig.json");
    descriptor.deps = descriptor.toolRoot /
        (std::string(identity.assembly) + ".deps.json");
    return descriptor;
}

ManagedToolInvocation ManagedToolRuntime::PrepareInvocation(
    ManagedToolId id, std::vector<std::string> arguments,
    ToolProcessRequest processOptions) const
{
    ManagedToolInvocation invocation;
    invocation.descriptor = Resolve(id);
    invocation.diagnostics = Preflight(invocation.descriptor);
    if (!invocation.diagnostics.empty()) return invocation;
    processOptions.executable = invocation.descriptor.host;
    processOptions.arguments.insert(processOptions.arguments.end(),
        std::make_move_iterator(arguments.begin()),
        std::make_move_iterator(arguments.end()));
    processOptions.arguments.insert(processOptions.arguments.begin(),
        invocation.descriptor.dll.string());
    processOptions.environment["DOTNET_ROOT"] =
        invocation.descriptor.runtimeRoot.string();
    processOptions.environment["DOTNET_ROOT_X64"] =
        invocation.descriptor.runtimeRoot.string();
    processOptions.environment["DOTNET_HOST_PATH"] =
        invocation.descriptor.host.string();
    processOptions.environment["DOTNET_MULTILEVEL_LOOKUP"] = "0";
    // Managed tools emit canonical machine data and never use localized
    // collation. This keeps the portable Microsoft runtime independent of a
    // machine-global ICU installation (notably on NixOS).
    processOptions.environment["DOTNET_SYSTEM_GLOBALIZATION_INVARIANT"] = "1";
    invocation.process = std::move(processOptions);
    return invocation;
}

ManagedToolProcessResult ManagedToolRuntime::Run(
    ManagedToolId id, std::vector<std::string> arguments,
    ToolProcessRequest processOptions) const
{
    const auto invocation = PrepareInvocation(
        id, std::move(arguments), std::move(processOptions));
    ManagedToolProcessResult result;
    result.descriptor = invocation.descriptor;
    result.preflightDiagnostics = invocation.diagnostics;
    if (!invocation.Ready())
    {
        result.process = PreflightProcessFailure(result.preflightDiagnostics);
        return result;
    }
    result.process = ToolProcessRunner{}.Run(*invocation.process);
    if (result.process.classification == ToolProcessExitClassification::NotStarted)
    {
        result.process.diagnosticCode =
            "Product.Toolchain.ToolLaunchFailed";
    }
    else if (!result.process.Succeeded() &&
             result.process.classification != ToolProcessExitClassification::Cancelled &&
             result.process.classification != ToolProcessExitClassification::TimedOut)
    {
        result.process.diagnosticCode = "Product.Toolchain.ToolExited";
    }
    return result;
}

} // namespace SagaEditor
