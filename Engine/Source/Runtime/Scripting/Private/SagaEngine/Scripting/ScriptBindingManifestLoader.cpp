/// @file ScriptBindingManifestLoader.cpp
/// @brief Implements SagaScript binding manifest loading.

#include <SagaEngine/Scripting/ScriptBindingManifestLoader.hpp>

#include "SagaShared/Diagnostics/DiagnosticCategory.hpp"
#include "SagaShared/Diagnostics/DiagnosticSeverity.hpp"
#include "SagaShared/Diagnostics/DiagnosticSource.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace SagaEngine::Scripting
{

namespace
{

constexpr std::uint32_t kSupportedSchemaVersion = 1;

struct RuntimeOverlayEntry
{
    ScriptBindingSelfOverlay self;
};

[[nodiscard]] ScriptDiagnostic MakeDiagnostic(
    std::string code,
    std::string title,
    std::string message)
{
    ScriptDiagnostic diagnostic;
    diagnostic.diagnostic.severity =
        SagaShared::Diagnostics::DiagnosticSeverity::Error;
    diagnostic.diagnostic.category =
        SagaShared::Diagnostics::DiagnosticCategory::Script;
    diagnostic.diagnostic.source =
        SagaShared::Diagnostics::DiagnosticSource::Runtime;
    diagnostic.diagnostic.code.value = std::move(code);
    diagnostic.diagnostic.title = std::move(title);
    diagnostic.diagnostic.message = std::move(message);
    return diagnostic;
}

[[nodiscard]] std::filesystem::path ResolvePath(
    const std::filesystem::path& root,
    const std::filesystem::path& path)
{
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }

    return (root / path).lexically_normal();
}

[[nodiscard]] std::optional<nlohmann::json> LoadJson(
    const std::filesystem::path& path,
    std::string_view manifestName,
    ScriptBindingManifestLoadResult& result)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptBindingDiagnostics::ManifestMissing,
            "Script binding manifest missing",
            "A required SagaScript binding manifest could not be opened.");
        diagnostic.metadata["manifest"] = std::string(manifestName);
        diagnostic.metadata["path"] = path.generic_string();
        result.diagnostics.push_back(std::move(diagnostic));
        return std::nullopt;
    }

    nlohmann::json root;
    try
    {
        input >> root;
    }
    catch (const nlohmann::json::exception& exception)
    {
        auto diagnostic = MakeDiagnostic(
            ScriptBindingDiagnostics::ManifestInvalid,
            "Invalid script binding manifest",
            std::string("SagaScript binding manifest JSON parse failed: ") +
                exception.what());
        diagnostic.metadata["manifest"] = std::string(manifestName);
        diagnostic.metadata["path"] = path.generic_string();
        result.diagnostics.push_back(std::move(diagnostic));
        return std::nullopt;
    }

    return root;
}

[[nodiscard]] bool HasSupportedSchemaVersion(const nlohmann::json& root)
{
    const auto iterator = root.find("schemaVersion");
    if (iterator == root.end() ||
        (!iterator->is_number_integer() && !iterator->is_number_unsigned()))
    {
        return false;
    }

    if (iterator->is_number_unsigned())
    {
        return iterator->get<std::uint64_t>() == kSupportedSchemaVersion;
    }

    return iterator->get<std::int64_t>() ==
           static_cast<std::int64_t>(kSupportedSchemaVersion);
}

[[nodiscard]] bool HasStringField(
    const nlohmann::json& object,
    std::string_view field)
{
    const auto iterator = object.find(field);
    return iterator != object.end() && iterator->is_string();
}

[[nodiscard]] std::string ReadStringField(
    const nlohmann::json& object,
    std::string_view field)
{
    return object.at(std::string(field)).get<std::string>();
}

