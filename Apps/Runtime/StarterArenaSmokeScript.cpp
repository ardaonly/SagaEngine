/// @file StarterArenaSmokeScript.cpp
/// @brief StarterArena script metadata, invocation, and lifecycle smoke helpers.

#include "StarterArenaSmokeScript.h"

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <SagaEngine/Scripting/ScriptLifecycleService.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SagaRuntimeApp
{
namespace
{

using Json = nlohmann::ordered_json;

std::string JsonStringField(const Json& object, const char* field)
{
    const auto iterator = object.find(field);
    return iterator != object.end() && iterator->is_string()
        ? iterator->get<std::string>()
        : "";
}

bool JsonBoolField(const Json& object, const char* field)
{
    const auto iterator = object.find(field);
    return iterator != object.end() && iterator->is_boolean()
        ? iterator->get<bool>()
        : false;
}

std::optional<Json> LoadJsonObjectFile(
    const std::filesystem::path& path,
    const char* label,
    const char* diagnosticPrefix,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    std::ifstream input(path);
    if (!input)
    {
        diagnostics.push_back({
            std::string(diagnosticPrefix) + ".OpenFailed",
            std::string("Failed to open ") + label + ": " + GenericPath(path)});
        return std::nullopt;
    }

    Json root;
    try
    {
        input >> root;
    }
    catch (const Json::exception& exception)
    {
        diagnostics.push_back({
            std::string(diagnosticPrefix) + ".ParseFailed",
            std::string(label) + " JSON is invalid: " + exception.what()});
        return std::nullopt;
    }

    if (!root.is_object())
    {
        diagnostics.push_back({
            std::string(diagnosticPrefix) + ".RootInvalid",
            std::string(label) + " root must be a JSON object."});
        return std::nullopt;
    }

    return root;
}

bool JsonArrayContainsString(const Json& array, const std::string& expected)
{
    if (!array.is_array())
    {
        return false;
    }

    for (const Json& item : array)
    {
        if (item.is_string() && item.get<std::string>() == expected)
        {
            return true;
        }
    }

    return false;
}

std::filesystem::path ResolveStarterArenaArtifactPath(
    const StarterArenaProject& project,
    const std::string& artifactPath)
{
    if (artifactPath.empty())
    {
        return {};
    }

    const std::filesystem::path path(artifactPath);
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }

    return (project.projectRoot / path).lexically_normal();
}

void AppendScriptHostDiagnostics(
    std::string_view prefix,
    const std::vector<SagaEngine::Scripting::ScriptDiagnostic>& scriptDiagnostics,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    if (scriptDiagnostics.empty())
    {
        diagnostics.push_back({
            std::string(prefix) + ".Unknown",
            "C# script host failed without diagnostics."});
        return;
    }

    for (const auto& diagnostic : scriptDiagnostics)
    {
        std::string message = diagnostic.diagnostic.message;
        if (message.empty())
        {
            message = diagnostic.diagnostic.title;
        }
        diagnostics.push_back({
            std::string(prefix) + "." + diagnostic.diagnostic.code.value,
            message});
    }
}

bool HasScriptLog(
    const std::vector<SagaEngine::Scripting::ScriptLogEvent>& logs,
    std::string_view message)
{
    return std::any_of(
        logs.begin(),
        logs.end(),
        [message](const SagaEngine::Scripting::ScriptLogEvent& event)
        {
            return event.message == message;
        });
}

} // namespace

