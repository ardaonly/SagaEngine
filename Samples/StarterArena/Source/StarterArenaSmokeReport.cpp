// SPDX-License-Identifier: Apache-2.0

/// @file StarterArenaSmokeReport.cpp
/// @brief StarterArena smoke JSON report helpers.

#include "StarterArenaSmokeReport.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace SagaRuntimeApp
{
namespace
{

StarterArenaSmokeJson Vec2ToJson(const StarterArenaVec2& value)
{
    return StarterArenaSmokeJson{{"x", value.x}, {"y", value.y}};
}

StarterArenaSmokeJson BoundsToJson(const StarterArenaBounds& bounds)
{
    return StarterArenaSmokeJson{
        {"minX", bounds.minX},
        {"maxX", bounds.maxX},
        {"minY", bounds.minY},
        {"maxY", bounds.maxY}};
}

StarterArenaSmokeJson CameraToJson(const StarterArenaCamera& camera)
{
    return StarterArenaSmokeJson{
        {"mode", camera.mode},
        {"x", camera.x},
        {"y", camera.y},
        {"width", camera.width},
        {"height", camera.height}};
}

StarterArenaSmokeJson ExpectedToJson(const StarterArenaExpected& expected)
{
    return StarterArenaSmokeJson{
        {"finalX", expected.finalX},
        {"finalY", expected.finalY},
        {"clampCount", expected.clampCount}};
}

std::string ScriptBindingExecution(
    const StarterArenaScriptBindingMetadata& metadata)
{
    if (!metadata.provided)
    {
        return "NotProvided";
    }
    return metadata.valid ? "MetadataOnly" : "Invalid";
}

StarterArenaSmokeJson ScriptBindingToJson(
    const StarterArenaScriptBindingMetadata& metadata)
{
    StarterArenaSmokeJson callableMethods = StarterArenaSmokeJson::array();
    for (const std::string& method : metadata.callableMethods)
    {
        callableMethods.push_back(method);
    }
    StarterArenaSmokeJson parameterTypes = StarterArenaSmokeJson::array();
    for (const std::string& parameterType : metadata.parameterTypes)
    {
        parameterTypes.push_back(parameterType);
    }

    return StarterArenaSmokeJson{
        {"status", metadata.provided ? (metadata.valid ? "Passed" : "Failed") : "NotProvided"},
        {"manifestPath", metadata.manifestPath.empty() ? "" : GenericPath(metadata.manifestPath)},
        {"artifactsPath", metadata.artifactsPath.empty() ? "" : GenericPath(metadata.artifactsPath)},
        {"scriptId", metadata.scriptId},
        {"bindingId", metadata.bindingId},
        {"typeName", metadata.typeName},
        {"authority", metadata.authority},
        {"callableMethods", callableMethods},
        {"parameterTypes", parameterTypes},
        {"returnType", metadata.returnType},
        {"artifactId", metadata.artifactId},
        {"targetFramework", metadata.targetFramework},
        {"assemblyPath", metadata.assemblyPath},
        {"runtimeConfigPath", metadata.runtimeConfigPath},
        {"execution", ScriptBindingExecution(metadata)}};
}

StarterArenaSmokeJson ScriptInvocationToJson(
    const StarterArenaScriptInvocationResult& invocation)
{
    StarterArenaSmokeJson arguments = StarterArenaSmokeJson::array();
    for (const std::int32_t argument : invocation.arguments)
    {
        arguments.push_back(argument);
    }

    std::string execution = "NotRequested";
    if (invocation.requested)
    {
        execution = invocation.attempted
            ? (invocation.passed ? "Invoked" : "Failed")
            : "NotExecuted";
    }

    return StarterArenaSmokeJson{
        {"status",
         invocation.requested
             ? (invocation.passed ? "Passed" : "Failed")
             : "NotRequested"},
        {"execution", execution},
        {"mode", invocation.mode},
        {"scriptId", invocation.scriptId},
        {"method", invocation.method},
        {"typeName", invocation.typeName},
        {"arguments", arguments},
        {"expectedResult", invocation.expectedResult},
        {"result", invocation.result},
        {"attempted", invocation.attempted}};
}

StarterArenaSmokeJson ScriptLifecycleToJson(
    const StarterArenaScriptLifecycleResult& lifecycle)
{
    StarterArenaSmokeJson callbacks = StarterArenaSmokeJson::array();
    for (const std::string& callback : lifecycle.callbacksObserved)
    {
        callbacks.push_back(callback);
    }

    std::string execution = "NotRequested";
    if (lifecycle.requested)
    {
        execution = lifecycle.attempted
            ? (lifecycle.passed ? "Invoked" : "Failed")
            : "NotExecuted";
    }

    return StarterArenaSmokeJson{
        {"status",
         lifecycle.requested
             ? (lifecycle.passed ? "Passed" : "Failed")
             : "NotRequested"},
        {"scriptId", lifecycle.scriptId},
        {"typeName", lifecycle.typeName},
        {"execution", execution},
        {"mode", lifecycle.mode},
        {"callbacksObserved", callbacks},
        {"attempted", lifecycle.attempted}};
}

StarterArenaSmokeJson DiagnosticsToJson(
    const std::vector<StarterArenaDiagnostic>& diagnostics)
{
    StarterArenaSmokeJson result = StarterArenaSmokeJson::array();
    for (const StarterArenaDiagnostic& diagnostic : diagnostics)
    {
        result.push_back({
            {"severity", "Error"},
            {"code", diagnostic.code},
            {"message", diagnostic.message},
        });
    }
    return result;
}

StarterArenaSmokeJson GameplayToJson(const StarterArenaGameplayState& state)
{
    const auto mutationValue = [](const StarterArenaGameplayMutation& mutation,
                                  const std::string& value) -> StarterArenaSmokeJson {
        if (mutation.operation == "SetBool") return value == "true";
        if (mutation.operation == "AddInt32") return std::stoi(value);
        return value;
    };
    StarterArenaSmokeJson mutations = StarterArenaSmokeJson::array();
    for (const auto& mutation : state.mutations)
        mutations.push_back({{"tick", mutation.tick}, {"lifecycleEvent", mutation.lifecycleEvent},
                             {"operation", mutation.operation}, {"target", mutation.target},
                             {"before", mutationValue(mutation, mutation.beforeValue)},
                             {"after", mutationValue(mutation, mutation.afterValue)},
                             {"status", mutation.status}, {"diagnostic", mutation.diagnostic}});
    StarterArenaSmokeJson callbacks = StarterArenaSmokeJson::array();
    for (const auto& callback : state.callbacksObserved) callbacks.push_back(callback);
    const auto triggerPosition = state.mutationPosition
        ? state.mutationPosition
        : state.firstReachablePosition;
    return {{"status", state.enabled ? (state.passed ? "Passed" : "Failed") : "NotRequested"},
            {"enabled", state.enabled}, {"source", state.enabled ? "GameRules" : ""},
            {"capability", "Sample.StarterArena.GameplayState"},
            {"initialState", {{"score", 0}, {"pickupCollected", false},
                              {"playerState", "normal"}}},
            {"finalState", {{"score", state.score},
                            {"pickupCollected", state.pickupCollected},
                            {"playerState", state.playerState}}},
            {"expectations", {{"score", 10}, {"pickupCollected", true},
                              {"playerState", "powered"}}},
            {"pickup", {{"id", state.pickup.id},
                        {"position", Vec2ToJson(state.pickup.position)},
                        {"radius", state.pickup.radius},
                        {"scoreValue", state.pickup.scoreValue}}},
            {"trigger", {{"kind", "PlayerOverlapsPickup"},
                         {"firstReachableTick", state.firstReachableTick ? StarterArenaSmokeJson(*state.firstReachableTick) : StarterArenaSmokeJson(nullptr)},
                         {"mutationTick", state.mutationTick ? StarterArenaSmokeJson(*state.mutationTick) : StarterArenaSmokeJson(nullptr)},
                         {"playerPosition", triggerPosition
                            ? Vec2ToJson(*triggerPosition)
                            : StarterArenaSmokeJson(nullptr)}}},
            {"lifecycle", {{"callbacksObserved", callbacks},
                           {"updateCount", state.updateCount}}},
            {"mutations", mutations},
            {"nonClaims", StarterArenaSmokeJson::array({
                "No broad gameplay scripting API claim",
                "No arbitrary world or ECS access claim",
                "No editor or Visual Blocks workflow claim",
                "No networking, replication, or prediction claim",
                "No production readiness claim"})}};
}

StarterArenaSmokeJson BuildSceneReport(
    const StarterArenaScene* scene,
    const StarterArenaLoopResult& loop)
{
    if (scene == nullptr)
    {
        return StarterArenaSmokeJson{
            {"consumed", false},
            {"sceneId", ""},
            {"displayName", ""},
            {"relativePath", ""},
            {"source", ""},
            {"schemaVersion", 0},
            {"camera", StarterArenaSmokeJson::object()},
            {"playerSpawn", Vec2ToJson(loop.spawn)},
            {"bounds", BoundsToJson(loop.bounds)},
            {"expected", ExpectedToJson(loop.expected)}};
    }

    return StarterArenaSmokeJson{
        {"consumed", true},
        {"sceneId", scene->sceneId},
        {"displayName", scene->displayName},
        {"relativePath", GenericPath(scene->relativePath)},
        {"source", GenericPath(scene->relativePath)},
        {"schemaVersion", scene->schemaVersion},
        {"camera", CameraToJson(scene->camera)},
        {"playerSpawn", Vec2ToJson(scene->playerSpawn)},
        {"bounds", BoundsToJson(scene->bounds)},
        {"expected", ExpectedToJson(scene->expected)}};
}

StarterArenaSmokeJson BuildExpectationsReport(
    const StarterArenaLoopResult& loop)
{
    return StarterArenaSmokeJson{
        {"status", loop.canRun ? (loop.expectationsPassed ? "Passed" : "Failed") : "NotRun"},
        {"expected", ExpectedToJson(loop.expected)},
        {"actual", {
            {"finalX", loop.finalPosition.x},
            {"finalY", loop.finalPosition.y},
            {"clampCount", loop.clampCount}
        }}};
}

StarterArenaSmokeJson BuildNonClaims(
    const StarterArenaScriptLifecycleResult& lifecycle)
{
    StarterArenaSmokeJson nonClaims = StarterArenaSmokeJson::array({
        "No interactive gameplay proof",
        "No renderer-visible gameplay proof",
        "No editor workflow proof",
        "No networking or multiplayer proof",
        "No arbitrary C# execution",
        "No Visual Blocks proof",
        "No package install proof",
        "No distribution proof",
        "No production readiness claim"
    });
    if (!lifecycle.passed)
    {
        nonClaims.push_back("No C# script lifecycle execution");
    }
    return nonClaims;
}

} // namespace

StarterArenaSmokeJson BuildStarterArenaSmokeReport(
    const StarterArenaSmokeReportInput& input)
{
    const RuntimeCommandLine& commandLine = input.commandLine;
    const StarterArenaProject* project = input.project;
    const StarterArenaLoopResult& loop = input.loop;

    return StarterArenaSmokeJson{
        {"schemaVersion", 1},
        {"tool", "SagaRuntime"},
        {"command", "starter-arena-smoke"},
        {"status", input.canRun ? "Passed" : "Failed"},
        {"project", {
            {"projectId", project != nullptr ? project->projectId : ""},
            {"displayName", project != nullptr ? project->displayName : ""},
            {"projectPath", project != nullptr ? GenericPath(project->manifestPath) : ""},
            {"projectRoot", project != nullptr ? GenericPath(project->projectRoot) : ""},
            {"manifestPath", project != nullptr ? GenericPath(project->manifestPath) : ""},
            {"sceneReferenceCount", project != nullptr ? project->sceneCount : 0},
            {"sceneSource", input.sceneSource}
        }},
        {"scene", BuildSceneReport(input.scene, loop)},
        {"scriptBinding", ScriptBindingToJson(input.scriptBinding)},
        {"scriptInvocation", ScriptInvocationToJson(input.scriptInvocation)},
        {"scriptLifecycle", ScriptLifecycleToJson(input.scriptLifecycle)},
        {"gameplay", GameplayToJson(input.gameplay)},
        {"settings", {
            {"headless", commandLine.launchOptions.headless},
            {"frames", commandLine.smokeFrames},
            {"fixedDtSeconds", commandLine.fixedDtSeconds}
        }},
        {"loop", {
            {"status", loop.canRun ? (loop.expectationsPassed ? "Passed" : "Failed") : "NotRun"},
            {"frames", commandLine.smokeFrames},
            {"fixedDtSeconds", commandLine.fixedDtSeconds},
            {"spawn", Vec2ToJson(loop.spawn)},
            {"finalPosition", Vec2ToJson(loop.finalPosition)},
            {"bounds", BoundsToJson(loop.bounds)},
            {"inputVector", Vec2ToJson(loop.inputVector)},
            {"clampCount", loop.clampCount},
            {"restartBehavior", "Deferred"},
            {"quitBehavior", "DeterministicEndOfSmoke"}
        }},
        {"expectations", BuildExpectationsReport(loop)},
        {"nonClaims", BuildNonClaims(input.scriptLifecycle)},
        {"diagnostics", DiagnosticsToJson(input.diagnostics)}
    };
}

bool WriteJsonReport(
    const std::filesystem::path& outputPath,
    const StarterArenaSmokeJson& report,
    std::string& error)
{
    if (outputPath.empty())
    {
        error = "--smoke-report-out is required for --starter-arena-smoke.";
        return false;
    }

    std::error_code createError;
    if (!outputPath.parent_path().empty())
    {
        std::filesystem::create_directories(
            outputPath.parent_path(),
            createError);
        if (createError)
        {
            error = "Failed to create smoke report directory: " +
                createError.message();
            return false;
        }
    }

    std::ofstream output(outputPath);
    if (!output)
    {
        error = "Failed to open smoke report: " + GenericPath(outputPath);
        return false;
    }

    output << report.dump(2) << '\n';
    if (!output)
    {
        error = "Failed to write smoke report: " + GenericPath(outputPath);
        return false;
    }

    return true;
}

} // namespace SagaRuntimeApp
