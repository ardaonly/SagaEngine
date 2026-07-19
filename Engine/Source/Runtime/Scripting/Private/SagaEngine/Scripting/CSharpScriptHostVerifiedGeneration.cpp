// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostVerifiedGeneration.cpp

#include <algorithm>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>

#include "CSharpScriptHostImpl.hpp"

namespace SagaEngine::Scripting
{

[[nodiscard]] ScriptPackageLoadResult CSharpScriptHost::Impl::LoadVerifiedGeneration(
    const VerifiedScriptGenerationLoadRequest& request)
{
    ScriptPackageLoadResult result;
    auto fail = [&](std::string code,
                    std::string message,
                    const std::filesystem::path& path = std::filesystem::path{}) {
        auto diagnostic = MakeDiagnostic(std::move(code),
                                         "Verified SagaScript generation rejected",
                                         std::move(message));
        if (!path.empty())
            diagnostic.metadata["path"] = path.generic_string();
        result.diagnostics.push_back(std::move(diagnostic));
    };
    const std::array<const VerifiedScriptFile*, 6> files{{&request.assembly,
                                                          &request.runtimeConfig,
                                                          &request.assemblyManifest,
                                                          &request.behaviorManifest,
                                                          &request.namedActionManifest,
                                                          &request.capabilityManifest}};
    for (const auto* file : files)
    {
        if (file->path.empty() || file->byteHash.empty() ||
            Sha256File(file->path) != file->byteHash)
        {
            fail(ScriptHostDiagnostics::ArtifactHashMismatch,
                 "A selected SagaScript artifact does not match its canonical byte hash.",
                 file->path);
            return result;
        }
    }
    if (options.runtimeBridgeAssembly.empty() ||
        Sha256File(options.runtimeBridgeAssembly) != request.runtimeBridgeHash)
    {
        fail(ScriptHostDiagnostics::ArtifactHashMismatch,
             "The loaded RuntimeBridge binary does not match the verified generation.",
             options.runtimeBridgeAssembly);
        return result;
    }
    auto readJson = [&](const VerifiedScriptFile& file, nlohmann::json& value) -> bool {
        std::ifstream input(file.path, std::ios::binary);
        try
        {
            if (input)
                input >> value;
            return value.is_object() && value.value("schemaVersion", -1) == 1;
        }
        catch (const nlohmann::json::exception&)
        {
            return false;
        }
    };
    nlohmann::json assembly, behaviors, actions, capabilities;
    if (!readJson(request.assemblyManifest, assembly) ||
        !readJson(request.behaviorManifest, behaviors) ||
        !readJson(request.namedActionManifest, actions) ||
        !readJson(request.capabilityManifest, capabilities))
    {
        fail(ScriptHostDiagnostics::ManifestInvalid,
             "Assembly, behavior, named-action, and capability manifests must use schema version "
             "1.");
        return result;
    }
    PackageState packageState;
    try
    {
        auto identityMatches = [&](const nlohmann::json& manifest) {
            return manifest.value("projectId", std::string{}) == request.projectId &&
                   manifest.value("projectHash", std::string{}) == request.projectHash &&
                   manifest.value("sourceRevision", std::string{}) == request.sourceRevision &&
                   manifest.value("sourceSetHash", std::string{}) == request.sourceSetHash;
        };
        if (!identityMatches(behaviors))
        {
            fail(ScriptHostDiagnostics::BehaviorManifestMismatch,
                 "Behavior manifest identity differs from the selected SagaScript generation.");
            return result;
        }
        if (!identityMatches(actions))
        {
            fail(ScriptHostDiagnostics::NamedActionManifestMismatch,
                 "Named-action manifest identity differs from the selected SagaScript generation.");
            return result;
        }
        if (!identityMatches(capabilities))
        {
            fail(ScriptHostDiagnostics::CapabilityManifestMismatch,
                 "Capability manifest identity differs from the selected SagaScript generation.");
            return result;
        }
        if (!identityMatches(assembly) ||
            assembly.value("assemblyHash", std::string{}) != request.assembly.byteHash ||
            assembly.value("runtimeConfigHash", std::string{}) != request.runtimeConfig.byteHash ||
            assembly.value("referenceManifestHash", std::string{}) !=
                request.referenceManifestHash ||
            capabilities.value("packageGraphHash", std::string{}) != request.packageGraphHash)
        {
            fail(ScriptHostDiagnostics::ReferenceIdentityMismatch,
                 "Assembly, source, reference, or package graph identity is inconsistent.");
            return result;
        }
        if (assembly.value("runtimeBridgeLogicalReferenceId", std::string{}) !=
                request.runtimeBridgeLogicalReferenceId ||
            assembly.value("runtimeBridgeHash", std::string{}) != request.runtimeBridgeHash ||
            assembly.value("targetFramework", std::string{}) != request.targetFramework ||
            assembly.value("runtimeCompatibility", std::string{}) != request.runtimeCompatibility ||
            assembly.value("trust", std::string{}) != request.trust ||
            request.runtimeBridgeAbi != "NativeCallbackTable.v2+NamedAction.v1")
        {
            fail(ScriptHostDiagnostics::RuntimeBridgeMismatch,
                 "RuntimeBridge identity, ABI, framework, runtime, or trust is inconsistent.");
            return result;
        }
        if (!behaviors.contains("behaviors") || !behaviors["behaviors"].is_array() ||
            !actions.contains("actions") || !actions["actions"].is_array() ||
            !capabilities.contains("capabilities") || !capabilities["capabilities"].is_array())
        {
            fail(ScriptHostDiagnostics::ManifestInvalid,
                 "Verified SagaScript behavior/action/capability arrays are required.");
            return result;
        }
        CapabilitySet globalCapabilities;
        for (const auto& capability : capabilities["capabilities"])
            if (!capability.is_string() || capability.get_ref<const std::string&>().empty() ||
                !globalCapabilities.insert(capability.get<std::string>()).second)
            {
                fail(ScriptHostDiagnostics::InvalidCapabilityMetadata,
                     "Capability identities must be non-empty and unique.");
                return result;
            }
        std::set<std::string> behaviorIds;
        std::map<std::pair<std::string, std::string>, std::string> behaviorActions;
        for (const auto& behavior : behaviors["behaviors"])
        {
            const auto behaviorId = behavior.value("behaviorId", std::string{});
            if (behaviorId.empty() || !behaviorIds.insert(behaviorId).second ||
                !behavior.contains("capabilities") || !behavior["capabilities"].is_array() ||
                !behavior.contains("actions") || !behavior["actions"].is_array())
            {
                fail(ScriptHostDiagnostics::ManifestInvalid,
                     "Behavior identities must be non-empty and unique.");
                return result;
            }
            auto& granted = packageState.grantedCapabilitiesByScriptId[behaviorId];
            for (const auto& capability : behavior["capabilities"])
            {
                if (!capability.is_string() ||
                    !globalCapabilities.contains(capability.get<std::string>()) ||
                    !granted.insert(capability.get<std::string>()).second)
                {
                    fail(ScriptHostDiagnostics::CapabilityGrantUnexpected,
                         "Behavior capabilities must be unique members of the verified grant set.");
                    return result;
                }
            }
            for (const auto& action : behavior["actions"])
            {
                const auto actionId = action.value("actionId", std::string{});
                const auto method = action.value("method", std::string{});
                if (actionId.empty() || method.empty() ||
                    !action.value("publiclyInvocable", false) ||
                    action.value("attributeTypeId", std::string{}) !=
                        "SagaEngine.Scripting.SagaActionAttribute" ||
                    !behaviorActions.emplace(std::make_pair(behaviorId, actionId), method).second)
                {
                    fail(ScriptHostDiagnostics::ManifestInvalid,
                         "Behavior actions must be unique, attributed, and publicly invocable.");
                    return result;
                }
            }
        }
        std::set<std::pair<std::string, std::string>> actionIds;
        for (const auto& action : actions["actions"])
        {
            const auto behaviorId = action.value("behaviorId", std::string{});
            const auto actionId = action.value("actionId", std::string{});
            const auto method = action.value("method", std::string{});
            const auto behaviorAction = behaviorActions.find({behaviorId, actionId});
            if (!behaviorIds.contains(behaviorId) || actionId.empty() ||
                !action.value("publiclyInvocable", false) || method.empty() ||
                action.value("attributeTypeId", std::string{}) !=
                    "SagaEngine.Scripting.SagaActionAttribute" ||
                behaviorAction == behaviorActions.end() || behaviorAction->second != method ||
                !actionIds.emplace(behaviorId, actionId).second)
            {
                fail(ScriptHostDiagnostics::ManifestInvalid,
                     "Named actions must be unique, public, and mapped to a verified behavior.");
                return result;
            }
        }
    }
    catch (const nlohmann::json::exception&)
    {
        fail(ScriptHostDiagnostics::ManifestInvalid,
             "Verified SagaScript manifest fields have invalid JSON types.");
        return result;
    }
    if (!ValidateRuntimeConfig(request.runtimeConfig.path, result.diagnostics) ||
        !EnsureRuntime(request.runtimeConfig.path, result.diagnostics))
        return result;
    std::array<char, kBridgeErrorBufferSize> error{};
    std::int64_t bridgePackageHandle = 0;
    const auto status = static_cast<BridgeStatus>(
        loadAssembly(request.assembly.path.string().c_str(),
                     error.data(),
                     static_cast<int>(error.size()),
                     &bridgePackageHandle));
    if (status != BridgeStatus::Ok)
    {
        fail(ScriptHostDiagnostics::AssemblyLoadFailed,
             "Verified SagaScript assembly failed managed loading.",
             request.assembly.path);
        return result;
    }
    packageState.managedPackages.push_back({bridgePackageHandle, request.assembly.path});
    const auto packageHandle = nextPackageHandle++;
    packages.emplace(packageHandle, std::move(packageState));
    result.succeeded = true;
    result.package.value = packageHandle;
    return result;
}

[[nodiscard]] ScriptPackageLoadResult CSharpScriptHost::Impl::LoadPackage(
    const ScriptPackageLoadRequest& request)
{
    ScriptPackageLoadResult result;
    const auto packageRoot = NormalizeRoot(request.packageRoot);
    const auto manifestPath = ResolvePackagePath(packageRoot, request.scriptArtifactManifest);
    const auto artifacts = LoadArtifactEntries(manifestPath, result);
    if (!artifacts.has_value())
    {
        return result;
    }

    if (artifacts->empty())
    {
        result.diagnostics.push_back(
            MakeDiagnostic(ScriptHostDiagnostics::ManifestInvalid,
                           "Invalid script artifact manifest",
                           "Script artifact manifest contains no loadable artifacts."));
        result.diagnostics.back().metadata["manifestPath"] = manifestPath.generic_string();
        return result;
    }

    const auto runtimeConfigPath = ResolvePackagePath(packageRoot,
                                                      artifacts->front().runtimeConfigPath);
    if (!std::filesystem::exists(runtimeConfigPath))
    {
        result.diagnostics.push_back(
            MakeDiagnostic(ScriptHostDiagnostics::RuntimeConfigMissing,
                           "Script runtimeconfig missing",
                           "C# script host could not open the runtimeconfig file."));
        result.diagnostics.back().metadata["runtimeConfigPath"] =
            runtimeConfigPath.generic_string();
        return result;
    }

    if (!ValidateRuntimeConfig(runtimeConfigPath, result.diagnostics))
    {
        return result;
    }

    if (!EnsureRuntime(runtimeConfigPath, result.diagnostics))
    {
        return result;
    }

    PackageState packageState;
    const auto releaseStagedPackages = [&] {
        if (releasePackage == nullptr)
        {
            return;
        }
        for (const auto& managedPackage : packageState.managedPackages)
        {
            static_cast<void>(releasePackage(managedPackage.bridgeHandle));
        }
        packageState.managedPackages.clear();
    };
    for (const auto& artifact : *artifacts)
    {
        if (artifact.scriptId.empty())
        {
            continue;
        }

        auto& granted = packageState.grantedCapabilitiesByScriptId[artifact.scriptId];
        granted.insert(artifact.grantedCapabilities.begin(), artifact.grantedCapabilities.end());
    }

    std::vector<std::filesystem::path> loadedAssemblies;
    for (const auto& artifact : *artifacts)
    {
        const auto assemblyPath = ResolvePackagePath(packageRoot, artifact.assemblyPath);
        if (std::find(loadedAssemblies.begin(), loadedAssemblies.end(), assemblyPath) !=
            loadedAssemblies.end())
        {
            continue;
        }

        if (!std::filesystem::exists(assemblyPath))
        {
            result.diagnostics.push_back(
                MakeDiagnostic(ScriptHostDiagnostics::AssemblyLoadFailed,
                               "Managed script assembly missing",
                               "C# script host could not open the script assembly."));
            result.diagnostics.back().metadata["assemblyPath"] = assemblyPath.generic_string();
            releaseStagedPackages();
            return result;
        }

        std::array<char, kBridgeErrorBufferSize> error{};
        std::int64_t bridgePackageHandle = 0;
        const auto status = static_cast<BridgeStatus>(loadAssembly(assemblyPath.string().c_str(),
                                                                   error.data(),
                                                                   static_cast<int>(error.size()),
                                                                   &bridgePackageHandle));
        if (status != BridgeStatus::Ok)
        {
            auto diagnostic = MakeDiagnostic(ScriptHostDiagnostics::AssemblyLoadFailed,
                                             "Managed script assembly load failed",
                                             "C# script host failed to load a script assembly.");
            diagnostic.metadata["assemblyPath"] = assemblyPath.generic_string();
            diagnostic.metadata["bridgeError"] = BridgeError(error);
            result.diagnostics.push_back(std::move(diagnostic));
            releaseStagedPackages();
            return result;
        }

        loadedAssemblies.push_back(assemblyPath);
        packageState.managedPackages.push_back(ManagedPackage{
            bridgePackageHandle,
            assemblyPath,
        });
    }

    const auto packageHandle = nextPackageHandle++;
    packages.emplace(packageHandle, std::move(packageState));
    result.succeeded = true;
    result.package.value = packageHandle;
    return result;
}

[[nodiscard]] std::optional<std::vector<CSharpScriptHost::Impl::ArtifactEntry>>
CSharpScriptHost::Impl::LoadArtifactEntries(const std::filesystem::path& manifestPath,
                                            ScriptPackageLoadResult& result)
{
    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        result.diagnostics.push_back(
            MakeDiagnostic(ScriptHostDiagnostics::ManifestMissing,
                           "Script artifact manifest missing",
                           "C# script host could not open the script artifact manifest."));
        result.diagnostics.back().metadata["manifestPath"] = manifestPath.generic_string();
        return std::nullopt;
    }