StarterArenaScriptBindingMetadata LoadStarterArenaScriptBindingMetadata(
    const std::filesystem::path& manifestPath,
    const std::filesystem::path& artifactsPath,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    constexpr const char* kExpectedScriptId = "script://starter-arena/game-rules";
    constexpr const char* kExpectedTypeName = "StarterArena.Scripts.GameRules";
    constexpr const char* kExpectedMethodName = "AddPickupScore";
    constexpr const char* kExpectedAuthority = "SharedPure";
    constexpr const char* kExpectedArtifactId =
        "artifact://scripts/starter-arena/game-rules";

    StarterArenaScriptBindingMetadata metadata;
    metadata.manifestPath = manifestPath;
    metadata.artifactsPath = artifactsPath;

    const bool hasManifest = !manifestPath.empty();
    const bool hasArtifacts = !artifactsPath.empty();
    if (!hasManifest && !hasArtifacts)
    {
        return metadata;
    }

    metadata.provided = true;
    const std::size_t diagnosticStart = diagnostics.size();

    if (hasManifest != hasArtifacts)
    {
        diagnostics.push_back({
            "StarterArena.ScriptBinding.InputIncomplete",
            "--script-manifest and --script-artifacts must be provided together."});
        return metadata;
    }

    const std::optional<Json> bindingsRoot = LoadJsonObjectFile(
        manifestPath,
        "script binding manifest",
        "StarterArena.ScriptBinding.Manifest",
        diagnostics);
    const std::optional<Json> artifactsRoot = LoadJsonObjectFile(
        artifactsPath,
        "script artifacts manifest",
        "StarterArena.ScriptBinding.Artifacts",
        diagnostics);
    if (!bindingsRoot.has_value() || !artifactsRoot.has_value())
    {
        return metadata;
    }

    const auto bindingsIterator = bindingsRoot->find("bindings");
    if (bindingsIterator == bindingsRoot->end() || !bindingsIterator->is_array())
    {
        diagnostics.push_back({
            "StarterArena.ScriptBinding.BindingsInvalid",
            "script_bindings.json must contain a bindings array."});
    }
    else
    {
        std::uint32_t matchingBindingCount = 0;
        for (const Json& binding : *bindingsIterator)
        {
            if (!binding.is_object() ||
                JsonStringField(binding, "scriptId") != kExpectedScriptId)
            {
                continue;
            }

            ++matchingBindingCount;
            metadata.scriptId = JsonStringField(binding, "scriptId");
            metadata.bindingId = JsonStringField(binding, "bindingId");
            metadata.typeName = JsonStringField(binding, "declaringType");
            metadata.authority = JsonStringField(binding, "authority");
            metadata.callableMethods.push_back(JsonStringField(binding, "methodName"));

            const auto parameters = binding.find("parameters");
            if (parameters != binding.end() && parameters->is_array())
            {
                for (const Json& parameter : *parameters)
                {
                    if (parameter.is_object())
                    {
                        metadata.parameterTypes.push_back(
                            JsonStringField(parameter, "type"));
                    }
                }
                metadata.parametersSupported =
                    parameters->size() == 2 &&
                    (*parameters)[0].is_object() &&
                    (*parameters)[1].is_object() &&
                    JsonStringField((*parameters)[0], "type") == "int" &&
                    JsonStringField((*parameters)[1], "type") == "int" &&
                    JsonBoolField((*parameters)[0], "supported") &&
                    JsonBoolField((*parameters)[1], "supported");
            }

            const auto returnType = binding.find("returnType");
            if (returnType != binding.end() && returnType->is_object())
            {
                metadata.returnType = JsonStringField(*returnType, "type");
                metadata.returnSupported =
                    metadata.returnType == "int" &&
                    JsonBoolField(*returnType, "supported");
            }
        }

        if (matchingBindingCount != 1)
        {
            diagnostics.push_back({
                "StarterArena.ScriptBinding.BindingMissing",
                "Expected exactly one GameRules script binding in script_bindings.json."});
        }
        if (metadata.typeName != kExpectedTypeName)
        {
            diagnostics.push_back({
                "StarterArena.ScriptBinding.TypeMismatch",
                "GameRules binding declaring type did not match StarterArena.Scripts.GameRules."});
        }
        if (metadata.callableMethods.size() != 1 ||
            metadata.callableMethods.front() != kExpectedMethodName)
        {
            diagnostics.push_back({
                "StarterArena.ScriptBinding.MethodMismatch",
                "GameRules binding must expose AddPickupScore metadata."});
        }
        if (metadata.authority != kExpectedAuthority)
        {
            diagnostics.push_back({
                "StarterArena.ScriptBinding.AuthorityMismatch",
                "GameRules binding authority must be SharedPure."});
        }
        if (!metadata.parametersSupported)
        {
            diagnostics.push_back({
                "StarterArena.ScriptBinding.ParametersMismatch",
                "GameRules binding must expose two supported int parameters."});
        }
        if (!metadata.returnSupported)
        {
            diagnostics.push_back({
                "StarterArena.ScriptBinding.ReturnMismatch",
                "GameRules binding must expose a supported int return type."});
        }
    }

    metadata.targetFramework = JsonStringField(*artifactsRoot, "targetFramework");
    if (metadata.targetFramework != "net10.0")
    {
        diagnostics.push_back({
            "StarterArena.ScriptBinding.TargetFrameworkMismatch",
            "StarterArena script artifacts must target net10.0."});
    }

    const auto artifactsIterator = artifactsRoot->find("artifacts");
    if (artifactsIterator == artifactsRoot->end() || !artifactsIterator->is_array())
    {
        diagnostics.push_back({
            "StarterArena.ScriptBinding.ArtifactsInvalid",
            "script_artifacts.json must contain an artifacts array."});
    }
    else
    {
        std::uint32_t matchingArtifactCount = 0;
        for (const Json& artifact : *artifactsIterator)
        {
            if (!artifact.is_object() ||
                JsonStringField(artifact, "scriptId") != kExpectedScriptId)
            {
                continue;
            }

            ++matchingArtifactCount;
            metadata.artifactId = JsonStringField(artifact, "artifactId");
            metadata.assemblyPath = JsonStringField(artifact, "assemblyPath");
            metadata.runtimeConfigPath =
                JsonStringField(artifact, "runtimeConfigPath");

            const auto bindingIds = artifact.find("bindingIds");
            if (!metadata.bindingId.empty() &&
                (bindingIds == artifact.end() ||
                 !JsonArrayContainsString(*bindingIds, metadata.bindingId)))
            {
                diagnostics.push_back({
                    "StarterArena.ScriptBinding.BindingIdMissing",
                    "GameRules artifact must reference the GameRules binding id."});
            }
        }

        if (matchingArtifactCount != 1)
        {
            diagnostics.push_back({
                "StarterArena.ScriptBinding.ArtifactMissing",
                "Expected exactly one GameRules script artifact in script_artifacts.json."});
        }
        if (metadata.artifactId != kExpectedArtifactId)
        {
            diagnostics.push_back({
                "StarterArena.ScriptBinding.ArtifactIdMismatch",
                "GameRules artifact id did not match StarterArena expectations."});
        }
        if (metadata.assemblyPath.empty() || metadata.runtimeConfigPath.empty())
        {
            diagnostics.push_back({
                "StarterArena.ScriptBinding.ArtifactPathMissing",
                "GameRules artifact must declare assembly and runtimeconfig paths."});
        }
    }

    metadata.valid = diagnostics.size() == diagnosticStart;
    return metadata;
}

