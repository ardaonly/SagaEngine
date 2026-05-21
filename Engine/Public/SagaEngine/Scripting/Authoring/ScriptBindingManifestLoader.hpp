/// @file ScriptBindingManifestLoader.hpp
/// @brief SagaScript binding manifest authoring workflow helpers.

#pragma once

#include "SagaEngine/Scripting/LowLevel/ISagaScriptHost.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEngine::Scripting
{

namespace ScriptBindingDiagnostics
{

inline constexpr const char* ManifestMissing = "Script.Binding.ManifestMissing";
inline constexpr const char* ManifestInvalid = "Script.Binding.ManifestInvalid";
inline constexpr const char* MissingBindingId = "Script.Binding.MissingBindingId";
inline constexpr const char* DuplicateBindingId = "Script.Binding.DuplicateBindingId";
inline constexpr const char* MissingScriptId = "Script.Binding.MissingScriptId";
inline constexpr const char* MissingDeclaringType =
    "Script.Binding.MissingDeclaringType";
inline constexpr const char* MissingArtifactScript =
    "Script.Binding.MissingArtifactScript";
inline constexpr const char* MissingCapabilityScript =
    "Script.Binding.MissingCapabilityScript";
inline constexpr const char* MissingRuntimeBinding =
    "Script.Binding.MissingRuntimeBinding";
inline constexpr const char* InvalidSelfPolicy =
    "Script.Binding.InvalidSelfPolicy";
inline constexpr const char* MissingSelfEntityKey =
    "Script.Binding.MissingSelfEntityKey";
inline constexpr const char* MissingCreateName =
    "Script.Binding.MissingCreateName";
inline constexpr const char* MissingExistingEntityKey =
    "Script.Binding.MissingExistingEntityKey";
inline constexpr const char* DuplicateCreateEntityKey =
    "Script.Binding.DuplicateCreateEntityKey";
inline constexpr const char* CreateSelfFailed =
    "Script.Binding.CreateSelfFailed";

} // namespace ScriptBindingDiagnostics

enum class ScriptBindingSelfPolicy
{
    Existing,
    Create,
};

struct ScriptBindingSelfOverlay
{
    ScriptBindingSelfPolicy policy = ScriptBindingSelfPolicy::Existing;
    std::string entityKey;
    std::string name;
};

struct ScriptBindingDefinition
{
    std::string bindingId;
    std::string scriptId;
    std::string declaringType;
};

struct ScriptBindingRuntimeEntry
{
    ScriptBindingDefinition binding;
    ScriptBindingSelfOverlay self;
};

struct ScriptBindingManifestLoadRequest
{
    std::filesystem::path packageRoot;
    std::filesystem::path bindingManifest =
        "Build/Manifests/script_bindings.json";
    std::filesystem::path artifactManifest =
        "Build/Manifests/script_artifacts.json";
    std::filesystem::path capabilityManifest =
        "Build/Manifests/script_capabilities.json";
    std::filesystem::path runtimeBindingManifest =
        "Build/Manifests/script_runtime_bindings.json";
};

struct ScriptBindingManifestLoadResult
{
    bool succeeded = false;
    std::vector<ScriptDiagnostic> diagnostics;
    std::vector<ScriptBindingRuntimeEntry> bindings;

    [[nodiscard]] bool Succeeded() const noexcept { return succeeded; }
};

class ScriptBindingManifestLoader
{
public:
    [[nodiscard]] static ScriptBindingManifestLoadResult Load(
        const ScriptBindingManifestLoadRequest& request);
};

} // namespace SagaEngine::Scripting