    nlohmann::json root;
    try
    {
        input >> root;
    }
    catch (const nlohmann::json::exception& exception)
    {
        result.diagnostics.push_back(MakeDiagnostic(
            ScriptHostDiagnostics::ManifestInvalid,
            "Invalid script artifact manifest",
            std::string("C# script host could not parse the manifest: ") + exception.what()));
        result.diagnostics.back().metadata["manifestPath"] = manifestPath.generic_string();
        return std::nullopt;
    }

    if (!root.is_object() || !root.contains("artifacts") || !root["artifacts"].is_array())
    {
        result.diagnostics.push_back(MakeDiagnostic(ScriptHostDiagnostics::ManifestInvalid,
                                                    "Invalid script artifact manifest",
                                                    "C# script host requires an artifacts array."));
        result.diagnostics.back().metadata["manifestPath"] = manifestPath.generic_string();
        return std::nullopt;
    }

    std::vector<ArtifactEntry> artifacts;
    for (const auto& artifact : root["artifacts"])
    {
        if (!artifact.is_object() || !HasStringField(artifact, "assemblyPath") ||
            !HasStringField(artifact, "runtimeConfigPath"))
        {
            continue;
        }

        artifacts.push_back(ArtifactEntry{
            ReadStringField(artifact, "assemblyPath"),
            ReadStringField(artifact, "runtimeConfigPath"),
            HasStringField(artifact, "scriptId") ? ReadStringField(artifact, "scriptId")
                                                 : std::string{},
            ReadStringArrayField(artifact, "grantedCapabilities"),
        });
    }

    return artifacts;
}

} // namespace SagaEngine::Scripting