void AddManifestShapeDiagnostic(
    ScriptBindingManifestLoadResult& result,
    std::string_view manifest,
    std::string_view arrayField)
{
    auto diagnostic = MakeDiagnostic(
        ScriptBindingDiagnostics::ManifestInvalid,
        "Invalid script binding manifest",
        "SagaScript binding manifests must be schemaVersion 1 with the expected array field.");
    diagnostic.metadata["manifest"] = std::string(manifest);
    diagnostic.metadata["field"] = std::string(arrayField);
    result.diagnostics.push_back(std::move(diagnostic));
}

[[nodiscard]] bool ValidateArrayManifest(
    const nlohmann::json& root,
    std::string_view manifest,
    std::string_view arrayField,
    ScriptBindingManifestLoadResult& result)
{
    if (!root.is_object() ||
        !HasSupportedSchemaVersion(root) ||
        !root.contains(std::string(arrayField)) ||
        !root.at(std::string(arrayField)).is_array())
    {
        AddManifestShapeDiagnostic(result, manifest, arrayField);
        return false;
    }

    return true;
}

[[nodiscard]] std::unordered_set<std::string> ReadScriptIdSet(
    const nlohmann::json& root,
    std::string_view arrayField)
{
    std::unordered_set<std::string> scripts;
    for (const auto& entry : root.at(std::string(arrayField)))
    {
        if (entry.is_object() && HasStringField(entry, "scriptId"))
        {
            const auto scriptId = ReadStringField(entry, "scriptId");
            if (!scriptId.empty())
            {
                scripts.insert(scriptId);
            }
        }
    }
    return scripts;
}

[[nodiscard]] std::vector<ScriptBindingDefinition> ReadGeneratedBindings(
    const nlohmann::json& root,
    ScriptBindingManifestLoadResult& result)
{
    std::vector<ScriptBindingDefinition> bindings;
    std::unordered_set<std::string> bindingIds;
    const auto& generated = root.at("bindings");

    for (std::size_t index = 0; index < generated.size(); ++index)
    {
        const auto& binding = generated[index];
        if (!binding.is_object())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::ManifestInvalid,
                "Invalid script binding manifest",
                "Generated script binding entries must be JSON objects.");
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
            continue;
        }

        ScriptBindingDefinition definition;
        if (HasStringField(binding, "bindingId"))
        {
            definition.bindingId = ReadStringField(binding, "bindingId");
        }
        if (HasStringField(binding, "scriptId"))
        {
            definition.scriptId = ReadStringField(binding, "scriptId");
        }
        if (HasStringField(binding, "declaringType"))
        {
            definition.declaringType = ReadStringField(binding, "declaringType");
        }

        if (definition.bindingId.empty())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::MissingBindingId,
                "Script binding id missing",
                "Generated script binding entries must include bindingId.");
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
        }
        else if (!bindingIds.insert(definition.bindingId).second)
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::DuplicateBindingId,
                "Duplicate script binding id",
                "Generated script binding manifest contains duplicate bindingId values.");
            diagnostic.bindingId = definition.bindingId;
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
        }

        if (definition.scriptId.empty())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::MissingScriptId,
                "Script id missing",
                "Generated script binding entries must include scriptId.");
            diagnostic.bindingId = definition.bindingId;
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
        }

        if (definition.declaringType.empty())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::MissingDeclaringType,
                "Script declaring type missing",
                "Generated script binding entries must include declaringType.");
            diagnostic.bindingId = definition.bindingId;
            diagnostic.scriptId = definition.scriptId;
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
        }

        bindings.push_back(std::move(definition));
    }

    return bindings;
}

[[nodiscard]] bool TryReadSelfPolicy(
    const nlohmann::json& self,
    ScriptBindingSelfOverlay& overlay)
{
    if (!HasStringField(self, "policy"))
    {
        return false;
    }

    const auto policy = ReadStringField(self, "policy");
    if (policy == "existing")
    {
        overlay.policy = ScriptBindingSelfPolicy::Existing;
        return true;
    }
    if (policy == "create")
    {
        overlay.policy = ScriptBindingSelfPolicy::Create;
        return true;
    }

    return false;
}

