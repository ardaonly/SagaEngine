/// @file SagaWorkspaceResolver.cpp
/// @brief Product manifest backed Saga workspace resolution implementation.

#include "SagaWorkspaceResolver.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace SagaProduct
{
namespace
{

constexpr const char* kProjectManifestFile = "saga.project.json";
constexpr const char* kDefaultWorkspaceId = "builtin.basic";
constexpr const char* kDefaultDisplayName = "Basic Workspace";
constexpr const char* kDefaultEditorProfile = "saga.profile.basic";
constexpr const char* kDefaultRuntimeRole = "SagaRuntime";
constexpr const char* kDefaultServerRole = "SagaServer";

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

[[nodiscard]] std::filesystem::path ResolveProjectRoot(
    const std::filesystem::path& selected)
{
    if (selected.filename() == kProjectManifestFile)
    {
        return selected.parent_path();
    }
    return selected;
}

[[nodiscard]] bool LoadProjectManifest(
    const std::filesystem::path& root,
    SagaWorkspaceDefinition& workspace,
    std::string& error)
{
    const std::filesystem::path manifestPath = root / kProjectManifestFile;
    if (!std::filesystem::exists(manifestPath))
    {
        error = "Saga project manifest is missing: " + manifestPath.string();
        return false;
    }

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

    if (!manifest.contains("projectId") || !manifest["projectId"].is_string() ||
        !manifest.contains("displayName") || !manifest["displayName"].is_string())
    {
        error =
            "Saga project manifest must contain string projectId and displayName.";
        return false;
    }

    workspace.id = manifest["projectId"].get<std::string>();
    workspace.displayName = manifest["displayName"].get<std::string>();
    workspace.root = std::filesystem::absolute(root);
    workspace.editorProfile =
        manifest.value("editorProfile", std::string{kDefaultEditorProfile});
    workspace.runtimeRole =
        manifest.value("runtimeRole", std::string{kDefaultRuntimeRole});
    workspace.serverRole =
        manifest.value("serverRole", std::string{kDefaultServerRole});
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
    workspace.serverRole = kDefaultServerRole;
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

    const std::filesystem::path root = ResolveProjectRoot(selectedRoot);
    if (root.empty() || !std::filesystem::exists(root))
    {
        result.error = "Saga workspace root is missing: " + root.string();
        return result;
    }

    if (selectedBuiltin)
    {
        result.workspace = MakeBuiltinWorkspace(root);
        result.ok = true;
        return result;
    }

    if (!LoadProjectManifest(root, result.workspace, result.error))
    {
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace SagaProduct
