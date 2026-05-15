/// @file SagaWorkspaceResolver.cpp
/// @brief SDE-backed Saga workspace resolution implementation.

#include "SagaWorkspaceResolver.h"

#include "SDE/Compilation/CompiledModelGraph.h"
#include "SDE/Compiler/CompilerFacade.h"
#include "SDE/Validation/CompileState.h"
#include "SDE/Validation/Diagnostic.h"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <variant>

namespace SagaProduct
{
namespace
{

constexpr const char* kWorkspaceModelId = "SagaWorkspace";
constexpr const char* kDefaultWorkspaceId = "builtin.basic";

[[nodiscard]] bool ContainsJsonFile(const std::filesystem::path& dir)
{
    if (!std::filesystem::exists(dir))
    {
        return false;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool ContainsSdeFile(const std::filesystem::path& dir)
{
    if (!std::filesystem::exists(dir))
    {
        return false;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".sde")
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string TextField(const SDE::CompiledInstance& instance,
                                    const std::string& fieldId)
{
    const SDE::CompiledValue* value = instance.GetField(fieldId);
    if (value == nullptr)
    {
        return {};
    }
    if (const auto* text = std::get_if<SDE::CompiledText>(&value->data))
    {
        return *text;
    }
    return {};
}

[[nodiscard]] std::string FormatDiagnostic(const SDE::Diagnostic& diagnostic)
{
    std::string text = diagnostic.code + ": " + diagnostic.message;
    if (!diagnostic.location.file.empty())
    {
        text += " (" + diagnostic.location.file + ")";
    }
    return text;
}

[[nodiscard]] std::filesystem::path ResolveSelector(
    const SagaWorkspaceResolveRequest& request,
    SagaWorkspaceResolveResult& result)
{
    const std::string selector =
        request.selector.empty() ? "builtin:basic" : request.selector;
    if (selector == "builtin:basic")
    {
        return request.builtInBasicRoot;
    }
    if (selector.rfind("builtin:", 0) == 0)
    {
        result.error = "Unknown built-in Saga workspace selector: " + selector;
        return {};
    }
    return selector;
}

[[nodiscard]] std::filesystem::path ResolveProjectSdeRoot(
    const std::filesystem::path& candidate)
{
    const std::filesystem::path defaultSdeRoot = candidate / ".sde";
    if (ContainsSdeFile(defaultSdeRoot / "source") ||
        (ContainsJsonFile(defaultSdeRoot / "schemas") &&
        ContainsJsonFile(defaultSdeRoot / "data"))
    )
    {
        return defaultSdeRoot;
    }

    if (ContainsSdeFile(candidate / "source") ||
        (ContainsJsonFile(candidate / "schemas") &&
        ContainsJsonFile(candidate / "data"))
    )
    {
        return candidate;
    }

    const std::filesystem::path manifestPath = candidate / "saga.project.json";
    if (std::filesystem::exists(manifestPath))
    {
        std::ifstream input(manifestPath);
        if (input.is_open())
        {
            try
            {
                nlohmann::json manifest;
                input >> manifest;
                const std::string configured =
                    manifest.value("sdeRoot", std::string{".sde"});
                const std::filesystem::path configuredPath =
                    std::filesystem::path(configured).is_absolute()
                        ? std::filesystem::path(configured)
                        : candidate / configured;
                if (ContainsSdeFile(configuredPath / "source") ||
                    (ContainsJsonFile(configuredPath / "schemas") &&
                    ContainsJsonFile(configuredPath / "data"))
                )
                {
                    return configuredPath;
                }
            }
            catch (const nlohmann::json::exception&)
            {
            }
        }
    }

    // Compatibility for pre-.sde workspaces.
    return candidate;
}

} // namespace

SagaWorkspaceResolveResult SagaWorkspaceResolver::Resolve(
    const SagaWorkspaceResolveRequest& request) const
{
    SagaWorkspaceResolveResult result;
    const std::filesystem::path selectedRoot = ResolveSelector(request, result);
    if (!result.error.empty())
    {
        return result;
    }

    const std::filesystem::path root = ResolveProjectSdeRoot(selectedRoot);
    if (root.empty() || !std::filesystem::exists(root))
    {
        result.error = "Saga workspace SDE root is missing: " + root.string();
        return result;
    }
    if (!ContainsSdeFile(root / "source") &&
        (!ContainsJsonFile(root / "schemas") || !ContainsJsonFile(root / "data")))
    {
        result.error =
            "Saga workspace SDE root must contain .sde source or legacy schema/data JSON files: " +
            root.string();
        return result;
    }

    SDE::CompilerFacade compiler;
    SDE::CompileRequest compileRequest;
    compileRequest.workspaceRoot = root;
    compileRequest.outputRoot = root / "artifacts";
    const SDE::CompilerFacadeResult compiled = compiler.Compile(compileRequest);
    for (const SDE::Diagnostic& diagnostic : compiled.project.validation.diagnostics)
    {
        result.diagnostics.push_back(FormatDiagnostic(diagnostic));
    }

    if (!SDE::IsUsable(compiled.project.state) ||
        !compiled.project.graph.has_value())
    {
        result.error = "Saga workspace SDE validation failed: " +
            SDE::StateName(compiled.project.state);
        return result;
    }

    const std::vector<const SDE::CompiledInstance*> workspaces =
        compiled.project.graph->AllOf(kWorkspaceModelId);
    if (workspaces.empty())
    {
        result.error =
            "Saga workspace SDE does not define a SagaWorkspace instance.";
        return result;
    }

    const SDE::CompiledInstance* selected = workspaces.front();
    if (const SDE::CompiledInstance* exact =
            compiled.project.graph->Find(kWorkspaceModelId, kDefaultWorkspaceId))
    {
        selected = exact;
    }

    result.workspace.id = selected->instanceId;
    result.workspace.displayName = TextField(*selected, "displayName");
    result.workspace.root = std::filesystem::absolute(root);
    result.workspace.editorProfile = TextField(*selected, "editorProfile");
    result.workspace.runtimeRole = TextField(*selected, "runtimeRole");
    result.workspace.serverRole = TextField(*selected, "serverRole");
    result.workspace.artifactHash = compiled.project.hashes.artifactHash;

    if (result.workspace.displayName.empty() ||
        result.workspace.editorProfile.empty() ||
        result.workspace.runtimeRole.empty() ||
        result.workspace.serverRole.empty())
    {
        result.error =
            "Saga workspace SDE is missing required workspace fields.";
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace SagaProduct