[[nodiscard]] std::unordered_map<std::string, RuntimeOverlayEntry>
ReadRuntimeOverlays(
    const nlohmann::json& root,
    ScriptBindingManifestLoadResult& result)
{
    std::unordered_map<std::string, RuntimeOverlayEntry> overlays;
    const auto& runtimeBindings = root.at("bindings");

    for (std::size_t index = 0; index < runtimeBindings.size(); ++index)
    {
        const auto& binding = runtimeBindings[index];
        if (!binding.is_object())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::ManifestInvalid,
                "Invalid script runtime binding manifest",
                "Runtime script binding entries must be JSON objects.");
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
            continue;
        }

        const auto bindingId = HasStringField(binding, "bindingId")
            ? ReadStringField(binding, "bindingId")
            : std::string{};
        if (bindingId.empty())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::MissingBindingId,
                "Script binding id missing",
                "Runtime script binding entries must include bindingId.");
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
            continue;
        }
        if (overlays.find(bindingId) != overlays.end())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::DuplicateBindingId,
                "Duplicate script binding id",
                "Runtime script binding manifest contains duplicate bindingId values.");
            diagnostic.bindingId = bindingId;
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
            continue;
        }

        const auto selfIterator = binding.find("self");
        if (selfIterator == binding.end() || !selfIterator->is_object())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::InvalidSelfPolicy,
                "Invalid script self policy",
                "Runtime script binding entries must include a self policy object.");
            diagnostic.bindingId = bindingId;
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
            continue;
        }

        ScriptBindingSelfOverlay overlay;
        if (!TryReadSelfPolicy(*selfIterator, overlay))
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::InvalidSelfPolicy,
                "Invalid script self policy",
                "Runtime script binding self policy must be existing or create.");
            diagnostic.bindingId = bindingId;
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
            continue;
        }

        if (HasStringField(*selfIterator, "entityKey"))
        {
            overlay.entityKey = ReadStringField(*selfIterator, "entityKey");
        }
        if (HasStringField(*selfIterator, "name"))
        {
            overlay.name = ReadStringField(*selfIterator, "name");
        }

        if (overlay.entityKey.empty())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::MissingSelfEntityKey,
                "Script self entity key missing",
                "Runtime script binding self policy must include entityKey.");
            diagnostic.bindingId = bindingId;
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
        }
        if (overlay.policy == ScriptBindingSelfPolicy::Create &&
            overlay.name.empty())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::MissingCreateName,
                "Script self create name missing",
                "Runtime script binding create self policy must include name.");
            diagnostic.bindingId = bindingId;
            diagnostic.metadata["bindingIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
        }

        overlays.emplace(bindingId, RuntimeOverlayEntry{std::move(overlay)});
    }

    return overlays;
}

void ValidateScriptCoverage(
    const std::vector<ScriptBindingDefinition>& bindings,
    const std::unordered_set<std::string>& artifactScripts,
    const std::unordered_set<std::string>& capabilityScripts,
    ScriptBindingManifestLoadResult& result)
{
    for (const auto& binding : bindings)
    {
        if (binding.scriptId.empty())
        {
            continue;
        }

        if (artifactScripts.find(binding.scriptId) == artifactScripts.end())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::MissingArtifactScript,
                "Script artifact entry missing",
                "Generated script binding references a scriptId absent from script_artifacts.json.");
            diagnostic.bindingId = binding.bindingId;
            diagnostic.scriptId = binding.scriptId;
            result.diagnostics.push_back(std::move(diagnostic));
        }

        if (capabilityScripts.find(binding.scriptId) == capabilityScripts.end())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::MissingCapabilityScript,
                "Script capability entry missing",
                "Generated script binding references a scriptId absent from script_capabilities.json.");
            diagnostic.bindingId = binding.bindingId;
            diagnostic.scriptId = binding.scriptId;
            result.diagnostics.push_back(std::move(diagnostic));
        }
    }
}