StarterArenaScriptInvocationResult RunStarterArenaScriptInvocation(
    const StarterArenaProject& project,
    const StarterArenaScriptBindingMetadata& metadata,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    constexpr const char* kExpectedMethodName = "AddPickupScore";
    constexpr std::int32_t kLeftArgument = 10;
    constexpr std::int32_t kRightArgument = 5;
    constexpr std::int32_t kExpectedResult = 15;

    StarterArenaScriptInvocationResult invocation;
    invocation.requested = true;
    invocation.mode = "ControlledStarterArenaPureMethod";
    invocation.scriptId = metadata.scriptId;
    invocation.method = kExpectedMethodName;
    invocation.typeName = metadata.typeName;
    invocation.arguments = {kLeftArgument, kRightArgument};
    invocation.expectedResult = kExpectedResult;

    if (!metadata.provided || !metadata.valid)
    {
        diagnostics.push_back({
            "StarterArena.ScriptInvocation.MetadataRequired",
            "Controlled StarterArena script invocation requires valid script metadata."});
        return invocation;
    }

    const std::filesystem::path assemblyPath =
        ResolveStarterArenaArtifactPath(project, metadata.assemblyPath);
    const std::filesystem::path runtimeBridgePath =
        assemblyPath.parent_path() / "SagaScript.RuntimeBridge.dll";
    std::error_code existsError;
    if (assemblyPath.empty() || !std::filesystem::exists(assemblyPath, existsError))
    {
        diagnostics.push_back({
            "StarterArena.ScriptInvocation.AssemblyMissing",
            "StarterArena script assembly was not found: " +
                GenericPath(assemblyPath)});
        return invocation;
    }
    if (!std::filesystem::exists(runtimeBridgePath, existsError))
    {
        diagnostics.push_back({
            "StarterArena.ScriptInvocation.RuntimeBridgeMissing",
            "SagaScript runtime bridge was not found next to the script assembly: " +
                GenericPath(runtimeBridgePath)});
        return invocation;
    }

    SagaEngine::Scripting::CSharpScriptHostOptions hostOptions;
    hostOptions.runtimeBridgeAssembly = runtimeBridgePath;
    SagaEngine::Scripting::CSharpScriptHost host(std::move(hostOptions));

    SagaEngine::Scripting::ScriptPackageLoadRequest loadRequest;
    loadRequest.packageRoot = project.projectRoot;
    loadRequest.scriptArtifactManifest = metadata.artifactsPath;
    loadRequest.packageId = "starter-arena";
    const auto load = host.LoadPackage(loadRequest);
    if (!load.Succeeded())
    {
        AppendScriptHostDiagnostics(
            "StarterArena.ScriptInvocation.LoadPackage",
            load.diagnostics,
            diagnostics);
        return invocation;
    }

    SagaEngine::Scripting::ScriptInstanceCreateRequest createRequest;
    createRequest.package = load.package;
    createRequest.classId.value = metadata.typeName;
    createRequest.scriptId = metadata.scriptId;
    const auto instance = host.CreateInstance(createRequest);
    if (!instance.Succeeded())
    {
        AppendScriptHostDiagnostics(
            "StarterArena.ScriptInvocation.CreateInstance",
            instance.diagnostics,
            diagnostics);
        return invocation;
    }

    SagaEngine::Scripting::CSharpInt32BinaryMethodInvocation methodInvocation;
    methodInvocation.instance = instance.instance;
    methodInvocation.methodName = kExpectedMethodName;
    methodInvocation.left = kLeftArgument;
    methodInvocation.right = kRightArgument;
    invocation.attempted = true;
    const auto invoke = host.InvokeInt32BinaryMethod(methodInvocation);
    invocation.result = invoke.value;
    if (!invoke.Succeeded())
    {
        AppendScriptHostDiagnostics(
            "StarterArena.ScriptInvocation.Invoke",
            invoke.diagnostics,
            diagnostics);
        return invocation;
    }

    if (invocation.result != invocation.expectedResult)
    {
        diagnostics.push_back({
            "StarterArena.ScriptInvocation.ResultMismatch",
            "AddPickupScore(10, 5) did not return 15."});
        return invocation;
    }

    invocation.passed = true;
    return invocation;
}

