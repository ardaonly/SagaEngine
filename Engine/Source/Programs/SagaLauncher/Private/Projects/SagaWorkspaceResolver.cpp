/// @file SagaWorkspaceResolver.cpp
/// @brief Product manifest backed Saga workspace resolution implementation.

#include "Projects/SagaWorkspaceResolver.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

namespace SagaProduct
{
namespace
{

constexpr const char* kDefaultWorkspaceId = "builtin.basic";
constexpr const char* kDefaultDisplayName = "Basic Workspace";
constexpr const char* kDefaultEditorProfile = "saga.profile.basic";
constexpr const char* kDefaultRuntimeRole = "SagaRuntime";

[[nodiscard]] std::filesystem::path ResolveSelector(
    const SagaWorkspaceResolveRequest& request,
    SagaWorkspaceResolveResult& result,
    bool& selectedBuiltin)
{
    const std::string selector =
        request.selector.empty() ? "builtin:basic" : request.selector;
    if (selector == "builtin:basic")
    {
        selectedBuiltin = true;
        return request.builtInBasicRoot.empty()
            ? std::filesystem::current_path()
            : request.builtInBasicRoot;
    }
    if (selector.rfind("builtin:", 0) == 0)
    {
        result.error = "Unknown built-in Saga workspace selector: " + selector;
        return {};
    }
    return selector;
}

[[nodiscard]] std::filesystem::path ResolveProjectManifest(
    const std::filesystem::path& selected,
    std::string& error)
{
    std::error_code filesystemError;
    if (std::filesystem::is_regular_file(selected, filesystemError))
    {
        if (selected.extension() != ".sagaproj")
        {
            error = "Explicit Saga project input must be a .sagaproj file.";
            return {};
        }
        return std::filesystem::weakly_canonical(selected, filesystemError);
    }

    if (!std::filesystem::is_directory(selected, filesystemError))
    {
        error = "Saga project input is not a file or directory: " +
            selected.string();
        return {};
    }

    std::vector<std::filesystem::path> candidates;
    for (const auto& entry :
         std::filesystem::directory_iterator(selected, filesystemError))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".sagaproj")
            candidates.push_back(entry.path());
    }
    if (filesystemError)
    {
        error = "Cannot enumerate Saga project directory: " +
            filesystemError.message();
        return {};
    }
    std::sort(candidates.begin(), candidates.end());
    if (candidates.size() != 1)
    {
        error = candidates.empty()
            ? "Saga project directory contains no .sagaproj manifest."
            : "Saga project directory contains more than one .sagaproj manifest.";
        return {};
    }
    return std::filesystem::weakly_canonical(candidates.front(), filesystemError);
}

[[nodiscard]] bool LoadProjectManifest(
    const std::filesystem::path& manifestPath,
    SagaWorkspaceDefinition& workspace,
    std::string& error)
{
    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        error = "Cannot open Saga project manifest: " + manifestPath.string();
        return false;
    }

    nlohmann::json manifest;
    try
    {
        input >> manifest;
    }
    catch (const nlohmann::json::exception& e)
    {
        error = std::string("Saga project manifest JSON is invalid: ") +
            e.what();
        return false;
    }

    if (!manifest.contains("schemaVersion") ||
        !manifest["schemaVersion"].is_number_integer() ||
        manifest["schemaVersion"].get<int>() != 0)
    {
        error = ".sagaproj requires schemaVersion 0.";
        return false;
    }
    if (!manifest.contains("projectId") || !manifest["projectId"].is_string() ||
        manifest["projectId"].get<std::string>().empty() ||
        !manifest.contains("displayName") || !manifest["displayName"].is_string() ||
        manifest["displayName"].get<std::string>().empty())
    {
        error =
            "Saga project manifest must contain string projectId and displayName.";
        return false;
    }

    workspace.id = manifest["projectId"].get<std::string>();
    workspace.displayName = manifest["displayName"].get<std::string>();
    workspace.root = std::filesystem::absolute(manifestPath.parent_path());
    workspace.editorProfile =
        manifest.value("editorProfile", std::string{kDefaultEditorProfile});
    workspace.runtimeRole =
        manifest.value("runtimeRole", std::string{kDefaultRuntimeRole});
    workspace.serverRole =
        manifest.value("serverRole", std::string{});
    return true;
}

[[nodiscard]] SagaWorkspaceDefinition MakeBuiltinWorkspace(
    const std::filesystem::path& root)
{
    SagaWorkspaceDefinition workspace;
    workspace.id = kDefaultWorkspaceId;
    workspace.displayName = kDefaultDisplayName;
    workspace.root = std::filesystem::absolute(root);
    workspace.editorProfile = kDefaultEditorProfile;
    workspace.runtimeRole = kDefaultRuntimeRole;
    return workspace;
}

} // namespace

SagaWorkspaceResolveResult SagaWorkspaceResolver::Resolve(
    const SagaWorkspaceResolveRequest& request) const
{
    SagaWorkspaceResolveResult result;
    bool selectedBuiltin = false;
    const std::filesystem::path selectedRoot =
        ResolveSelector(request, result, selectedBuiltin);
    if (!result.error.empty())
    {
        return result;
    }

    if (selectedRoot.empty() || !std::filesystem::exists(selectedRoot))
    {
        result.error = "Saga workspace root is missing: " + selectedRoot.string();
        return result;
    }

    if (selectedBuiltin)
    {
        result.workspace = MakeBuiltinWorkspace(selectedRoot);
        result.ok = true;
        return result;
    }

    const std::filesystem::path manifestPath =
        ResolveProjectManifest(selectedRoot, result.error);
    if (manifestPath.empty() ||
        !LoadProjectManifest(manifestPath, result.workspace, result.error))
    {
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace SagaProduct