void JoinRuntimeOverlays(
    const std::vector<ScriptBindingDefinition>& bindings,
    const std::unordered_map<std::string, RuntimeOverlayEntry>& overlays,
    ScriptBindingManifestLoadResult& result)
{
    for (const auto& binding : bindings)
    {
        if (binding.bindingId.empty())
        {
            continue;
        }

        const auto overlay = overlays.find(binding.bindingId);
        if (overlay == overlays.end())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::MissingRuntimeBinding,
                "Script runtime binding missing",
                "Generated script binding has no matching runtime self overlay.");
            diagnostic.bindingId = binding.bindingId;
            diagnostic.scriptId = binding.scriptId;
            result.diagnostics.push_back(std::move(diagnostic));
            continue;
        }

        result.bindings.push_back(ScriptBindingRuntimeEntry{
            binding,
            overlay->second.self,
        });
    }
}

} // namespace

ScriptBindingManifestLoadResult ScriptBindingManifestLoader::Load(
    const ScriptBindingManifestLoadRequest& request)
{
    ScriptBindingManifestLoadResult result;
    const auto packageRoot =
        std::filesystem::absolute(request.packageRoot).lexically_normal();

    const auto bindingsPath = ResolvePath(packageRoot, request.bindingManifest);
    const auto artifactsPath = ResolvePath(packageRoot, request.artifactManifest);
    const auto capabilitiesPath =
        ResolvePath(packageRoot, request.capabilityManifest);
    const auto runtimePath =
        ResolvePath(packageRoot, request.runtimeBindingManifest);

    const auto generated = LoadJson(bindingsPath, "script_bindings.json", result);
    const auto artifacts = LoadJson(artifactsPath, "script_artifacts.json", result);
    const auto capabilities =
        LoadJson(capabilitiesPath, "script_capabilities.json", result);
    const auto runtime =
        LoadJson(runtimePath, "script_runtime_bindings.json", result);
    if (!generated.has_value() || !artifacts.has_value() ||
        !capabilities.has_value() || !runtime.has_value())
    {
        return result;
    }

    const bool generatedValid =
        ValidateArrayManifest(*generated, "script_bindings.json", "bindings", result);
    const bool artifactsValid =
        ValidateArrayManifest(*artifacts, "script_artifacts.json", "artifacts", result);
    const bool capabilitiesValid =
        ValidateArrayManifest(*capabilities, "script_capabilities.json", "scripts", result);
    const bool runtimeValid = ValidateArrayManifest(
        *runtime,
        "script_runtime_bindings.json",
        "bindings",
        result);
    if (!generatedValid || !artifactsValid || !capabilitiesValid ||
        !runtimeValid)
    {
        return result;
    }

    const auto generatedBindings = ReadGeneratedBindings(*generated, result);
    const auto artifactScripts = ReadScriptIdSet(*artifacts, "artifacts");
    const auto capabilityScripts = ReadScriptIdSet(*capabilities, "scripts");
    const auto runtimeOverlays = ReadRuntimeOverlays(*runtime, result);

    ValidateScriptCoverage(
        generatedBindings,
        artifactScripts,
        capabilityScripts,
        result);
    JoinRuntimeOverlays(generatedBindings, runtimeOverlays, result);

    std::sort(
        result.bindings.begin(),
        result.bindings.end(),
        [](const ScriptBindingRuntimeEntry& left,
           const ScriptBindingRuntimeEntry& right)
        {
            return left.binding.bindingId < right.binding.bindingId;
        });

    result.succeeded = result.diagnostics.empty();
    if (!result.succeeded)
    {
        result.bindings.clear();
    }
    return result;
}

} // namespace SagaEngine::Scripting
