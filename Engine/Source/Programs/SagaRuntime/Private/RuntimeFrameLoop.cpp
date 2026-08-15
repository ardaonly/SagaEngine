// SPDX-License-Identifier: MPL-2.0

#include "RuntimeFrameLoop.h"
#include "RuntimeGameplaySession.h"
#include "RuntimeGameplayResultWriter.h"
#include "RuntimeArtifactSession.h"
#include "RuntimeEvidenceSession.h"
#include "RuntimeLaunchSession.h"
#include "RuntimeRenderSession.h"
#include "RuntimeScriptSession.h"
#include "RuntimeStateStore.h"
#include "RuntimeTriggerSession.h"
#include "RuntimeUiPromptController.h"
#include "RuntimeUiSession.h"

#include <SagaEngine/Scripting/BehaviorBackend.hpp>
#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <SagaEngine/Scripting/ScriptLifecycleService.hpp>
#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <thread>
#include <variant>

namespace SagaRuntimeApp
{
namespace
{
using Json = nlohmann::json;
namespace Play = SagaRuntime::Play;
namespace Scripting = SagaEngine::Scripting;
RuntimeHostResult Failure(std::string code, std::string message)
{
    RuntimeHostResult result; result.exitCode = 1;
    result.diagnosticId = std::move(code); result.message = std::move(message);
    return result;
}
} // namespace
RuntimeHostResult RunRuntimeFrameLoop(const std::filesystem::path& playEnvelope, bool headless,
                                      bool smokeInteractions,
                                      bool legacyGenericMode)
{
    if (legacyGenericMode)
    {
        auto result = Failure(
            RuntimeHostDiagnostics::GenericProjectModeUnsupported,
            "Generic project execution is unsupported; use a verified Play envelope.");
        result.exitCode = 2;
        return result;
    }
    if (playEnvelope.empty())
        return Failure(RuntimeHostDiagnostics::PlayEnvelopeRequired,
            "SagaRuntime requires --play-envelope for canonical project Play.");
    auto envelopeResult = RuntimeLaunchSession::ReadEnvelope(playEnvelope);
    auto& envelope = envelopeResult.envelope;
    std::string canonicalPackageGenerationId, canonicalPackageGenerationHash;
    std::string canonicalPackageGraphHash, canonicalCollectibleImplementation;
    std::string canonicalScenarioIdentity;
    auto writeResult = [&](std::string status, std::string classification,
        std::uint64_t frames, std::uint64_t actions, const Json& observed)
    {
        if (envelope.resultPath.empty()) return;
        static_cast<void>(WriteRuntimeGameplayResult({envelope.resultPath, envelope.logPath,
            envelope.playSessionId, envelope.canonicalManifestHash,
            canonicalScenarioIdentity, canonicalPackageGenerationId,
            canonicalPackageGenerationHash, canonicalPackageGraphHash,
            canonicalCollectibleImplementation}, envelopeResult.diagnostics,
            status, classification, frames, actions, observed));
    };
    auto structuredFailure = [&](std::string code, std::string message,
        std::string classification)
    {
        envelopeResult.diagnostics.push_back({code, message, {}});
        writeResult("Failed", std::move(classification), 0, 0, Json::object());
        return Failure(std::move(code), std::move(message));
    };
    if (!envelopeResult.diagnostics.empty())
    {
        writeResult("Failed", "LaunchValidationFailure", 0, 0, Json::object());
        return Failure(envelopeResult.diagnostics.front().code,
            envelopeResult.diagnostics.front().message);
    }

    const auto manifestResult = Play::ParseCanonicalManifest(
        RuntimeLaunchSession::ReadFile(envelope.canonicalManifestPath), envelope.canonicalManifestHash);
    if (!manifestResult.Succeeded() ||
        manifestResult.manifest->playSessionId != envelope.playSessionId)
    {
        envelopeResult.diagnostics.insert(envelopeResult.diagnostics.end(),
            manifestResult.diagnostics.begin(), manifestResult.diagnostics.end());
        writeResult("Failed", "LaunchValidationFailure", 0, 0, Json::object());
        return Failure("Runtime.Launch.ManifestInvalid",
            "Canonical runtime launch manifest validation failed.");
    }
    const auto& launch = *manifestResult.manifest;
    canonicalScenarioIdentity = launch.projectId;
    canonicalPackageGenerationId = launch.packageGeneration.logicalId;
    canonicalPackageGenerationHash = launch.packageGeneration.hash;
    canonicalPackageGraphHash = launch.packageGraphHash;
    if (launch.runtimeBridge.abi != "NativeCallbackTable.v3+NamedAction.v1" ||
        launch.runtimeBridge.targetFramework != "net10.0")
    {
        envelopeResult.diagnostics.push_back({"Runtime.Bridge.AbiMismatch",
            "RuntimeBridge ABI or target framework is incompatible with this SagaRuntime.",
            launch.runtimeBridge.logicalReferenceId});
    }
    std::vector<Play::ArtifactIdentity> launchArtifacts{
        launch.scene, launch.input, launch.scriptAssembly, launch.uiSet,
        launch.scriptRuntimeConfig, launch.scriptAssemblyManifest,
        launch.scriptReferenceManifest, launch.behaviors,
        launch.namedActions, launch.capabilities};
    if (launch.schemaVersion >= 3)
    {
        launchArtifacts.push_back(launch.behaviorModules);
        launchArtifacts.push_back(launch.behaviorArtifacts);
    }
    if (launch.schemaVersion == 4) launchArtifacts.push_back(launch.apiOperations);
    static_cast<void>(RuntimeArtifactSession::VerifyFiles(
        envelope.artifacts, launchArtifacts, envelopeResult.diagnostics));
    const Play::ArtifactIdentity bridge{launch.runtimeBridge.logicalReferenceId,
                                        launch.runtimeBridge.assemblyHash};
    static_cast<void>(RuntimeArtifactSession::VerifyFiles(
        envelope.artifacts, {bridge}, envelopeResult.diagnostics));
    const auto packageCurrentPath = envelope.artifacts.find("packages.current");
    const auto packageManifestPath = envelope.artifacts.find("packages.generation-manifest");
    const auto generationLockPath = envelope.artifacts.find("packages.generation-lock");
    const auto trackedLockPath = envelope.artifacts.find("packages.tracked-lock");
    Json packageCurrent;
    Json packageManifest;
    try
    {
        if (packageCurrentPath == envelope.artifacts.end() ||
            packageManifestPath == envelope.artifacts.end() ||
            generationLockPath == envelope.artifacts.end() ||
            trackedLockPath == envelope.artifacts.end())
            throw std::runtime_error("package generation paths are incomplete");
        packageCurrent = Json::parse(RuntimeLaunchSession::ReadFile(packageCurrentPath->second));
        packageManifest = Json::parse(RuntimeLaunchSession::ReadFile(packageManifestPath->second));
        const auto packageBytes = packageManifest.dump();
        canonicalCollectibleImplementation =
            packageBytes.find("collectibles.lowlevel") != std::string::npos
                ? "com.saga.gameplay.challenge/collectibles.lowlevel"
                : "com.saga.gameplay.challenge/collectibles.highlevel";
        if (packageCurrent.value("schemaVersion", -1) != 1 ||
            packageCurrent.value("generationId", std::string{}) !=
                launch.packageGeneration.logicalId ||
            packageCurrent.value("generationHash", std::string{}) !=
                launch.packageGeneration.hash ||
            packageCurrent.value("graphHash", std::string{}) != launch.packageGraphHash ||
            packageCurrent.value("projectHash", std::string{}) != launch.projectHash ||
            packageManifest.value("generationId", std::string{}) !=
                launch.packageGeneration.logicalId ||
            packageManifest.value("generationHash", std::string{}) !=
                launch.packageGeneration.hash ||
            Play::Sha256Bytes(RuntimeLaunchSession::ReadFile(packageManifestPath->second)) !=
                packageCurrent.value("generationManifestHash", std::string{}) ||
            RuntimeLaunchSession::ReadFile(generationLockPath->second) !=
                RuntimeLaunchSession::ReadFile(trackedLockPath->second))
            throw std::runtime_error("package generation identity or tracked lock mismatch");
    }
    catch (const std::exception& error)
    {
        envelopeResult.diagnostics.push_back({"Runtime.Package.GenerationMismatch",
            error.what(), "packages.current"});
    }
    if (!envelopeResult.diagnostics.empty())
    {
        writeResult("Failed", "DependencyFailure", 0, 0, Json::object());
        return Failure(envelopeResult.diagnostics.front().code,
            envelopeResult.diagnostics.front().message);
    }

    const auto sceneResult = Play::LoadRuntimeSceneArtifact(
        envelope.artifacts.at(launch.scene.logicalId));
    const auto inputResult = Play::LoadRuntimeInputMapArtifact(
        envelope.artifacts.at(launch.input.logicalId));
    const auto uiResult = launch.schemaVersion >= 2
        ? Play::LoadRuntimeUiSetArtifact(envelope.artifacts.at(launch.uiSet.logicalId))
        : Play::ArtifactLoadResult<Play::RuntimeUiSetArtifact>{};
    if (!sceneResult.Succeeded() || !inputResult.Succeeded() ||
        (launch.schemaVersion >= 2 && !uiResult.Succeeded()))
    {
        writeResult("Failed", "ArtifactFailure", 0, 0, Json::object());
        return Failure("Runtime.Play.ArtifactLoadFailed",
            "Runtime scene, input, or UI artifact failed strict loading.");
    }
    Json namedActions, behaviorManifest, capabilityManifest;
    Json behaviorModuleCatalog, behaviorArtifactCatalog;
    try
    {
        namedActions = Json::parse(RuntimeLaunchSession::ReadFile(
            envelope.artifacts.at(launch.namedActions.logicalId)));
        behaviorManifest = Json::parse(RuntimeLaunchSession::ReadFile(
            envelope.artifacts.at(launch.behaviors.logicalId)));
        capabilityManifest = Json::parse(RuntimeLaunchSession::ReadFile(
            envelope.artifacts.at(launch.capabilities.logicalId)));
        if (launch.schemaVersion >= 3)
        {
            behaviorModuleCatalog = Json::parse(RuntimeLaunchSession::ReadFile(
                envelope.artifacts.at(launch.behaviorModules.logicalId)));
            behaviorArtifactCatalog = Json::parse(RuntimeLaunchSession::ReadFile(
                envelope.artifacts.at(launch.behaviorArtifacts.logicalId)));
        }
    }
    catch (const Json::exception&)
    {
        return structuredFailure("Runtime.Script.NamedActionsInvalid",
            "Named-action manifest is malformed.", "ScriptLoadFailure");
    }
    const auto behaviorSchema = behaviorManifest.value("schemaVersion", -1);
    if ((behaviorSchema != 2 && behaviorSchema != 3 && behaviorSchema != 4) ||
        !behaviorManifest.contains("behaviors") ||
        !behaviorManifest["behaviors"].is_array())
        return structuredFailure("Runtime.Script.StateContractMissing",
            "Verified Play requires behavior manifest schema version 2, 3, or 4.",
            "ScriptLoadFailure");
    RuntimeStateStore::TypedWritePolicy stateWrites;
    try
    {
        for (const auto& behavior : behaviorManifest["behaviors"])
        {
            const auto behaviorId = behavior.value("behaviorId", std::string{});
            if (behaviorId.empty() || !behavior.contains("stateAccesses") ||
                !behavior["stateAccesses"].is_array())
                return structuredFailure("Runtime.Script.StateContractInvalid",
                    "Behavior state-access policy is missing or malformed.",
                    "ScriptLoadFailure");
            auto& keys = stateWrites[behaviorId];
            for (const auto& access : behavior["stateAccesses"])
            {
                if (!access.is_object() || access.size() != 3 ||
                    access.value("key", std::string{}).empty())
                    return structuredFailure("Runtime.Script.StateContractInvalid",
                        "Behavior state-access policy contains an unsupported or duplicate entry.",
                        "ScriptLoadFailure");
                const auto operation = access.value("operation", std::string{});
                const auto valueType = access.value("valueType", std::string{});
                const bool knownType = valueType == "Bool" || valueType == "Int32" ||
                    valueType == "Double" || valueType == "String";
                const bool knownOperation = operation == "GetBool" ||
                    operation == "GetInt32" || operation == "GetDouble" ||
                    operation == "GetString" || operation == "AddInt32" ||
                    operation == "SetBool" || operation == "SetInt32" ||
                    operation == "SetDouble" || operation == "SetString";
                if (!knownType || !knownOperation)
                    return structuredFailure("Runtime.Script.StateContractInvalid",
                        "Behavior state-access policy contains an unsupported operation.",
                        "ScriptLoadFailure");
                if (operation.starts_with("Set") || operation == "AddInt32")
                {
                    const auto [item, inserted] =
                        keys.emplace(access.value("key", std::string{}), valueType);
                    if (!inserted && item->second != valueType)
                        return structuredFailure("Runtime.Script.StateContractInvalid",
                            "One typed state key cannot have conflicting value types.",
                            "ScriptLoadFailure");
                }
            }
        }
    }
    catch (const Json::exception&)
    {
        return structuredFailure("Runtime.Script.StateContractInvalid",
            "Behavior state-access policy contains invalid field types.",
            "ScriptLoadFailure");
    }
    RuntimeScriptInputPort::Policy inputReads;
    std::set<std::string> authoredInputActions;
    for (const auto& action : inputResult.value->actions)
        authoredInputActions.insert(action.actionId);
    if (behaviorSchema == 3)
    {
        try
        {
            for (const auto& behavior : behaviorManifest["behaviors"])
            {
                const auto behaviorId = behavior.value("behaviorId", std::string{});
                if (!behavior.contains("inputAccesses") ||
                    !behavior["inputAccesses"].is_array())
                    return structuredFailure("Runtime.Script.InputContractInvalid",
                        "Behavior logical-input policy is missing or malformed.",
                        "ScriptLoadFailure");
                auto& actions = inputReads[behaviorId];
                for (const auto& access : behavior["inputAccesses"])
                {
                    const auto actionId = access.value("key", std::string{});
                    const auto operation = access.value("operation", std::string{});
                    if (!access.is_object() || access.size() != 3 ||
                        actionId.empty() ||
                        (operation != "Pressed" && operation != "Held" &&
                         operation != "Released" && operation != "GetValue") ||
                        !authoredInputActions.contains(actionId))
                        return structuredFailure("Runtime.Script.InputContractInvalid",
                            "Behavior input access is undeclared by the compiled input artifact.",
                            "ScriptLoadFailure");
                    actions.insert(actionId);
                }
            }
        }
        catch (const Json::exception&)
        {
            return structuredFailure("Runtime.Script.InputContractInvalid",
                "Behavior logical-input policy contains invalid field types.",
                "ScriptLoadFailure");
        }
    }
    RuntimeScriptWorld world(*sceneResult.value);
    RuntimeStateStore stateStore(stateWrites);
    RuntimeScriptInputPort inputPort(std::move(inputReads));
    RuntimeExitPort exitPort;
    Scripting::CSharpScriptHostOptions hostOptions;
    hostOptions.runtimeBridgeAssembly =
        envelope.artifacts.at(launch.runtimeBridge.logicalReferenceId);
    hostOptions.world = &world;
    hostOptions.statePort = &stateStore;
    hostOptions.grantStatePortAccess = true;
    hostOptions.inputPort = &inputPort;
    hostOptions.runtimePort = &exitPort;
    Scripting::CSharpScriptHost host(std::move(hostOptions));
    auto csharpBackend = std::make_shared<Scripting::CSharpBehaviorBackend>(host);
    Scripting::BehaviorBackendCatalog backendCatalog;
    const auto backendRegistration = backendCatalog.Register(csharpBackend);
    if (!backendRegistration.succeeded)
        return structuredFailure("Runtime.Script.BackendRegistrationFailed",
            backendRegistration.message, "ScriptLoadFailure");
    Scripting::ScriptLifecycleService lifecycle(host);
    Scripting::VerifiedScriptGenerationLoadRequest loadRequest{
        .assembly = {envelope.artifacts.at(launch.scriptAssembly.logicalId), launch.scriptAssembly.hash},
        .runtimeConfig = {envelope.artifacts.at(launch.scriptRuntimeConfig.logicalId), launch.scriptRuntimeConfig.hash},
        .assemblyManifest = {envelope.artifacts.at(launch.scriptAssemblyManifest.logicalId), launch.scriptAssemblyManifest.hash},
        .behaviorManifest = {envelope.artifacts.at(launch.behaviors.logicalId), launch.behaviors.hash},
        .namedActionManifest = {envelope.artifacts.at(launch.namedActions.logicalId), launch.namedActions.hash},
        .capabilityManifest = {envelope.artifacts.at(launch.capabilities.logicalId), launch.capabilities.hash},
        .operationCatalog = {envelope.artifacts.at(launch.apiOperations.logicalId), launch.apiOperations.hash},
        .projectId = launch.projectId, .projectHash = launch.projectHash,
        .sourceRevision = launch.scriptSourceRevision, .sourceSetHash = launch.scriptSourceSetHash,
        .referenceManifestHash = launch.scriptReferenceManifest.hash,
        .packageGraphHash = launch.packageGraphHash,
        .packageGenerationId = launch.packageGeneration.logicalId,
        .packageGenerationHash = launch.packageGeneration.hash,
        .runtimeBridgeLogicalReferenceId = launch.runtimeBridge.logicalReferenceId,
        .runtimeBridgeHash = launch.runtimeBridge.assemblyHash, .runtimeBridgeAbi = launch.runtimeBridge.abi,
        .targetFramework = launch.runtimeBridge.targetFramework,
        .runtimeCompatibility = launch.runtimeBridge.runtimeCompatibility,
        .trust = launch.runtimeBridge.trust};
    const auto describeHostFailure = [](std::string detail, const auto& result)
    {
        if (result.diagnostics.empty()) return detail;
        const auto& diagnostic = result.diagnostics.front();
        detail += " " + diagnostic.diagnostic.code.value + ": " +
            diagnostic.diagnostic.message;
        const auto bridgeError = diagnostic.metadata.find("bridgeError");
        if (bridgeError != diagnostic.metadata.end() &&
            !bridgeError->second.empty())
            detail += " (" + bridgeError->second + ")";
        for (const auto& [key, value] : diagnostic.metadata)
            if (key != "bridgeError" && !value.empty())
                detail += " [" + key + "=" + value + "]";
        return detail;
    };
    const auto csharpResolution = backendCatalog.Resolve(
        SagaShared::Behavior::CSharpBehaviorLanguage);
    if (!csharpResolution.succeeded || csharpResolution.backend != csharpBackend.get())
        return structuredFailure("Runtime.Script.BackendUnavailable",
            csharpResolution.message, "ScriptLoadFailure");
    const auto package = csharpBackend->LoadVerifiedGeneration(loadRequest);
    if (!package.Succeeded())
        return structuredFailure("Runtime.Script.PackageLoadFailed",
            describeHostFailure(
                "Verified script assembly could not be loaded.", package),
            "ScriptLoadFailure");

    std::map<std::pair<std::string, std::string>, std::string> methods;
    for (const auto& action : namedActions.value("actions", Json::array()))
        if (action.value("publiclyInvocable", false))
            methods.emplace(std::make_pair(action.value("behaviorId", std::string{}),
                action.value("actionId", std::string{})), action.value("method", std::string{}));

    std::map<std::string, std::set<std::string>> behaviorCapabilities;
    std::map<std::string, SagaShared::Behavior::BehaviorModuleDescriptor> behaviorModules;
    if (behaviorSchema == 4)
    {
        if (launch.schemaVersion < 3 || behaviorModuleCatalog.value("schemaVersion", -1) != 1 ||
            behaviorModuleCatalog.value("projectId", std::string{}) != launch.projectId ||
            behaviorModuleCatalog.value("packageGenerationId", std::string{}) !=
                launch.packageGeneration.logicalId ||
            !behaviorModuleCatalog.contains("modules") ||
            !behaviorModuleCatalog["modules"].is_array() ||
            behaviorArtifactCatalog.value("schemaVersion", -1) != 1 ||
            behaviorArtifactCatalog.value("projectId", std::string{}) != launch.projectId ||
            behaviorArtifactCatalog.value("packageGenerationId", std::string{}) !=
                launch.packageGeneration.logicalId ||
            !behaviorArtifactCatalog.contains("artifacts") ||
            !behaviorArtifactCatalog["artifacts"].is_array() ||
            behaviorArtifactCatalog["artifacts"].size() != 1)
            return structuredFailure("Runtime.Script.GenericCatalogInvalid",
                "Verified behavior module and artifact catalogs are missing or inconsistent.",
                "ScriptLoadFailure");
        for (const auto& item : behaviorModuleCatalog["modules"])
        {
            if (!item.is_object() || !item.contains("sourceIdentity") ||
                !item["sourceIdentity"].is_object() || !item.contains("ownership") ||
                !item["ownership"].is_object())
                return structuredFailure("Runtime.Script.ModuleCatalogInvalid",
                    "Behavior module descriptor shape is invalid.", "ScriptLoadFailure");
            const auto source = item.value("sourceIdentity", Json::object());
            const auto ownership = item.value("ownership", Json::object());
            SagaShared::Behavior::BehaviorModuleDescriptor descriptor;
            descriptor.moduleId.value = item.value("behaviorModuleId", std::string{});
            descriptor.source.language.value = item.value("language", std::string{});
            descriptor.source.sourceSymbolId = source.value("sourceSymbolId", std::string{});
            descriptor.source.sourcePath = source.value("sourcePath", std::string{});
            descriptor.projectId = item.value("projectId", std::string{});
            descriptor.packageGenerationId =
                item.value("packageGenerationId", std::string{});
            for (const auto& capability : item.value("requestedCapabilities", Json::array()))
                if (capability.is_string())
                    descriptor.requestedCapabilities.push_back(capability.get<std::string>());
            for (const auto& path : item.value("sourceInventory", Json::array()))
                if (path.is_string()) descriptor.sourceInventory.push_back(path.get<std::string>());
            const auto backend = backendCatalog.Resolve(descriptor.source.language);
            if (!descriptor.moduleId.IsValid() || descriptor.projectId != launch.projectId ||
                descriptor.packageGenerationId != launch.packageGeneration.logicalId ||
                ownership.value("kind", std::string{}) != "project" ||
                ownership.value("ownerId", std::string{}) != launch.projectId ||
                descriptor.source.sourceSymbolId.empty() || descriptor.source.sourcePath.empty() ||
                source.value("identityKind", std::string{}) != "CanonicalModuleMetadata" ||
                descriptor.sourceInventory.empty() || !backend.succeeded ||
                !behaviorModules.emplace(descriptor.moduleId.value, std::move(descriptor)).second)
                return structuredFailure("Runtime.Script.ModuleCatalogInvalid",
                    "Behavior module identity, source owner, language, or uniqueness is invalid.",
                    "ScriptLoadFailure");
        }
        const auto& artifact = behaviorArtifactCatalog["artifacts"].front();
        if (!artifact.is_object())
            return structuredFailure("Runtime.Script.ArtifactCatalogInvalid",
                "Behavior artifact descriptor shape is invalid.", "ScriptLoadFailure");
        SagaShared::Behavior::BehaviorArtifactDescriptor artifactDescriptor;
        artifactDescriptor.artifactId = artifact.value("artifactId", std::string{});
        artifactDescriptor.language.value = artifact.value("language", std::string{});
        artifactDescriptor.format = artifact.value("format", std::string{});
        artifactDescriptor.artifactHash = artifact.value("artifactHash", std::string{});
        artifactDescriptor.packageGenerationId =
            artifact.value("packageGenerationId", std::string{});
        std::set<std::string> artifactModules;
        for (const auto& id : artifact.value("behaviorModuleIds", Json::array()))
            if (id.is_string())
            {
                artifactModules.insert(id.get<std::string>());
                artifactDescriptor.behaviorModuleIds.push_back(id.get<std::string>());
            }
        std::set<std::string> catalogModules;
        for (const auto& [id, descriptor] : behaviorModules)
            catalogModules.insert(id);
        for (const auto& payload : artifact.value("payloadArtifacts", Json::array()))
            if (payload.is_object())
                artifactDescriptor.payloadArtifacts.emplace(
                    payload.value("artifactId", std::string{}),
                    payload.value("hash", std::string{}));
        const auto artifactBackend = backendCatalog.Resolve(artifactDescriptor.language);
        if (!artifactBackend.succeeded ||
            artifactDescriptor.artifactHash != launch.scriptAssembly.hash ||
            artifactDescriptor.packageGenerationId !=
                launch.packageGeneration.logicalId || artifactModules != catalogModules ||
            !artifactBackend.backend->ValidateArtifact(artifactDescriptor).succeeded)
            return structuredFailure("Runtime.Script.ArtifactCatalogInvalid",
                "Behavior artifact descriptor does not match its verified backend payload.",
                "ScriptLoadFailure");
    }
    std::set<std::string> grantedCapabilities;
    for (const auto& capability : capabilityManifest.value("capabilities", Json::array()))
        if (capability.is_string()) grantedCapabilities.insert(capability.get<std::string>());
    for (const auto& behavior : behaviorManifest.value("behaviors", Json::array()))
    {
        const auto behaviorId = behavior.value("behaviorId", std::string{});
        const auto language = behaviorSchema == 4
            ? behavior.value("language", std::string{}) : std::string{"csharp"};
        const auto backend = backendCatalog.Resolve({language});
        if (!backend.succeeded)
            return structuredFailure("Runtime.Script.BackendUnavailable", backend.message,
                "ScriptLoadFailure");
        auto& grants = behaviorCapabilities[behaviorId];
        for (const auto& capability : behavior.value("capabilities", Json::array()))
            if (capability.is_string()) grants.insert(capability.get<std::string>());
        const auto moduleId = behavior.value("behaviorModuleId", std::string{});
        if (!moduleId.empty())
        {
            const auto descriptor = behaviorModules.find(moduleId);
            if (descriptor == behaviorModules.end() || behaviorId != moduleId ||
                descriptor->second.source.language.value != language ||
                descriptor->second.source.sourceSymbolId !=
                    behavior.value("sourceSymbolId", std::string{}))
                return structuredFailure("Runtime.Script.ModuleCatalogInvalid",
                    "Behavior manifest does not match the verified module catalog.",
                    "ScriptLoadFailure");
            const std::set<std::string> descriptorCapabilities(
                descriptor->second.requestedCapabilities.begin(),
                descriptor->second.requestedCapabilities.end());
            if (descriptorCapabilities != grants)
                return structuredFailure("Runtime.Script.ModuleCatalogInvalid",
                    "Behavior capabilities do not match the verified module catalog.",
                    "ScriptLoadFailure");
        }
    }

    std::size_t behaviorCount = 0;
    for (const auto& entity : world.Scene().entities)
    {
        if (!entity.scriptBehavior || !entity.scriptBehavior->enabled) continue;
        ++behaviorCount;
        const auto grants = behaviorCapabilities.find(entity.scriptBehavior->behaviorModuleId);
        if (grants == behaviorCapabilities.end())
            return structuredFailure("Runtime.Script.BehaviorManifestMismatch",
                "Scene behavior is absent from the verified behavior manifest.", "ScriptLoadFailure");
        for (const auto& capability : entity.scriptBehavior->requiredCapabilities)
            if (!grantedCapabilities.contains(capability) || !grants->second.contains(capability))
                return structuredFailure("Runtime.Script.CapabilityNotGranted",
                    "Scene behavior requests a capability absent from its verified grant.",
                    "ScriptLoadFailure");
    }
    if (behaviorCount == 0)
        return structuredFailure("Runtime.Script.BehaviorMissing",
            "Runtime scene has no script behavior.", "ScriptLoadFailure");
    if (launch.schemaVersion >= 2)
    {
        if (uiResult.value->schemaVersion == 1)
        {
            const auto& prompt = uiResult.value->prompts.front();
            const auto allowed = stateWrites.find(prompt.behaviorId);
            if (allowed == stateWrites.end() ||
                !allowed->second.contains(prompt.stateKey) ||
                allowed->second.at(prompt.stateKey) != "Bool")
                return structuredFailure("Runtime.UI.StateContractMismatch",
                    "Verified UI prompt state is absent from the behavior state-access contract.",
                    "UiBootstrapFailure");
        }
        else
        {
            for (const auto& binding : uiResult.value->textBindings)
            {
                const auto allowed = stateWrites.find(binding.behaviorId);
                if (allowed == stateWrites.end() ||
                    !allowed->second.contains(binding.stateKey) ||
                    allowed->second.at(binding.stateKey) != binding.valueType)
                    return structuredFailure("Runtime.UI.StateContractMismatch",
                        "Typed UI text state is absent from the behavior contract.",
                        "UiBootstrapFailure");
            }
            for (const auto& document : uiResult.value->documents)
            {
                const auto allowed =
                    stateWrites.find(document.visibilityBehaviorId);
                if (allowed == stateWrites.end() ||
                    !allowed->second.contains(document.visibilityStateKey) ||
                    allowed->second.at(document.visibilityStateKey) !=
                        document.visibilityValueType)
                    return structuredFailure("Runtime.UI.StateContractMismatch",
                        "Typed UI visibility state is absent from the behavior contract.",
                        "UiBootstrapFailure");
            }
        }
    }

    std::map<std::pair<std::string, std::string>, Scripting::ScriptInstanceHandle> instances;
    for (const auto& entity : world.Scene().entities)
    {
        if (!entity.scriptBehavior || !entity.scriptBehavior->enabled) continue;
        const auto key = std::make_pair(entity.entityId, entity.scriptBehavior->behaviorModuleId);
        if (instances.contains(key))
            return structuredFailure("Runtime.Script.InstanceIdentityDuplicate",
                "Script instance identity must be unique by entity and behavior type.",
                "ScriptLifecycleFailure");
        const auto module = behaviorModules.find(entity.scriptBehavior->behaviorModuleId);
        if (module == behaviorModules.end() &&
            !entity.scriptBehavior->legacyCSharpTypeIdentity)
            return structuredFailure("Runtime.Script.ModuleCatalogMismatch",
                "Scene attachment has no canonical behavior module descriptor.",
                "ScriptLifecycleFailure");
        Scripting::ScriptInstanceCreateResult created;
        if (entity.scriptBehavior->legacyCSharpTypeIdentity)
        {
            Scripting::ScriptInstanceCreateRequest legacy;
            legacy.package = package.package;
            legacy.classId.value = entity.scriptBehavior->behaviorModuleId;
            legacy.scriptId = entity.scriptBehavior->behaviorModuleId;
            legacy.selfEntity = world.Handle(entity.entityId);
            legacy.instanceConfigurationCanonicalJson =
                entity.scriptBehavior->instanceConfigurationCanonicalJson;
            created = lifecycle.CreateInstance(legacy);
        }
        else
        {
            Scripting::CSharpBehaviorInstanceCreateRequest create;
            create.package = package.package;
            create.module = module->second;
            create.attachment.attachmentId = entity.scriptBehavior->attachmentId;
            create.attachment.behaviorModuleId.value = entity.scriptBehavior->behaviorModuleId;
            create.attachment.instanceConfigurationCanonicalJson =
                entity.scriptBehavior->instanceConfigurationCanonicalJson;
            create.attachment.requiredCapabilities = entity.scriptBehavior->requiredCapabilities;
            create.attachment.enabled = entity.scriptBehavior->enabled;
            create.instanceIdentity.value = "behavior-instance://" + envelope.playSessionId + "/" +
                entity.scriptBehavior->attachmentId;
            create.instanceIdentity.behaviorModuleId = create.attachment.behaviorModuleId;
            create.instanceIdentity.attachmentId = create.attachment.attachmentId;
            create.instanceIdentity.worldGenerationId = envelope.playSessionId;
            create.selfEntity = world.Handle(entity.entityId);
            created = csharpBackend->CreateInstance(create);
        }
        if (!created.Succeeded())
            return structuredFailure("Runtime.Script.InstanceCreateFailed",
                describeHostFailure(
                    "Script behavior instance could not be created.", created),
                "ScriptLifecycleFailure");
        const auto started = lifecycle.StartInstance(created.instance);
        if (!started.Succeeded())
            return structuredFailure("Runtime.Script.InstanceStartFailed",
                describeHostFailure(
                    "Script behavior instance failed to start.", started),
                "ScriptLifecycleFailure");
        instances.emplace(key, created.instance);
    }

    RuntimeNamedActionTarget target(host, instances, std::move(methods));
    Play::ScriptActionBindingDispatcher dispatcher(target);
    const auto bindingDiagnostics = dispatcher.Build(world.Scene());
    if (!bindingDiagnostics.empty())
        return structuredFailure(bindingDiagnostics.front().code,
            bindingDiagnostics.front().message, "ActionBindingFailure");
    Play::ScriptEventBindingDispatcher eventDispatcher(target);
    const auto eventBindingDiagnostics = eventDispatcher.Build(world.Scene());
    if (!eventBindingDiagnostics.empty())
        return structuredFailure(eventBindingDiagnostics.front().code,
            eventBindingDiagnostics.front().message, "EventBindingFailure");
    RuntimeTriggerSession triggers(world, eventDispatcher);

    const Play::RuntimeUiPromptBinding* runtimePrompt = nullptr;
    Scripting::ScriptEntityHandle promptEntity;
    std::string promptText;
    if (launch.schemaVersion >= 2)
    {
        if (uiResult.value->schemaVersion == 1)
        {
            if (uiResult.value->prompts.size() != 1)
                return structuredFailure("Runtime.UI.PromptBindingInvalid",
                    "Exactly one verified runtime prompt is required.",
                    "UiBootstrapFailure");
            runtimePrompt = &uiResult.value->prompts.front();
            promptEntity = world.Handle(runtimePrompt->entityId);
            const auto text = RuntimeUiPromptController::Compose(
                *runtimePrompt, *inputResult.value, stateStore, promptEntity);
            if (!text)
                return structuredFailure("Runtime.UI.PromptStateUnavailable",
                    "The prompt could not join verified input and active script state.",
                    "UiBootstrapFailure");
            promptText = *text;
        }
        else
        {
            const auto entityId = !uiResult.value->textBindings.empty()
                ? uiResult.value->textBindings.front().entityId
                : uiResult.value->documents.front().visibilityEntityId;
            promptEntity = world.Handle(entityId);
            if (!promptEntity.IsValid())
                return structuredFailure("Runtime.UI.EntityMissing",
                    "Typed UI binding entity is absent from the runtime scene.",
                    "UiBootstrapFailure");
        }
    }

    const auto publishReady = [&]
    {
        return RuntimeLaunchSession::WriteJson(envelope.startupReadyPath,
            {{"canonicalLaunchManifestHash", envelope.canonicalManifestHash},
             {"playSessionId", envelope.playSessionId}, {"schemaVersion", 1},
             {"status", "Ready"}});
    };

    std::uint64_t frames = 0;
    std::vector<double> observedHeights;
    std::vector<std::string> observedPrompts;
    std::vector<std::string> observedPhases;
    std::size_t restartCycleCount = 0;
    std::size_t restartCyclesWithScheduledRespawn = 0;
    bool restartCyclesPassed = true;
    const bool challengeMicrogame =
        authoredInputActions.contains("StartChallenge");
    const auto challengeBehaviorId =
        challengeMicrogame && uiResult.value->schemaVersion == 2 &&
                !uiResult.value->documents.empty()
            ? uiResult.value->documents.front().visibilityBehaviorId
            : std::string{};
    const auto challengePhaseKey =
        challengeMicrogame && uiResult.value->schemaVersion == 2 &&
                !uiResult.value->documents.empty()
            ? uiResult.value->documents.front().visibilityStateKey
            : std::string{};
    std::vector<std::string> collectibleEntityIds;
    std::string participantEntityId;
    if (challengeMicrogame)
    {
        std::string participantCategory;
        for (const auto& entity : world.Scene().entities)
            for (const auto& binding : entity.eventBindings)
                if (binding.behaviorModuleId == challengeBehaviorId &&
                    binding.eventType == "TriggerEnter" &&
                    binding.namedActionId.starts_with("Collect") &&
                    entity.triggerVolume)
                {
                    collectibleEntityIds.push_back(entity.entityId);
                    participantCategory =
                        entity.triggerVolume->acceptedCategory;
                }
        std::sort(collectibleEntityIds.begin(),
                  collectibleEntityIds.end());
        for (const auto& entity : world.Scene().entities)
            if (entity.triggerParticipant &&
                entity.triggerParticipant->category == participantCategory)
            {
                participantEntityId = entity.entityId;
                break;
            }
    }
    Json authoredChallengeConfiguration = Json::object();
    if (challengeMicrogame)
        for (const auto& entity : world.Scene().entities)
            if (entity.scriptBehavior &&
                entity.scriptBehavior->behaviorModuleId == challengeBehaviorId)
            {
                try
                {
                    authoredChallengeConfiguration = Json::parse(
                        entity.scriptBehavior
                            ->instanceConfigurationCanonicalJson);
                }
                catch (const Json::exception&) {}
                break;
            }
    const double authoredDuration =
        authoredChallengeConfiguration.value(
            "challengeDurationSeconds", 60.0);
    const int authoredTarget =
        authoredChallengeConfiguration.value("targetProgress", 100);
    const bool timeoutConfigurationScenario =
        challengeMicrogame && authoredDuration <= 1.0;
    Json collectibleLifecycleEvidence = Json::object();
    const auto gameModeEntity = promptEntity;
    const auto recordArenaPhase = [&]
    {
        const auto snapshot = stateStore.SnapshotValue(gameModeEntity,
            challengeBehaviorId, challengePhaseKey);
        if (snapshot && std::holds_alternative<std::string>(snapshot->value))
            observedPhases.push_back(std::get<std::string>(snapshot->value));
    };
    const auto observedEntity = std::find_if(world.Scene().entities.begin(), world.Scene().entities.end(),
        [](const auto& entity) { return !entity.actionBindings.empty(); });
    if (observedEntity == world.Scene().entities.end())
        return structuredFailure("Runtime.Script.ActionBindingMissing",
            "Runtime scene has no observable script action binding.", "ActionBindingFailure");
    const auto record = [&]
    {
        const auto position = world.GetPosition(world.Handle(observedEntity->entityId));
        if (position.Succeeded()) observedHeights.push_back(position.position.y);
        if (runtimePrompt != nullptr)
        {
            const auto text = RuntimeUiPromptController::Compose(
                *runtimePrompt, *inputResult.value, stateStore, promptEntity);
            if (text) { promptText = *text; observedPrompts.push_back(*text); }
        }
    };
    const auto staticInputActionCount = inputResult.value->actions.size();
    const auto staticUiBindingCount = launch.schemaVersion >= 2
        ? uiResult.value->prompts.size() +
              uiResult.value->textBindings.size() +
              uiResult.value->inputLabelBindings.size()
        : 0U;
    RuntimeGameplaySession gameplay(inputPort, triggers,
        [&](const double fixedDelta)
        {
            for (const auto& [identity, instance] : instances)
            {
                (void)identity;
                if (!lifecycle.UpdateInstance(instance, fixedDelta).Succeeded())
                    return false;
            }
            return true;
        },
        [&]
        {
            std::size_t pendingRespawns = 0;
            if (challengeMicrogame)
            {
                const auto pending = stateStore.SnapshotValue(gameModeEntity,
                    challengeBehaviorId, "challenge.pending-respawns");
                pendingRespawns = pending &&
                        std::holds_alternative<std::int32_t>(pending->value) &&
                        std::get<std::int32_t>(pending->value) >= 0
                    ? static_cast<std::size_t>(
                        std::get<std::int32_t>(pending->value))
                    : std::numeric_limits<std::size_t>::max();
            }
            return RuntimeGameplayOwnershipCounters{
                world.EntityCount(),
                host.InstanceCount(),
                eventDispatcher.BindingCount(),
                0,
                stateStore.KeyCount(),
                pendingRespawns};
        });
    auto liveOwnershipBaseline = gameplay.LiveOwnershipCounters();
    auto liveOwnershipFinal = liveOwnershipBaseline;
    const auto updateInstances = [&](std::set<std::string> pressed =
                                         std::set<std::string>{},
                                     std::set<std::string> held =
                                         std::set<std::string>{})
    {
        return gameplay.Advance(1.0 / 60.0, std::move(pressed),
                                std::move(held));
    };
    record();
    bool renderEvidence = false;
    bool gracefulStop = false;
    std::size_t drawCount = 0;
    Json evidenceCaptures = Json::array();
    Json uiStateEvidence = Json::array();
    if (headless)
    {
        if (!publishReady())
            return structuredFailure("Runtime.Startup.ReadyPublishFailed",
                "The session-bound startup-ready record could not be published.",
                "LaunchValidationFailure");
        if (smokeInteractions)
        {
            if (challengeMicrogame)
            {
                if (collectibleEntityIds.empty() ||
                    participantEntityId.empty() ||
                    challengeBehaviorId.empty() ||
                    challengePhaseKey.empty())
                    return structuredFailure(
                        "Runtime.Play.ChallengeEvidenceInvalid",
                        "Challenge smoke evidence could not derive its verified "
                        "behavior, participant, collectibles, or phase state.",
                        "ObservedStateMismatch");
                const auto dispatchAction = [&](std::string_view action)
                {
                    const auto diagnostics = dispatcher.Dispatch(action);
                    if (action == "Restart") gameplay.Reset();
                    return diagnostics.empty();
                };
                const auto tick = [&](const int count)
                {
                    for (int index = 0; index < count; ++index)
                    {
                        if (!updateInstances()) return false;
                        ++frames;
                    }
                    return true;
                };
                const auto collectRound = [&]
                {
                    for (const auto& collectibleId : collectibleEntityIds)
                    {
                        const auto collectible =
                            world.GetPosition(world.Handle(collectibleId));
                        if (!collectible.Succeeded() ||
                            !world.SetPosition(world.Handle(
                                participantEntityId),
                                collectible.position).Succeeded() || !tick(1))
                            return false;
                    }
                    return true;
                };
                recordArenaPhase();
                if (!dispatchAction("StartChallenge") || !tick(1))
                    return structuredFailure("Runtime.Play.ArenaStartFailed",
                        "Challenge microgame could not arm and enter Running.",
                        "ActionDispatchFailure");
                recordArenaPhase();
                if (timeoutConfigurationScenario)
                {
                    const auto timeoutTicks = static_cast<int>(
                        std::ceil(authoredDuration * 60.0)) + 1;
                    if (!tick(timeoutTicks))
                        return structuredFailure("Runtime.Play.ArenaTimeoutFailed",
                            "Challenge authored timeout lifecycle failed.",
                            "ObservedStateMismatch");
                    recordArenaPhase();
                    if (!dispatchAction("Restart"))
                        return structuredFailure(
                            "Runtime.Play.ArenaRestartFailed",
                            "Challenge timeout restart failed.",
                            "ActionDispatchFailure");
                    recordArenaPhase();
                }
                else
                {
                    const auto intValue = [](const auto& snapshot)
                    {
                        return snapshot &&
                            std::holds_alternative<std::int32_t>(
                                snapshot->value)
                            ? std::get<std::int32_t>(snapshot->value)
                            : -1;
                    };
                    const auto safePlayerPosition = world.GetPosition(
                        world.Handle(participantEntityId));
                    const auto firstCollectible = world.GetPosition(
                        world.Handle(collectibleEntityIds.front()));
                    if (!safePlayerPosition.Succeeded() ||
                        !firstCollectible.Succeeded() ||
                        !world.SetPosition(world.Handle(participantEntityId),
                            firstCollectible.position).Succeeded() || !tick(1))
                        return structuredFailure(
                            "Runtime.Play.ArenaCollectFailed",
                            "Challenge first collectible failed.",
                            "ObservedStateMismatch");
                    const auto scoreAfterFirst = stateStore.SnapshotValue(
                        gameModeEntity, challengeBehaviorId, "challenge.score");
                    if (intValue(scoreAfterFirst) >= authoredTarget)
                    {
                        collectibleLifecycleEvidence = {
                            {"mode", "SingleCollectWin"},
                            {"score", intValue(scoreAfterFirst)},
                            {"target", authoredTarget}};
                    }
                    else
                    {
                        if (!collectRound() ||
                            !world.SetPosition(world.Handle(
                                participantEntityId),
                                safePlayerPosition.position).Succeeded())
                            return structuredFailure(
                                "Runtime.Play.ArenaCollectFailed",
                                "Challenge first collectible round failed.",
                                "ObservedStateMismatch");
                        const auto pendingBefore = stateStore.SnapshotValue(
                            gameModeEntity, challengeBehaviorId,
                            "challenge.pending-respawns");
                        const auto activeBefore = stateStore.SnapshotValue(
                            gameModeEntity, challengeBehaviorId,
                            "challenge.active-cores");
                        int respawnTicks = 0;
                        while (respawnTicks < 600)
                        {
                            const auto pending = stateStore.SnapshotValue(
                                gameModeEntity, challengeBehaviorId,
                                "challenge.pending-respawns");
                            if (intValue(pending) == 0) break;
                            if (!tick(1))
                                return structuredFailure(
                                    "Runtime.Play.ArenaRespawnFailed",
                                    "Challenge respawn observation failed.",
                                    "ObservedStateMismatch");
                            ++respawnTicks;
                        }
                        const auto pendingAfter = stateStore.SnapshotValue(
                            gameModeEntity, challengeBehaviorId,
                            "challenge.pending-respawns");
                        const auto activeAfter = stateStore.SnapshotValue(
                            gameModeEntity, challengeBehaviorId,
                            "challenge.active-cores");
                        collectibleLifecycleEvidence = {
                            {"scoreAfterFirst", intValue(scoreAfterFirst)},
                            {"activeAfterCollect", intValue(activeBefore)},
                            {"activeAfterRespawn", intValue(activeAfter)},
                            {"pendingAfterCollect", intValue(pendingBefore)},
                            {"pendingAfterRespawn", intValue(pendingAfter)},
                            {"respawnDelaySeconds",
                                static_cast<double>(respawnTicks) / 60.0},
                            {"respawnTicks", respawnTicks}};
                        if (intValue(pendingBefore) == 0 ||
                            intValue(activeBefore) != 0 ||
                            intValue(pendingAfter) != 0 ||
                            intValue(activeAfter) !=
                                static_cast<int>(collectibleEntityIds.size()) ||
                            !collectRound())
                            return structuredFailure(
                                "Runtime.Play.ArenaRespawnFailed",
                                "Challenge collectible deactivate/respawn "
                                "state did not match authored configuration "
                                "(active " +
                                    std::to_string(intValue(activeBefore)) +
                                    "->" +
                                    std::to_string(intValue(activeAfter)) +
                                    ", pending " +
                                    std::to_string(intValue(pendingBefore)) +
                                    "->" +
                                    std::to_string(intValue(pendingAfter)) +
                                    ").",
                                "ObservedStateMismatch");
                    }
                    recordArenaPhase();
                    if (!dispatchAction("Restart"))
                        return structuredFailure(
                            "Runtime.Play.ArenaRestartFailed",
                            "Challenge first restart failed.",
                            "ActionDispatchFailure");
                    recordArenaPhase();
                    const auto timeoutTicks = static_cast<int>(
                        std::ceil(authoredDuration * 60.0)) + 1;
                    if (!dispatchAction("StartChallenge") || !tick(1) ||
                        !tick(timeoutTicks))
                        return structuredFailure(
                            "Runtime.Play.ArenaTimeoutFailed",
                            "Challenge timeout lifecycle failed.",
                            "ObservedStateMismatch");
                    recordArenaPhase();
                    if (!dispatchAction("Restart"))
                        return structuredFailure(
                            "Runtime.Play.ArenaSecondRestartFailed",
                            "Challenge second restart failed.",
                            "ActionDispatchFailure");
                    recordArenaPhase();
                }
                liveOwnershipBaseline = gameplay.LiveOwnershipCounters();
                const auto expectedCounters = liveOwnershipBaseline;
                restartCyclesPassed =
                    expectedCounters.activeTriggerOverlaps == 0 &&
                    expectedCounters.pendingCollectibleRespawns == 0;
                for (int cycle = 0;
                     restartCyclesPassed && cycle < 50; ++cycle)
                {
                    const auto collectible = world.GetPosition(
                        world.Handle(collectibleEntityIds.front()));
                    if (!dispatchAction("StartChallenge") || !tick(1) ||
                        !collectible.Succeeded() ||
                        !world.SetPosition(world.Handle(participantEntityId),
                            collectible.position).Succeeded() ||
                        !tick(1))
                    {
                        restartCyclesPassed = false;
                        break;
                    }
                    const auto scheduled = gameplay.LiveOwnershipCounters();
                    if (scheduled.pendingCollectibleRespawns == 0 ||
                        scheduled.pendingCollectibleRespawns ==
                            std::numeric_limits<std::size_t>::max() ||
                        !dispatchAction("Restart"))
                    {
                        restartCyclesPassed = false;
                        break;
                    }
                    ++restartCyclesWithScheduledRespawn;
                    const auto counters = gameplay.LiveOwnershipCounters();
                    restartCyclesPassed =
                        counters.runtimeEntities == expectedCounters.runtimeEntities &&
                        counters.scriptInstances == expectedCounters.scriptInstances &&
                        counters.triggerBindings == expectedCounters.triggerBindings &&
                        counters.activeTriggerOverlaps ==
                            expectedCounters.activeTriggerOverlaps &&
                        counters.stateKeys == expectedCounters.stateKeys &&
                        counters.pendingCollectibleRespawns == 0;
                    liveOwnershipFinal = counters;
                    if (!restartCyclesPassed) break;
                    ++restartCycleCount;
                }
            }
            else
            {
                for (int cycle = 0; cycle < 2; ++cycle)
                {
                    const auto dispatch = dispatcher.Dispatch("Interact");
                    if (!dispatch.empty())
                        return structuredFailure(dispatch.front().code,
                            dispatch.front().message, "ActionDispatchFailure");
                    for (int frame = 0; frame < 120; ++frame)
                    {
                        if (!updateInstances())
                            return structuredFailure("Runtime.Script.UpdateFailed",
                                "Script OnUpdate failed.", "ScriptLifecycleFailure");
                        ++frames;
                    }
                    record();
                }
            }
        }
        else
        {
            while (!std::filesystem::exists(envelope.stopPath))
            {
                if (!updateInstances())
                    return structuredFailure("Runtime.Script.UpdateFailed",
                        "Script OnUpdate failed.", "ScriptLifecycleFailure");
                ++frames;
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
            gracefulStop = true;
        }
    }
    else
    {
        RuntimeRenderSession render(world, envelope.artifacts, envelopeResult.diagnostics,
            envelope.workingDirectory / "captures");
        if (!render.Initialize())
        {
            writeResult("Failed", "RendererFailure", frames, dispatcher.DispatchCount(), Json::object());
            return Failure(envelopeResult.diagnostics.back().code,
                envelopeResult.diagnostics.back().message);
        }
        std::uint64_t renderedFrameIndex = 0;
        RuntimeUiSession ui;
        auto* overlay = render.OverlaySubmission();
        if (launch.schemaVersion >= 2 && (overlay == nullptr ||
            !ui.Initialize(*uiResult.value, *inputResult.value, stateStore,
                promptEntity, *overlay, render.Width(), render.Height())))
        {
            render.Shutdown();
            return structuredFailure("Runtime.UI.BootstrapFailed",
                "Verified runtime UI context, font, document, or prompt failed to initialize.",
                "UiBootstrapFailure");
        }
        if (!publishReady())
        {
            render.Shutdown();
            return structuredFailure("Runtime.Startup.ReadyPublishFailed",
                "The session-bound startup-ready record could not be published.",
                "LaunchValidationFailure");
        }
        bool recordedOpen = false;
        std::string bindingLabel;
        const auto evidenceAction = uiResult.value->schemaVersion == 1
            ? "Interact" : "StartChallenge";
        for (const auto& action : inputResult.value->actions)
            if (action.actionId == evidenceAction && !action.bindings.empty())
            {
                bindingLabel = action.bindings.front().displayLabel;
                break;
            }
        std::string capturePrefix;
        std::transform(bindingLabel.begin(), bindingLabel.end(),
            std::back_inserter(capturePrefix), [](const unsigned char value)
            { return static_cast<char>(std::tolower(value)); });
        if (capturePrefix.empty())
        {
            ui.Shutdown();
            render.Shutdown();
            return structuredFailure("Runtime.Render.EvidenceBindingMissing",
                "Named graphical evidence requires a verified display label.",
                "RendererFailure");
        }
        while (!render.ShouldClose())
        {
            if (std::filesystem::exists(envelope.stopPath)) { gracefulStop = true; break; }
            if (smokeInteractions && frames == 240 && !render.RequestResize(1024, 640))
            {
                ui.Shutdown();
                render.Shutdown();
                return structuredFailure("Runtime.Render.EvidenceResizeFailed",
                    "The named resize evidence frame could not resize the viewport.",
                    "RendererFailure");
            }
            std::string_view injectedAction;
            if (smokeInteractions)
            {
                if (challengeMicrogame && frames == 30)
                    injectedAction = "StartChallenge";
                else if (challengeMicrogame && frames == 190)
                    injectedAction = "Restart";
                else if (!challengeMicrogame &&
                         (frames == 30 || frames == 190))
                    injectedAction = "Interact";
            }
            auto actions =
                render.PollLogicalActions(*inputResult.value, injectedAction);
            if (!injectedAction.empty() &&
                std::find(actions.begin(), actions.end(), injectedAction) ==
                    actions.end())
                actions.emplace_back(injectedAction);
            if (render.ConsumeFocusLost())
            {
                inputPort.Clear();
                render.ReleaseCursor();
                const auto focusDiagnostics =
                    eventDispatcher.Dispatch("FocusLost", "game-mode-focus-binding");
                if (!focusDiagnostics.empty())
                {
                    ui.Shutdown();
                    render.Shutdown();
                    return structuredFailure(focusDiagnostics.front().code,
                        focusDiagnostics.front().message, "EventDispatchFailure");
                }
            }
            for (const auto& action : actions)
            {
                if (smokeInteractions && dispatcher.DispatchCount() == 1 && !recordedOpen)
                { record(); recordedOpen = true; }
                const auto dispatch = dispatcher.Dispatch(action);
                if (!dispatch.empty())
                {
                    render.Shutdown();
                    return structuredFailure(dispatch.front().code,
                        dispatch.front().message, "ActionDispatchFailure");
                }
                if (action == "Restart") gameplay.Reset();
            }
            if (challengeMicrogame && smokeInteractions &&
                ((frames >= 40 && frames <= 44) ||
                 (frames >= 170 && frames <= 174)))
            {
                const auto index = frames >= 170
                    ? static_cast<std::size_t>(frames - 170)
                    : static_cast<std::size_t>(frames - 40);
                if (index >= collectibleEntityIds.size())
                {
                    ui.Shutdown();
                    render.Shutdown();
                    return structuredFailure(
                        "Runtime.Play.ChallengeEvidenceInvalid",
                        "Graphical challenge evidence requires five verified "
                        "collectible trigger sources.",
                        "ObservedStateMismatch");
                }
                const auto core =
                    world.GetPosition(world.Handle(
                        collectibleEntityIds[index]));
                if (!core.Succeeded() ||
                    !world.SetPosition(
                        world.Handle(participantEntityId),
                        core.position).Succeeded())
                {
                    ui.Shutdown();
                    render.Shutdown();
                    return structuredFailure("Runtime.Play.ArenaCollectFailed",
                        "Graphical challenge could not position the participant for "
                        "deterministic trigger evidence.",
                        "ObservedStateMismatch");
                }
            }
            if (!updateInstances(
                    std::set<std::string>(actions.begin(), actions.end()),
                    render.HeldLogicalActions()))
            {
                render.Shutdown();
                return structuredFailure("Runtime.Script.UpdateFailed",
                    "Script OnUpdate failed.", "ScriptLifecycleFailure");
            }
            std::optional<RuntimeEvidenceCaptureRequest> captureRequest;
            if (smokeInteractions && launch.schemaVersion >= 2 &&
                runtimePrompt != nullptr &&
                (frames == 0 || frames == 30 || frames == 190 || frames == 245))
            {
                const auto snapshot = stateStore.Snapshot(
                    promptEntity, runtimePrompt->behaviorId, runtimePrompt->stateKey);
                const auto text = RuntimeUiPromptController::Compose(
                    *runtimePrompt, *inputResult.value, stateStore, promptEntity);
                if (!snapshot || !text)
                {
                    ui.Shutdown();
                    render.Shutdown();
                    return structuredFailure("Runtime.Render.EvidenceSemanticStateMissing",
                        "Named capture semantic state was unavailable for its frame.",
                        "RendererFailure");
                }
                const auto suffix = frames == 0 ? "open" : frames == 30 ? "close" :
                    frames == 190 ? "reopened" : "resized";
                const auto prior = frames == 30 ? capturePrefix + "-open" :
                    frames == 190 ? capturePrefix + "-close" :
                    frames == 245 ? capturePrefix + "-reopened" : std::string{};
                captureRequest = RuntimeEvidenceCaptureRequest{
                    capturePrefix + "-" + suffix, renderedFrameIndex,
                    bindingLabel, *text, snapshot->value, 0U, prior};
            }
            if (!render.RenderFrame(launch.schemaVersion >= 2 ? &ui : nullptr,
                                    captureRequest ? &*captureRequest : nullptr))
            {
                ui.Shutdown();
                render.Shutdown();
                return structuredFailure("Runtime.Render.FrameFailed",
                    "Runtime renderer failed a frame.", "RendererFailure");
            }
            if (challengeMicrogame &&
                uiResult.value->schemaVersion == 2 &&
                (frames == 0 || frames == 30 || frames == 174 ||
                 frames == 190))
            {
                const auto phase = stateStore.SnapshotValue(gameModeEntity,
                    challengeBehaviorId, challengePhaseKey);
                Json visible = Json::array();
                for (const auto& document : ui.VisibleDocuments())
                    visible.push_back(document);
                uiStateEvidence.push_back({
                    {"drawCallCount", ui.LastDrawCallCount()},
                    {"frame", frames},
                    {"phase", phase &&
                        std::holds_alternative<std::string>(phase->value)
                            ? std::get<std::string>(phase->value)
                            : std::string{}},
                    {"visibleDocuments", std::move(visible)}});
            }
            ++renderedFrameIndex;
            ++frames;
            if (exitPort.Requested()) { gracefulStop = true; break; }
            if (smokeInteractions && frames >= 350) break;
        }
        record();
        renderEvidence = uiResult.value->schemaVersion == 2
            ? render.LastDrawCount() > 0 : render.HasMeaningfulPixels();
        drawCount = render.LastDrawCount();
        Json stylesheetHashes = Json::array();
        for (const auto& stylesheet : uiResult.value->stylesheets)
            stylesheetHashes.push_back(stylesheet.hash);
        std::sort(stylesheetHashes.begin(), stylesheetHashes.end());
        for (const auto& capture : render.EvidenceRecords())
        {
            evidenceCaptures.push_back({
                {"baselineDifferencePixelCount", capture.baselineDifferencePixelCount},
                {"baselineRgbAbsoluteDifference", capture.baselineRgbAbsoluteDifference},
                {"canonicalLaunchHash", envelope.canonicalManifestHash},
                {"doorState", capture.doorState}, {"filePath", capture.path.string()},
                {"frameIndex", capture.frameIndex},
                {"height", capture.height},
                {"hudRegion", {{"height", capture.hudHeight}, {"width", capture.hudWidth},
                    {"x", capture.hudX}, {"y", capture.hudY}}},
                {"layoutBounds", {{"height", capture.layoutBounds.height},
                    {"width", capture.layoutBounds.width},
                    {"x", capture.layoutBounds.x}, {"y", capture.layoutBounds.y}}},
                {"inputArtifactHash", launch.input.hash}, {"name", capture.name},
                {"nonBackgroundPixelCount", capture.nonBackgroundPixelCount},
                {"packageGenerationHash", launch.packageGeneration.hash},
                {"pixelRatio", capture.pixelRatio},
                {"preOverlayFrameIndex", capture.preOverlayFrameIndex},
                {"preOverlayPath", capture.preOverlayPath.string()},
                {"preOverlaySha256", Play::Sha256Bytes(
                    RuntimeLaunchSession::ReadFile(capture.preOverlayPath))},
                {"priorSemanticDifferencePixelCount",
                    capture.priorSemanticDifferencePixelCount},
                {"priorSemanticRgbAbsoluteDifference",
                    capture.priorSemanticRgbAbsoluteDifference},
                {"promptBindingLabel", capture.promptBindingLabel},
                {"promptSemanticLabel", capture.promptSemanticLabel},
                {"sceneHash", launch.scene.hash},
                {"scriptAssemblyHash", launch.scriptAssembly.hash},
                {"sessionId", envelope.playSessionId},
                {"sha256", Play::Sha256Bytes(RuntimeLaunchSession::ReadFile(capture.path))},
                {"uiArtifactHash", launch.uiSet.hash},
                {"uiDocumentHash", uiResult.value->documents.front().hash},
                {"uiStylesheetHashes", stylesheetHashes},
                {"width", capture.width}});
            evidenceCaptures.back()["uiDrawCallCount"] = capture.uiDrawCallCount;
            envelopeResult.diagnostics.push_back({"Runtime.Render.NamedFrameCapture",
                capture.name, capture.path.string()});
        }
        const auto& records = render.EvidenceRecords();
        const auto findCapture = [&](std::string_view suffix)
        {
            return std::find_if(records.begin(), records.end(),
                [&](const auto& value) { return value.name == capturePrefix + std::string(suffix); });
        };
        const auto open = findCapture("-open");
        const auto close = findCapture("-close");
        const auto reopened = findCapture("-reopened");
        const auto resized = findCapture("-resized");
        const auto hasBaselineDifference = [](const auto& capture)
        {
            return capture.baselineDifferencePixelCount >= 32U &&
                capture.baselineRgbAbsoluteDifference >= 2048U;
        };
        const auto hasSemanticDifference = [](const auto& capture)
        {
            return capture.priorSemanticDifferencePixelCount >= 16U &&
                capture.priorSemanticRgbAbsoluteDifference >= 1024U;
        };
        const auto centered = [](const auto& capture)
        {
            const double center = capture.layoutBounds.x +
                capture.layoutBounds.width * 0.5;
            return std::abs(center - static_cast<double>(capture.width) * 0.5) <= 2.0;
        };
        bool resizeAnchored = false;
        if (open != records.end() && resized != records.end() &&
            (open->width != resized->width || open->height != resized->height))
        {
            const double baselineGap = static_cast<double>(open->height) -
                (open->layoutBounds.y + open->layoutBounds.height);
            const double resizedGap = static_cast<double>(resized->height) -
                (resized->layoutBounds.y + resized->layoutBounds.height);
            const double expectedGap = baselineGap *
                static_cast<double>(resized->height) / open->height;
            resizeAnchored = std::abs(resizedGap - expectedGap) <= 2.0;
        }
        if (uiResult.value->schemaVersion == 1)
            renderEvidence = renderEvidence && records.size() == 5U &&
            open != records.end() && close != records.end() &&
            reopened != records.end() && resized != records.end() &&
            hasBaselineDifference(*open) && hasBaselineDifference(*close) &&
            hasBaselineDifference(*reopened) && hasBaselineDifference(*resized) &&
            hasSemanticDifference(*close) && hasSemanticDifference(*reopened) &&
            centered(*open) && centered(*close) && centered(*reopened) &&
            centered(*resized) && resizeAnchored;
        ui.Shutdown();
        render.Shutdown();
    }
    for (auto iterator = instances.rbegin(); iterator != instances.rend(); ++iterator)
        if (!lifecycle.DestroyInstance(iterator->second).Succeeded())
            return structuredFailure("Runtime.Script.DestroyFailed",
                "Script OnDestroy failed.", "ScriptLifecycleFailure");
    const bool transitioned = observedHeights.size() == 3 &&
        observedHeights[0] < observedHeights[1] &&
        std::abs(observedHeights[0] - observedHeights[2]) < 0.01;
    const bool challengeAccepted = timeoutConfigurationScenario
        ? observedPhases.size() == 4 &&
            observedPhases[0] == "PreStart" &&
            observedPhases[1] == "Running" &&
            observedPhases[2] == "Timeout" &&
            observedPhases[3] == "PreStart"
        : observedPhases.size() == 6 &&
            observedPhases[0] == "PreStart" &&
            observedPhases[1] == "Running" &&
            observedPhases[2] == "Won" &&
            observedPhases[3] == "PreStart" &&
            observedPhases[4] == "Timeout" &&
            observedPhases[5] == "PreStart";
    const auto uiEvidenceMatches = [&](const std::size_t index,
        std::string_view phase, std::string_view document)
    {
        if (index >= uiStateEvidence.size()) return false;
        const auto& evidence = uiStateEvidence[index];
        if (evidence.value("phase", std::string{}) != phase ||
            evidence.value("drawCallCount", 0U) == 0U ||
            !evidence.contains("visibleDocuments") ||
            !evidence["visibleDocuments"].is_array() ||
            evidence["visibleDocuments"].size() != 1)
            return false;
        return evidence["visibleDocuments"].front().get<std::string>() ==
            document;
    };
    const auto visibilityDocumentFor = [&](std::string_view phase)
    {
        const auto found = std::find_if(
            uiResult.value->documents.begin(),
            uiResult.value->documents.end(),
            [&](const auto& document)
            {
                return std::find(document.visibilityEqualsAny.begin(),
                    document.visibilityEqualsAny.end(), phase) !=
                    document.visibilityEqualsAny.end();
            });
        return found == uiResult.value->documents.end()
            ? std::string{} : found->documentId;
    };
    const bool graphicalChallengeAccepted =
        uiStateEvidence.size() == 4 &&
        uiEvidenceMatches(0, "PreStart",
            visibilityDocumentFor("PreStart")) &&
        uiEvidenceMatches(1, "Running",
            visibilityDocumentFor("Running")) &&
        uiEvidenceMatches(2, "Won", visibilityDocumentFor("Won")) &&
        uiEvidenceMatches(3, "PreStart",
            visibilityDocumentFor("PreStart"));
    const bool smokeAccepted = challengeMicrogame
        ? (headless
            ? challengeAccepted && restartCyclesPassed &&
                restartCycleCount == 50 &&
                restartCyclesWithScheduledRespawn == 50
            : graphicalChallengeAccepted)
        : transitioned;
    const Json observed = {{"drawCount", drawCount}, {"entityId", observedEntity->entityId},
        {"evidenceCaptures", evidenceCaptures},
        {"uiStateEvidence", uiStateEvidence},
        {"meaningfulRendererPixels", renderEvidence}, {"positionY", observedHeights},
        {"promptText", observedPrompts},
        {"authoredConfiguration", authoredChallengeConfiguration},
        {"challengePhases", observedPhases},
        {"collectibleLifecycle", collectibleLifecycleEvidence},
        {"restartCycles", {
            {"completed", restartCycleCount},
            {"passed", restartCyclesPassed},
            {"scheduledRespawnCycles",
                restartCyclesWithScheduledRespawn}}},
        {"liveOwnershipCounters", {
            {"baseline", {
                {"activeTriggerOverlaps",
                    liveOwnershipBaseline.activeTriggerOverlaps},
                {"runtimeEntities",
                    liveOwnershipBaseline.runtimeEntities},
                {"scriptInstances",
                    liveOwnershipBaseline.scriptInstances},
                {"stateKeys", liveOwnershipBaseline.stateKeys},
                {"triggerBindings",
                    liveOwnershipBaseline.triggerBindings}}},
            {"final", {
                {"activeTriggerOverlaps",
                    liveOwnershipFinal.activeTriggerOverlaps},
                {"runtimeEntities", liveOwnershipFinal.runtimeEntities},
                {"scriptInstances", liveOwnershipFinal.scriptInstances},
                {"stateKeys", liveOwnershipFinal.stateKeys},
                {"triggerBindings",
                    liveOwnershipFinal.triggerBindings}}}}},
        {"pendingTransientState", {
            {"baselineCollectibleRespawns",
                liveOwnershipBaseline.pendingCollectibleRespawns},
            {"finalCollectibleRespawns",
                liveOwnershipFinal.pendingCollectibleRespawns}}},
        {"staticTopology", {
            {"collectibles", collectibleEntityIds.size()},
            {"collectibleIds", collectibleEntityIds},
            {"inputActions", staticInputActionCount},
            {"uiArtifactBindings", staticUiBindingCount}}},
        {"simulationTicks", gameplay.TickCount()},
        {"transition", challengeMicrogame
            ? (headless
                ? (challengeAccepted
                    ? (timeoutConfigurationScenario
                        ? "ChallengeTimeoutConfigurationAccepted"
                        : "ChallengeAccepted")
                    : "Incomplete")
                : (graphicalChallengeAccepted
                    ? "ChallengeUiAccepted" : "Incomplete"))
            : (transitioned ? "ClosedOpenClosed" : "Incomplete")}};
    if ((smokeInteractions && !smokeAccepted) ||
        (!headless && !renderEvidence))
    {
        writeResult("Failed", "ObservedStateMismatch", frames,
            dispatcher.DispatchCount(), observed);
        return Failure("Runtime.Play.ObservedStateMismatch",
            "Script-driven transform did not complete closed/open/closed transitions.");
    }
    writeResult("Succeeded", gracefulStop ? "GracefulStop" : "Completed",
                frames, dispatcher.DispatchCount(), observed);
    return {true, 0, {}, "Runtime Play completed successfully."};
}
} // namespace SagaRuntimeApp
