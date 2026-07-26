// SPDX-License-Identifier: MPL-2.0

#include "RuntimeFrameLoop.h"
#include "RuntimeArtifactSession.h"
#include "RuntimeLaunchSession.h"
#include "RuntimeRenderSession.h"
#include "RuntimeScriptSession.h"
#include "RuntimeStateStore.h"
#include "RuntimeUiPromptController.h"
#include "RuntimeUiSession.h"

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <SagaEngine/Scripting/ScriptLifecycleService.hpp>
#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <map>
#include <thread>

namespace SagaRuntimeApp
{
namespace
{
using Json = nlohmann::json;
namespace Play = SagaRuntime::Play;
namespace Scripting = SagaEngine::Scripting;

RuntimeHostResult Failure(std::string code, std::string message)
{
    RuntimeHostResult result;
    result.exitCode = 1;
    result.diagnosticId = std::move(code);
    result.message = std::move(message);
    return result;
}
} // namespace

RuntimeHostResult RunRuntimeFrameLoop(const std::filesystem::path& playEnvelope,
                                      bool headless,
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
    auto writeResult = [&](std::string status, std::string classification,
                           std::uint64_t frames, std::uint64_t actions,
                           const Json& observed)
    {
        if (envelope.resultPath.empty()) return;
        Json diagnostics = Json::array();
        for (const auto& diagnostic : envelopeResult.diagnostics)
            diagnostics.push_back({{"code", diagnostic.code},
                {"message", diagnostic.message}, {"subject", diagnostic.subject}});
        static_cast<void>(RuntimeLaunchSession::WriteJson(envelope.resultPath, {{"canonicalLaunchManifestHash", envelope.canonicalManifestHash},
            {"crashArtifactPath", nullptr},
            {"exitClassification", classification}, {"exitCode", status == "Succeeded" ? 0 : 1},
            {"framesExecuted", frames}, {"interactionActionCount", actions},
            {"logPath", envelope.logPath.string()},
            {"observedState", observed}, {"playSessionId", envelope.playSessionId},
            {"runtimeDiagnostics", diagnostics}, {"schemaVersion", 1},
            {"startupDiagnostics", Json::array()}, {"status", status},
            {"shutdownClassification", classification == "GracefulStop" ?
                "GracefulStop" : (status == "Succeeded" ? "Clean" : "StartupFailure")}}));
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
    if (launch.runtimeBridge.abi != "NativeCallbackTable.v2+NamedAction.v1" ||
        launch.runtimeBridge.targetFramework != "net10.0")
    {
        envelopeResult.diagnostics.push_back({"Runtime.Bridge.AbiMismatch",
            "RuntimeBridge ABI or target framework is incompatible with this SagaRuntime.",
            launch.runtimeBridge.logicalReferenceId});
    }
    static_cast<void>(RuntimeArtifactSession::VerifyFiles(envelope.artifacts,
        {launch.scene, launch.input, launch.scriptAssembly, launch.uiSet,
         launch.scriptRuntimeConfig, launch.scriptAssemblyManifest,
         launch.scriptReferenceManifest, launch.behaviors,
         launch.namedActions, launch.capabilities}, envelopeResult.diagnostics));
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
    const auto uiResult = launch.schemaVersion == 2
        ? Play::LoadRuntimeUiSetArtifact(envelope.artifacts.at(launch.uiSet.logicalId))
        : Play::ArtifactLoadResult<Play::RuntimeUiSetArtifact>{};
    if (!sceneResult.Succeeded() || !inputResult.Succeeded() ||
        (launch.schemaVersion == 2 && !uiResult.Succeeded()))
    {
        writeResult("Failed", "ArtifactFailure", 0, 0, Json::object());
        return Failure("Runtime.Play.ArtifactLoadFailed",
            "Runtime scene, input, or UI artifact failed strict loading.");
    }
    Json namedActions, behaviorManifest, capabilityManifest;
    try
    {
        namedActions = Json::parse(RuntimeLaunchSession::ReadFile(
            envelope.artifacts.at(launch.namedActions.logicalId)));
        behaviorManifest = Json::parse(RuntimeLaunchSession::ReadFile(
            envelope.artifacts.at(launch.behaviors.logicalId)));
        capabilityManifest = Json::parse(RuntimeLaunchSession::ReadFile(
            envelope.artifacts.at(launch.capabilities.logicalId)));
    }
    catch (const Json::exception&)
    {
        return structuredFailure("Runtime.Script.NamedActionsInvalid",
            "Named-action manifest is malformed.", "ScriptLoadFailure");
    }
    RuntimeScriptWorld world(*sceneResult.value);
    RuntimeStateStore stateStore;
    Scripting::CSharpScriptHost host({envelope.artifacts.at(
        launch.runtimeBridge.logicalReferenceId), {}, &world, &stateStore, true, {}});
    Scripting::ScriptLifecycleService lifecycle(host);
    Scripting::VerifiedScriptGenerationLoadRequest loadRequest;
    loadRequest.assembly = {envelope.artifacts.at(launch.scriptAssembly.logicalId),
                            launch.scriptAssembly.hash};
    loadRequest.runtimeConfig = {envelope.artifacts.at(launch.scriptRuntimeConfig.logicalId),
                                 launch.scriptRuntimeConfig.hash};
    loadRequest.assemblyManifest = {envelope.artifacts.at(launch.scriptAssemblyManifest.logicalId),
                                    launch.scriptAssemblyManifest.hash};
    loadRequest.behaviorManifest = {envelope.artifacts.at(launch.behaviors.logicalId),
                                    launch.behaviors.hash};
    loadRequest.namedActionManifest = {envelope.artifacts.at(launch.namedActions.logicalId),
                                       launch.namedActions.hash};
    loadRequest.capabilityManifest = {envelope.artifacts.at(launch.capabilities.logicalId),
                                      launch.capabilities.hash};
    loadRequest.projectId = launch.projectId;
    loadRequest.projectHash = launch.projectHash;
    loadRequest.sourceRevision = launch.scriptSourceRevision;
    loadRequest.sourceSetHash = launch.scriptSourceSetHash;
    loadRequest.referenceManifestHash = launch.scriptReferenceManifest.hash;
    loadRequest.packageGraphHash = launch.packageGraphHash;
    loadRequest.runtimeBridgeLogicalReferenceId = launch.runtimeBridge.logicalReferenceId;
    loadRequest.runtimeBridgeHash = launch.runtimeBridge.assemblyHash;
    loadRequest.runtimeBridgeAbi = launch.runtimeBridge.abi;
    loadRequest.targetFramework = launch.runtimeBridge.targetFramework;
    loadRequest.runtimeCompatibility = launch.runtimeBridge.runtimeCompatibility;
    loadRequest.trust = launch.runtimeBridge.trust;
    const auto package = lifecycle.LoadVerifiedGeneration(loadRequest);
    if (!package.Succeeded())
        return structuredFailure("Runtime.Script.PackageLoadFailed",
            "Verified script assembly could not be loaded.", "ScriptLoadFailure");

    std::map<std::pair<std::string, std::string>, std::string> methods;
    for (const auto& action : namedActions.value("actions", Json::array()))
        if (action.value("publiclyInvocable", false))
            methods.emplace(std::make_pair(action.value("behaviorId", std::string{}),
                action.value("actionId", std::string{})), action.value("method", std::string{}));

    std::map<std::string, std::set<std::string>> behaviorCapabilities;
    std::set<std::string> grantedCapabilities;
    for (const auto& capability : capabilityManifest.value("capabilities", Json::array()))
        if (capability.is_string()) grantedCapabilities.insert(capability.get<std::string>());
    for (const auto& behavior : behaviorManifest.value("behaviors", Json::array()))
    {
        auto& grants = behaviorCapabilities[behavior.value("behaviorId", std::string{})];
        for (const auto& capability : behavior.value("capabilities", Json::array()))
            if (capability.is_string()) grants.insert(capability.get<std::string>());
    }

    std::size_t behaviorCount = 0;
    for (const auto& entity : world.Scene().entities)
    {
        if (!entity.scriptBehavior) continue;
        ++behaviorCount;
        const auto grants = behaviorCapabilities.find(entity.scriptBehavior->behaviorTypeId);
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

    std::map<std::pair<std::string, std::string>, Scripting::ScriptInstanceHandle> instances;
    for (const auto& entity : world.Scene().entities)
    {
        if (!entity.scriptBehavior) continue;
        const auto key = std::make_pair(entity.entityId, entity.scriptBehavior->behaviorTypeId);
        if (instances.contains(key))
            return structuredFailure("Runtime.Script.InstanceIdentityDuplicate",
                "Script instance identity must be unique by entity and behavior type.",
                "ScriptLifecycleFailure");
        Scripting::ScriptInstanceCreateRequest create;
        create.package = package.package;
        create.classId.value = entity.scriptBehavior->behaviorTypeId;
        create.scriptId = entity.scriptBehavior->behaviorTypeId;
        create.selfEntity = world.Handle(entity.entityId);
        const auto created = lifecycle.CreateInstance(create);
        if (!created.Succeeded() || !lifecycle.StartInstance(created.instance).Succeeded())
            return structuredFailure("Runtime.Script.InstanceStartFailed",
                "Script behavior instance failed to start.", "ScriptLifecycleFailure");
        instances.emplace(key, created.instance);
    }

    RuntimeNamedActionTarget target(host, instances, std::move(methods));
    Play::ScriptActionBindingDispatcher dispatcher(target);
    const auto bindingDiagnostics = dispatcher.Build(world.Scene());
    if (!bindingDiagnostics.empty())
        return structuredFailure(bindingDiagnostics.front().code,
            bindingDiagnostics.front().message, "ActionBindingFailure");

    const Play::RuntimeUiPromptBinding* runtimePrompt = nullptr;
    Scripting::ScriptEntityHandle promptEntity;
    std::string promptText;
    if (launch.schemaVersion == 2)
    {
        if (uiResult.value->prompts.size() != 1)
            return structuredFailure("Runtime.UI.PromptBindingInvalid",
                "Exactly one verified runtime prompt is required.", "UiBootstrapFailure");
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
    const auto updateInstances = [&]()
    {
        for (const auto& [identity, instance] : instances)
        {
            (void)identity;
            if (!lifecycle.UpdateInstance(instance, 1.0 / 60.0).Succeeded()) return false;
        }
        return true;
    };
    record();
    bool renderEvidence = false;
    bool gracefulStop = false;
    std::size_t drawCount = 0;
    if (headless)
    {
        if (!publishReady())
            return structuredFailure("Runtime.Startup.ReadyPublishFailed",
                "The session-bound startup-ready record could not be published.",
                "LaunchValidationFailure");
        if (smokeInteractions)
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
            envelope.workingDirectory / "runtime-frame.ppm");
        if (!render.Initialize())
        {
            writeResult("Failed", "RendererFailure", frames, dispatcher.DispatchCount(), Json::object());
            return Failure(envelopeResult.diagnostics.back().code,
                envelopeResult.diagnostics.back().message);
        }
        RuntimeUiSession ui;
        auto* overlay = render.OverlaySubmission();
        if (launch.schemaVersion == 2 && (overlay == nullptr ||
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
        while (!render.ShouldClose())
        {
            if (std::filesystem::exists(envelope.stopPath)) { gracefulStop = true; break; }
            const bool inject = smokeInteractions && (frames == 30 || frames == 190);
            const auto actions = render.PollLogicalActions(*inputResult.value, inject);
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
            }
            if (!updateInstances())
            {
                render.Shutdown();
                return structuredFailure("Runtime.Script.UpdateFailed",
                    "Script OnUpdate failed.", "ScriptLifecycleFailure");
            }
            if (!render.RenderFrame(launch.schemaVersion == 2 ? &ui : nullptr))
            {
                ui.Shutdown();
                render.Shutdown();
                return structuredFailure("Runtime.Render.FrameFailed",
                    "Runtime renderer failed a frame.", "RendererFailure");
            }
            ++frames;
            if (smokeInteractions && frames >= 350) break;
        }
        record();
        renderEvidence = render.HasMeaningfulPixels();
        drawCount = render.LastDrawCount();
        if (renderEvidence)
            envelopeResult.diagnostics.push_back({"Runtime.Render.FrameCaptureEvidence",
                "Renderer produced a non-clear frame capture.", render.CapturePath().string()});
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
    const Json observed = {{"drawCount", drawCount}, {"entityId", observedEntity->entityId},
        {"meaningfulRendererPixels", renderEvidence}, {"positionY", observedHeights},
        {"promptText", observedPrompts},
        {"transition", transitioned ? "ClosedOpenClosed" : "Incomplete"}};
    if ((smokeInteractions && !transitioned) || (!headless && !renderEvidence))
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