StarterArenaScriptLifecycleResult RunStarterArenaScriptLifecycle(
    const StarterArenaProject& project,
    const StarterArenaScriptBindingMetadata& metadata,
    const double fixedDtSeconds,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    StarterArenaScriptLifecycleResult lifecycle;
    lifecycle.requested = true;
    lifecycle.mode = "FocusedStarterArenaLifecycle";
    lifecycle.scriptId = metadata.scriptId;
    lifecycle.typeName = metadata.typeName;

    if (!metadata.provided || !metadata.valid)
    {
        diagnostics.push_back({
            "StarterArena.ScriptLifecycle.MetadataRequired",
            "StarterArena script lifecycle smoke requires valid script metadata."});
        return lifecycle;
    }

    const std::filesystem::path assemblyPath =
        ResolveStarterArenaArtifactPath(project, metadata.assemblyPath);
    const std::filesystem::path runtimeBridgePath =
        assemblyPath.parent_path() / "SagaScript.RuntimeBridge.dll";
    std::error_code existsError;
    if (assemblyPath.empty() || !std::filesystem::exists(assemblyPath, existsError))
    {
        diagnostics.push_back({
            "StarterArena.ScriptLifecycle.AssemblyMissing",
            "StarterArena script assembly was not found: " +
                GenericPath(assemblyPath)});
        return lifecycle;
    }
    if (!std::filesystem::exists(runtimeBridgePath, existsError))
    {
        diagnostics.push_back({
            "StarterArena.ScriptLifecycle.RuntimeBridgeMissing",
            "SagaScript runtime bridge was not found next to the script assembly: " +
                GenericPath(runtimeBridgePath)});
        return lifecycle;
    }

    std::vector<SagaEngine::Scripting::ScriptLogEvent> logs;
    SagaEngine::Scripting::CSharpScriptHostOptions hostOptions;
    hostOptions.runtimeBridgeAssembly = runtimeBridgePath;
    hostOptions.logSink =
        [&logs](const SagaEngine::Scripting::ScriptLogEvent& event)
        {
            logs.push_back(event);
        };
    SagaEngine::Scripting::CSharpScriptHost host(std::move(hostOptions));
    SagaEngine::Scripting::ScriptPackageValidationOptions validationOptions;
    validationOptions.expectedPackageDestination = "server";
    SagaEngine::Scripting::ScriptLifecycleService lifecycleService(
        host,
        {},
        validationOptions);

    SagaEngine::Scripting::ScriptPackageLoadRequest loadRequest;
    loadRequest.packageRoot = project.projectRoot;
    loadRequest.scriptArtifactManifest = metadata.artifactsPath;
    loadRequest.packageId = "starter-arena";
    const auto load = lifecycleService.LoadPackage(loadRequest);
    if (!load.Succeeded())
    {
        AppendScriptHostDiagnostics(
            "StarterArena.ScriptLifecycle.LoadFailure",
            load.diagnostics,
            diagnostics);
        return lifecycle;
    }

    SagaEngine::Scripting::ScriptInstanceCreateRequest createRequest;
    createRequest.package = load.package;
    createRequest.classId.value = metadata.typeName;
    createRequest.scriptId = metadata.scriptId;
    lifecycle.attempted = true;
    const auto instance = lifecycleService.CreateInstance(createRequest);
    if (!instance.Succeeded())
    {
        AppendScriptHostDiagnostics(
            "StarterArena.ScriptLifecycle.CreateFailure",
            instance.diagnostics,
            diagnostics);
        return lifecycle;
    }

    const auto start = lifecycleService.StartInstance(instance.instance);
    if (!start.Succeeded())
    {
        AppendScriptHostDiagnostics(
            "StarterArena.ScriptLifecycle.StartFailure",
            start.diagnostics,
            diagnostics);
        return lifecycle;
    }

    const auto update =
        lifecycleService.UpdateInstance(instance.instance, fixedDtSeconds);
    if (!update.Succeeded())
    {
        AppendScriptHostDiagnostics(
            "StarterArena.ScriptLifecycle.UpdateFailure",
            update.diagnostics,
            diagnostics);
        return lifecycle;
    }

    const auto destroy = lifecycleService.DestroyInstance(instance.instance);
    if (!destroy.Succeeded())
    {
        AppendScriptHostDiagnostics(
            "StarterArena.ScriptLifecycle.DestroyFailure",
            destroy.diagnostics,
            diagnostics);
        return lifecycle;
    }

    const std::vector<std::pair<std::string_view, std::string_view>> expected = {
        {"OnCreate", "StarterArena.GameRules.OnCreate"},
        {"OnStart", "StarterArena.GameRules.OnStart"},
        {"OnUpdate", "StarterArena.GameRules.OnUpdate"},
        {"OnDestroy", "StarterArena.GameRules.OnDestroy"}};
    for (const auto& [callback, message] : expected)
    {
        if (HasScriptLog(logs, message))
        {
            lifecycle.callbacksObserved.emplace_back(callback);
            continue;
        }

        diagnostics.push_back({
            "StarterArena.ScriptLifecycle.CallbackMissing",
            "StarterArena lifecycle smoke did not observe " +
                std::string(callback) + "."});
    }

    lifecycle.passed = lifecycle.callbacksObserved.size() == expected.size();
    return lifecycle;
}

} // namespace SagaRuntimeApp
