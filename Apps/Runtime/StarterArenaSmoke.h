/// @file StarterArenaSmoke.h
/// @brief StarterArena headless runtime smoke contract.

#pragma once

#include "RuntimeCommandLine.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaRuntimeApp
{

struct StarterArenaDiagnostic
{
    std::string code;
    std::string message;
};

struct StarterArenaSceneReference
{
    std::string id;
    std::filesystem::path relativePath;
    std::filesystem::path absolutePath;
    std::string kind;
};

struct StarterArenaProject
{
    std::string projectId;
    std::string displayName;
    std::filesystem::path projectRoot;
    std::filesystem::path manifestPath;
    std::filesystem::path diagnosticsPath;
    std::filesystem::path generatedReportsPath;
    std::uint32_t sceneCount = 0;
    std::vector<StarterArenaSceneReference> scenes;
};

struct StarterArenaVec2
{
    double x = 0.0;
    double y = 0.0;
};

struct StarterArenaBounds
{
    double minX = -1.0;
    double maxX = 1.0;
    double minY = -1.0;
    double maxY = 1.0;
};

struct StarterArenaCamera
{
    std::string mode;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct StarterArenaExpected
{
    double finalX = 0.0;
    double finalY = 0.0;
    std::uint32_t clampCount = 0;
};

struct StarterArenaScene
{
    std::uint32_t schemaVersion = 0;
    std::string sceneId;
    std::string displayName;
    std::filesystem::path relativePath;
    std::filesystem::path absolutePath;
    StarterArenaVec2 playerSpawn;
    StarterArenaBounds bounds;
    StarterArenaCamera camera;
    StarterArenaVec2 testInput;
    StarterArenaExpected expected;
};

struct StarterArenaScriptBindingMetadata
{
    bool provided = false;
    bool valid = false;
    std::filesystem::path manifestPath;
    std::filesystem::path artifactsPath;
    std::string scriptId;
    std::string bindingId;
    std::string typeName;
    std::string authority;
    std::vector<std::string> callableMethods;
    std::vector<std::string> parameterTypes;
    std::string returnType;
    bool parametersSupported = false;
    bool returnSupported = false;
    std::string artifactId;
    std::string targetFramework;
    std::string assemblyPath;
    std::string runtimeConfigPath;
};

struct StarterArenaScriptInvocationResult
{
    bool requested = false;
    bool attempted = false;
    bool passed = false;
    std::string mode;
    std::string scriptId;
    std::string method;
    std::string typeName;
    std::vector<std::int32_t> arguments;
    std::int32_t expectedResult = 15;
    std::int32_t result = 0;
};

struct StarterArenaScriptLifecycleResult
{
    bool requested = false;
    bool attempted = false;
    bool passed = false;
    std::string mode;
    std::string scriptId;
    std::string typeName;
    std::vector<std::string> callbacksObserved;
};

struct StarterArenaLoopResult
{
    bool canRun = false;
    bool expectationsPassed = false;
    StarterArenaVec2 spawn;
    StarterArenaBounds bounds;
    StarterArenaVec2 inputVector;
    StarterArenaExpected expected;
    StarterArenaVec2 finalPosition;
    std::uint32_t clampCount = 0;
};

[[nodiscard]] std::string GenericPath(const std::filesystem::path& path);
[[nodiscard]] bool NearlyEqual(double left, double right);

[[nodiscard]] std::optional<StarterArenaProject> LoadStarterArenaProject(
    const std::filesystem::path& input,
    std::vector<StarterArenaDiagnostic>& diagnostics);
[[nodiscard]] std::optional<StarterArenaScene> LoadStarterArenaScene(
    const StarterArenaProject& project,
    std::vector<StarterArenaDiagnostic>& diagnostics);

[[nodiscard]] int RunStarterArenaSmoke(const RuntimeCommandLine& commandLine);

} // namespace SagaRuntimeApp
