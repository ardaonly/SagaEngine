/// @file StarterArenaSmoke.cpp
/// @brief StarterArena headless runtime smoke contract.

#include "StarterArenaSmoke.h"

#include "StarterArenaSmokeReport.h"
#include "StarterArenaSmokeScript.h"

#include <SagaEngine/Core/Log/Log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace SagaRuntimeApp
{
namespace
{

constexpr const char* kLogTag = "Runtime";
using Json = nlohmann::ordered_json;

bool IsSafeRelativePath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute())
    {
        return false;
    }

    for (const std::filesystem::path& part : path)
    {
        if (part == "..")
        {
            return false;
        }
    }

    return true;
}

bool ReadRequiredStringField(
    const Json& object,
    const char* field,
    const std::string& qualifiedField,
    std::string& value,
    std::vector<StarterArenaDiagnostic>& diagnostics,
    const char* diagnosticCode)
{
    const auto iterator = object.find(field);
    if (iterator == object.end() || !iterator->is_string())
    {
        diagnostics.push_back({
            diagnosticCode,
            "Scene field '" + qualifiedField + "' must be a string."});
        return false;
    }

    value = iterator->get<std::string>();
    if (value.empty())
    {
        diagnostics.push_back({
            diagnosticCode,
            "Scene field '" + qualifiedField + "' must not be empty."});
        return false;
    }

    return true;
}

const Json* FindObjectField(
    const Json& object,
    const char* field,
    const std::string& qualifiedField,
    std::vector<StarterArenaDiagnostic>& diagnostics,
    const char* diagnosticCode)
{
    const auto iterator = object.find(field);
    if (iterator == object.end() || !iterator->is_object())
    {
        diagnostics.push_back({
            diagnosticCode,
            "Scene field '" + qualifiedField + "' must be an object."});
        return nullptr;
    }

    return &(*iterator);
}

bool ReadNumberField(
    const Json& object,
    const char* field,
    const std::string& qualifiedField,
    double& value,
    std::vector<StarterArenaDiagnostic>& diagnostics,
    const char* diagnosticCode)
{
    const auto iterator = object.find(field);
    if (iterator == object.end() || !iterator->is_number())
    {
        diagnostics.push_back({
            diagnosticCode,
            "Scene field '" + qualifiedField + "' must be a number."});
        return false;
    }

    value = iterator->get<double>();
    return true;
}

bool ReadUintField(
    const Json& object,
    const char* field,
    const std::string& qualifiedField,
    std::uint32_t& value,
    std::vector<StarterArenaDiagnostic>& diagnostics,
    const char* diagnosticCode)
{
    const auto iterator = object.find(field);
    if (iterator == object.end() ||
        (!iterator->is_number_integer() && !iterator->is_number_unsigned()))
    {
        diagnostics.push_back({
            diagnosticCode,
            "Scene field '" + qualifiedField + "' must be a non-negative integer."});
        return false;
    }

    if (iterator->is_number_unsigned())
    {
        const std::uint64_t raw = iterator->get<std::uint64_t>();
        if (raw > std::numeric_limits<std::uint32_t>::max())
        {
            diagnostics.push_back({
                diagnosticCode,
                "Scene field '" + qualifiedField + "' is too large."});
            return false;
        }
        value = static_cast<std::uint32_t>(raw);
        return true;
    }

    const std::int64_t raw = iterator->get<std::int64_t>();
    if (raw < 0 ||
        raw > static_cast<std::int64_t>(
            std::numeric_limits<std::uint32_t>::max()))
    {
        diagnostics.push_back({
            diagnosticCode,
            "Scene field '" + qualifiedField + "' must be a non-negative integer."});
        return false;
    }

    value = static_cast<std::uint32_t>(raw);
    return true;
}

bool ReadVec2Field(
    const Json& object,
    const char* field,
    const std::string& qualifiedField,
    StarterArenaVec2& value,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    const Json* vectorObject = FindObjectField(
        object,
        field,
        qualifiedField,
        diagnostics,
        "StarterArena.Scene.VectorInvalid");
    if (vectorObject == nullptr)
    {
        return false;
    }

    bool ok = true;
    ok &= ReadNumberField(
        *vectorObject,
        "x",
        qualifiedField + ".x",
        value.x,
        diagnostics,
        "StarterArena.Scene.VectorInvalid");
    ok &= ReadNumberField(
        *vectorObject,
        "y",
        qualifiedField + ".y",
        value.y,
        diagnostics,
        "StarterArena.Scene.VectorInvalid");
    return ok;
}

bool ReadBoundsField(
    const Json& object,
    StarterArenaBounds& bounds,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    const Json* boundsObject = FindObjectField(
        object,
        "bounds",
        "starterArenaSmoke.bounds",
        diagnostics,
        "StarterArena.Scene.BoundsInvalid");
    if (boundsObject == nullptr)
    {
        return false;
    }

    bool ok = true;
    ok &= ReadNumberField(
        *boundsObject,
        "minX",
        "starterArenaSmoke.bounds.minX",
        bounds.minX,
        diagnostics,
        "StarterArena.Scene.BoundsInvalid");
    ok &= ReadNumberField(
        *boundsObject,
        "maxX",
        "starterArenaSmoke.bounds.maxX",
        bounds.maxX,
        diagnostics,
        "StarterArena.Scene.BoundsInvalid");
    ok &= ReadNumberField(
        *boundsObject,
        "minY",
        "starterArenaSmoke.bounds.minY",
        bounds.minY,
        diagnostics,
        "StarterArena.Scene.BoundsInvalid");
    ok &= ReadNumberField(
        *boundsObject,
        "maxY",
        "starterArenaSmoke.bounds.maxY",
        bounds.maxY,
        diagnostics,
        "StarterArena.Scene.BoundsInvalid");

    if (ok && (bounds.minX >= bounds.maxX || bounds.minY >= bounds.maxY))
    {
        diagnostics.push_back({
            "StarterArena.Scene.BoundsInvalid",
            "Scene smoke bounds minimums must be smaller than maximums."});
        ok = false;
    }

    return ok;
}

bool ReadCameraField(
    const Json& object,
    StarterArenaCamera& camera,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    const Json* cameraObject = FindObjectField(
        object,
        "camera",
        "starterArenaSmoke.camera",
        diagnostics,
        "StarterArena.Scene.CameraInvalid");
    if (cameraObject == nullptr)
    {
        return false;
    }

    bool ok = true;
    ok &= ReadRequiredStringField(
        *cameraObject,
        "mode",
        "starterArenaSmoke.camera.mode",
        camera.mode,
        diagnostics,
        "StarterArena.Scene.CameraInvalid");
    ok &= ReadNumberField(
        *cameraObject,
        "x",
        "starterArenaSmoke.camera.x",
        camera.x,
        diagnostics,
        "StarterArena.Scene.CameraInvalid");
    ok &= ReadNumberField(
        *cameraObject,
        "y",
        "starterArenaSmoke.camera.y",
        camera.y,
        diagnostics,
        "StarterArena.Scene.CameraInvalid");
    ok &= ReadNumberField(
        *cameraObject,
        "width",
        "starterArenaSmoke.camera.width",
        camera.width,
        diagnostics,
        "StarterArena.Scene.CameraInvalid");
    ok &= ReadNumberField(
        *cameraObject,
        "height",
        "starterArenaSmoke.camera.height",
        camera.height,
        diagnostics,
        "StarterArena.Scene.CameraInvalid");

    if (ok && camera.mode != "fixed")
    {
        diagnostics.push_back({
            "StarterArena.Scene.CameraInvalid",
            "StarterArena smoke only supports fixed camera metadata."});
        ok = false;
    }
    if (ok && (camera.width <= 0.0 || camera.height <= 0.0))
    {
        diagnostics.push_back({
            "StarterArena.Scene.CameraInvalid",
            "Scene smoke camera width and height must be greater than zero."});
        ok = false;
    }

    return ok;
}

bool ReadExpectedField(
    const Json& object,
    StarterArenaExpected& expected,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    const Json* expectedObject = FindObjectField(
        object,
        "expected",
        "starterArenaSmoke.expected",
        diagnostics,
        "StarterArena.Scene.ExpectedInvalid");
    if (expectedObject == nullptr)
    {
        return false;
    }

    bool ok = true;
    ok &= ReadNumberField(
        *expectedObject,
        "finalX",
        "starterArenaSmoke.expected.finalX",
        expected.finalX,
        diagnostics,
        "StarterArena.Scene.ExpectedInvalid");
    ok &= ReadNumberField(
        *expectedObject,
        "finalY",
        "starterArenaSmoke.expected.finalY",
        expected.finalY,
        diagnostics,
        "StarterArena.Scene.ExpectedInvalid");
    ok &= ReadUintField(
        *expectedObject,
        "clampCount",
        "starterArenaSmoke.expected.clampCount",
        expected.clampCount,
        diagnostics,
        "StarterArena.Scene.ExpectedInvalid");
    return ok;
}

std::optional<std::filesystem::path> ResolveProjectManifest(
    const std::filesystem::path& input,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    if (input.empty())
    {
        diagnostics.push_back({
            "StarterArena.Project.MissingInput",
            "--project is required for --starter-arena-smoke."});
        return std::nullopt;
    }

    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(input, error).lexically_normal();
    if (error)
    {
        diagnostics.push_back({
            "StarterArena.Project.ResolveFailed",
            "Failed to resolve project path: " + error.message()});
        return std::nullopt;
    }

    if (std::filesystem::is_regular_file(absolute, error))
    {
        return absolute;
    }

    if (!std::filesystem::is_directory(absolute, error))
    {
        diagnostics.push_back({
            "StarterArena.Project.NotFound",
            "Project path does not exist: " + GenericPath(absolute)});
        return std::nullopt;
    }

    std::vector<std::filesystem::path> manifests;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(absolute, error))
    {
        if (error)
        {
            diagnostics.push_back({
                "StarterArena.Project.ScanFailed",
                "Failed to scan project directory: " + error.message()});
            return std::nullopt;
        }

        if (entry.is_regular_file(error) &&
            entry.path().extension() == ".sagaproj")
        {
            manifests.push_back(entry.path());
        }
    }

    if (manifests.size() != 1)
    {
        diagnostics.push_back({
            manifests.empty()
                ? "StarterArena.Project.ManifestMissing"
                : "StarterArena.Project.ManifestAmbiguous",
            manifests.empty()
                ? "Project directory does not contain a .sagaproj manifest."
                : "Project directory contains more than one .sagaproj manifest."});
        return std::nullopt;
    }

    return manifests.front().lexically_normal();
}

bool ReadStringField(
    const Json& object,
    const char* field,
    std::string& value,
    std::vector<StarterArenaDiagnostic>& diagnostics,
    const char* diagnosticCode)
{
    const auto iterator = object.find(field);
    if (iterator == object.end() || !iterator->is_string())
    {
        diagnostics.push_back({
            diagnosticCode,
            std::string("Project manifest field '") + field +
                "' must be a string."});
        return false;
    }

    value = iterator->get<std::string>();
    if (value.empty())
    {
        diagnostics.push_back({
            diagnosticCode,
            std::string("Project manifest field '") + field +
                "' must not be empty."});
        return false;
    }

    return true;
}

bool ReadProjectPathField(
    const Json& paths,
    const char* field,
    const std::filesystem::path& projectRoot,
    std::filesystem::path& value,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    std::string raw;
    if (!ReadStringField(
            paths,
            field,
            raw,
            diagnostics,
            "StarterArena.Project.PathInvalid"))
    {
        return false;
    }

    const std::filesystem::path relative(raw);
    if (!IsSafeRelativePath(relative))
    {
        diagnostics.push_back({
            "StarterArena.Project.PathUnsafe",
            std::string("Project path '") + field +
                "' must be project-relative and must not contain parent traversal."});
        return false;
    }

    value = (projectRoot / relative).lexically_normal();
    if (!std::filesystem::is_directory(value))
    {
        diagnostics.push_back({
            "StarterArena.Project.PathMissing",
            std::string("Project path '") + field +
                "' must resolve to an existing directory."});
        return false;
    }

    return true;
}

bool ValidateArrayField(
    const Json& object,
    const char* field,
    std::uint32_t* count,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    const auto iterator = object.find(field);
    if (iterator == object.end() || !iterator->is_array())
    {
        diagnostics.push_back({
            "StarterArena.Project.ArrayInvalid",
            std::string("Project manifest field '") + field +
                "' must be an array."});
        return false;
    }

    if (count != nullptr)
    {
        *count = static_cast<std::uint32_t>(iterator->size());
    }
    return true;
}

bool ReadSceneReferences(
    const Json& object,
    const std::filesystem::path& projectRoot,
    std::vector<StarterArenaSceneReference>& scenes,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    const auto iterator = object.find("scenes");
    if (iterator == object.end() || !iterator->is_array())
    {
        diagnostics.push_back({
            "StarterArena.Project.ArrayInvalid",
            "Project manifest field 'scenes' must be an array."});
        return false;
    }

    bool ok = true;
    for (std::size_t index = 0; index < iterator->size(); ++index)
    {
        const Json& item = (*iterator)[index];
        if (!item.is_object())
        {
            diagnostics.push_back({
                "StarterArena.Project.SceneReferenceInvalid",
                "Project manifest scene references must be objects."});
            ok = false;
            continue;
        }

        std::string id;
        std::string rawPath;
        std::string kind;
        ok &= ReadStringField(
            item,
            "id",
            id,
            diagnostics,
            "StarterArena.Project.SceneReferenceInvalid");
        ok &= ReadStringField(
            item,
            "path",
            rawPath,
            diagnostics,
            "StarterArena.Project.SceneReferenceInvalid");
        ok &= ReadStringField(
            item,
            "kind",
            kind,
            diagnostics,
            "StarterArena.Project.SceneReferenceInvalid");

        if (kind != "scene")
        {
            diagnostics.push_back({
                "StarterArena.Project.SceneKindInvalid",
                "StarterArena smoke scene reference kind must be 'scene'."});
            ok = false;
        }

        const std::filesystem::path relative(rawPath);
        if (!IsSafeRelativePath(relative))
        {
            diagnostics.push_back({
                "StarterArena.Project.ScenePathUnsafe",
                "StarterArena scene paths must be project-relative and must not contain parent traversal."});
            ok = false;
            continue;
        }

        StarterArenaSceneReference reference;
        reference.id = id;
        reference.relativePath = relative.lexically_normal();
        reference.absolutePath = (projectRoot / reference.relativePath).lexically_normal();
        reference.kind = kind;

        if (!std::filesystem::is_regular_file(reference.absolutePath))
        {
            diagnostics.push_back({
                "StarterArena.Project.ScenePathMissing",
                "StarterArena scene reference is missing: " +
                    GenericPath(reference.absolutePath)});
            ok = false;
        }

        scenes.push_back(std::move(reference));
    }

    return ok;
}

std::optional<StarterArenaProject> LoadStarterArenaProject(
    const std::filesystem::path& input,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    const std::size_t diagnosticStart = diagnostics.size();
    const std::optional<std::filesystem::path> manifestPath =
        ResolveProjectManifest(input, diagnostics);
    if (!manifestPath.has_value())
    {
        return std::nullopt;
    }

    std::ifstream inputFile(*manifestPath);
    if (!inputFile)
    {
        diagnostics.push_back({
            "StarterArena.Project.OpenFailed",
            "Failed to open StarterArena project: " + GenericPath(*manifestPath)});
        return std::nullopt;
    }

    Json root;
    try
    {
        inputFile >> root;
    }
    catch (const Json::exception& exception)
    {
        diagnostics.push_back({
            "StarterArena.Project.ParseFailed",
            std::string("StarterArena project JSON is invalid: ") +
                exception.what()});
        return std::nullopt;
    }

    if (!root.is_object())
    {
        diagnostics.push_back({
            "StarterArena.Project.RootInvalid",
            "StarterArena project root must be a JSON object."});
        return std::nullopt;
    }

    std::uint32_t schemaVersion = 0;
    ReadUintField(
        root,
        "schemaVersion",
        "schemaVersion",
        schemaVersion,
        diagnostics,
        "StarterArena.Project.SchemaVersionUnsupported");
    if (schemaVersion != 0)
    {
        diagnostics.push_back({
            "StarterArena.Project.SchemaVersionUnsupported",
            "StarterArena smoke requires .sagaproj schemaVersion 0."});
    }

    StarterArenaProject project;
    project.projectRoot = manifestPath->parent_path().lexically_normal();
    project.manifestPath = *manifestPath;

    ReadStringField(
        root,
        "projectId",
        project.projectId,
        diagnostics,
        "StarterArena.Project.ProjectIdInvalid");
    ReadStringField(
        root,
        "displayName",
        project.displayName,
        diagnostics,
        "StarterArena.Project.DisplayNameInvalid");

    if (project.projectId != "starter-arena")
    {
        diagnostics.push_back({
            "StarterArena.Project.UnsupportedProject",
            "StarterArena smoke only accepts projectId 'starter-arena'."});
    }

    const Json* paths = FindObjectField(
        root,
        "paths",
        "paths",
        diagnostics,
        "StarterArena.Project.PathsInvalid");
    if (paths != nullptr)
    {
        ReadProjectPathField(
            *paths,
            "diagnostics",
            project.projectRoot,
            project.diagnosticsPath,
            diagnostics);
        ReadProjectPathField(
            *paths,
            "generatedReports",
            project.projectRoot,
            project.generatedReportsPath,
            diagnostics);
    }

    ValidateArrayField(root, "assets", nullptr, diagnostics);
    ValidateArrayField(root, "scriptFolders", nullptr, diagnostics);
    ValidateArrayField(root, "launchProfiles", nullptr, diagnostics);
    ValidateArrayField(root, "packageProfiles", nullptr, diagnostics);
    ReadSceneReferences(root, project.projectRoot, project.scenes, diagnostics);
    project.sceneCount = static_cast<std::uint32_t>(project.scenes.size());

    if (diagnostics.size() != diagnosticStart)
    {
        return std::nullopt;
    }

    return project;
}

std::optional<StarterArenaScene> LoadStarterArenaScene(
    const StarterArenaProject& project,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    const std::size_t diagnosticStart = diagnostics.size();
    if (project.scenes.size() != 1)
    {
        diagnostics.push_back({
            "StarterArena.Scene.ReferenceCountInvalid",
            "StarterArena smoke requires exactly one project scene reference."});
        return std::nullopt;
    }

    const StarterArenaSceneReference& reference = project.scenes.front();
    std::ifstream input(reference.absolutePath);
    if (!input)
    {
        diagnostics.push_back({
            "StarterArena.Scene.OpenFailed",
            "Failed to open StarterArena scene: " +
                GenericPath(reference.absolutePath)});
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
            "StarterArena.Scene.ParseFailed",
            std::string("StarterArena scene JSON is invalid: ") +
                exception.what()});
        return std::nullopt;
    }

    if (!root.is_object())
    {
        diagnostics.push_back({
            "StarterArena.Scene.RootInvalid",
            "StarterArena scene root must be a JSON object."});
        return std::nullopt;
    }

    StarterArenaScene scene;
    scene.relativePath = reference.relativePath;
    scene.absolutePath = reference.absolutePath;

    ReadUintField(
        root,
        "schemaVersion",
        "schemaVersion",
        scene.schemaVersion,
        diagnostics,
        "StarterArena.Scene.SchemaVersionUnsupported");
    if (scene.schemaVersion != 1)
    {
        diagnostics.push_back({
            "StarterArena.Scene.SchemaVersionUnsupported",
            "StarterArena smoke requires scene schemaVersion 1."});
    }
    ReadRequiredStringField(
        root,
        "sceneId",
        "sceneId",
        scene.sceneId,
        diagnostics,
        "StarterArena.Scene.SceneIdInvalid");
    ReadRequiredStringField(
        root,
        "displayName",
        "displayName",
        scene.displayName,
        diagnostics,
        "StarterArena.Scene.DisplayNameInvalid");

    std::string sourceKind;
    ReadRequiredStringField(
        root,
        "sourceKind",
        "sourceKind",
        sourceKind,
        diagnostics,
        "StarterArena.Scene.SourceKindInvalid");
    if (sourceKind != "SceneSourceTruth")
    {
        diagnostics.push_back({
            "StarterArena.Scene.SourceKindInvalid",
            "StarterArena scene must declare sourceKind SceneSourceTruth."});
    }

    if (scene.sceneId != reference.id)
    {
        diagnostics.push_back({
            "StarterArena.Scene.SceneIdMismatch",
            "StarterArena sceneId must match the project scene reference id."});
    }

    const auto entities = root.find("entities");
    if (entities == root.end() || !entities->is_array() || entities->empty())
    {
        diagnostics.push_back({
            "StarterArena.Scene.EntitiesInvalid",
            "StarterArena scene source truth must declare at least one entity."});
    }

    const Json* smokeObject = FindObjectField(
        root,
        "starterArenaSmoke",
        "starterArenaSmoke",
        diagnostics,
        "StarterArena.Scene.SmokeMetadataMissing");
    if (smokeObject != nullptr)
    {
        ReadVec2Field(
            *smokeObject,
            "playerSpawn",
            "starterArenaSmoke.playerSpawn",
            scene.playerSpawn,
            diagnostics);
        ReadBoundsField(*smokeObject, scene.bounds, diagnostics);
        ReadCameraField(*smokeObject, scene.camera, diagnostics);
        ReadVec2Field(
            *smokeObject,
            "testInput",
            "starterArenaSmoke.testInput",
            scene.testInput,
            diagnostics);
        ReadExpectedField(*smokeObject, scene.expected, diagnostics);
    }

    if (diagnostics.size() != diagnosticStart)
    {
        return std::nullopt;
    }

    return scene;
}

StarterArenaLoopResult RunStarterArenaLoop(
    const RuntimeCommandLine& commandLine,
    const std::optional<StarterArenaProject>& project,
    const std::optional<StarterArenaScene>& scene,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    StarterArenaLoopResult loop;
    loop.spawn = scene.has_value() ? scene->playerSpawn : StarterArenaVec2{};
    loop.bounds = scene.has_value() ? scene->bounds : StarterArenaBounds{};
    loop.inputVector = scene.has_value() ? scene->testInput : StarterArenaVec2{};
    loop.expected = scene.has_value() ? scene->expected : StarterArenaExpected{};
    loop.finalPosition = loop.spawn;
    loop.canRun = diagnostics.empty() && project.has_value() && scene.has_value();

    if (loop.canRun)
    {
        for (std::uint32_t frame = 0; frame < commandLine.smokeFrames; ++frame)
        {
            const double nextX =
                loop.finalPosition.x +
                loop.inputVector.x * commandLine.fixedDtSeconds;
            const double nextY =
                loop.finalPosition.y +
                loop.inputVector.y * commandLine.fixedDtSeconds;
            const double clampedX =
                std::clamp(nextX, loop.bounds.minX, loop.bounds.maxX);
            const double clampedY =
                std::clamp(nextY, loop.bounds.minY, loop.bounds.maxY);
            if (clampedX != nextX || clampedY != nextY)
            {
                ++loop.clampCount;
            }
            loop.finalPosition.x = clampedX;
            loop.finalPosition.y = clampedY;
        }
    }

    loop.expectationsPassed =
        loop.canRun &&
        NearlyEqual(loop.finalPosition.x, loop.expected.finalX) &&
        NearlyEqual(loop.finalPosition.y, loop.expected.finalY) &&
        loop.clampCount == loop.expected.clampCount;
    if (loop.canRun && !loop.expectationsPassed)
    {
        diagnostics.push_back({
            "StarterArena.Smoke.ExpectedMismatch",
            "StarterArena smoke result did not match scene expected values."});
    }

    return loop;
}

} // namespace

std::string GenericPath(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

bool NearlyEqual(double left, double right)
{
    return std::fabs(left - right) <= 0.000001;
}

int RunStarterArenaSmoke(const RuntimeCommandLine& commandLine)
{
    std::vector<StarterArenaDiagnostic> diagnostics;
    if (!commandLine.launchOptions.headless)
    {
        diagnostics.push_back({
            "StarterArena.Smoke.HeadlessRequired",
            "StarterArena smoke must be launched with --headless."});
    }
    if (commandLine.smokeFrames == 0)
    {
        diagnostics.push_back({
            "StarterArena.Smoke.FrameCountInvalid",
            "--smoke-frames must be greater than zero."});
    }
    if (commandLine.fixedDtSeconds <= 0.0)
    {
        diagnostics.push_back({
            "StarterArena.Smoke.FixedDtInvalid",
            "--fixed-dt must be greater than zero."});
    }

    const std::optional<StarterArenaProject> project =
        LoadStarterArenaProject(commandLine.projectPath, diagnostics);
    std::optional<StarterArenaScene> scene;
    if (project.has_value())
    {
        scene = LoadStarterArenaScene(*project, diagnostics);
    }

    const StarterArenaScriptBindingMetadata scriptBinding =
        LoadStarterArenaScriptBindingMetadata(
            commandLine.scriptManifestPath,
            commandLine.scriptArtifactsPath,
            diagnostics);
    StarterArenaScriptInvocationResult scriptInvocation;
    StarterArenaScriptLifecycleResult scriptLifecycle;
    if (commandLine.invokeStarterArenaScript)
    {
        if (project.has_value())
        {
            scriptInvocation = RunStarterArenaScriptInvocation(
                *project,
                scriptBinding,
                diagnostics);
        }
        else
        {
            scriptInvocation.requested = true;
            scriptInvocation.mode = "ControlledStarterArenaPureMethod";
            scriptInvocation.method = "AddPickupScore";
            scriptInvocation.arguments = {10, 5};
            diagnostics.push_back({
                "StarterArena.ScriptInvocation.ProjectRequired",
                "Controlled StarterArena script invocation requires a valid project."});
        }
    }
    if (commandLine.runStarterArenaScriptLifecycle)
    {
        if (project.has_value())
        {
            scriptLifecycle = RunStarterArenaScriptLifecycle(
                *project,
                scriptBinding,
                commandLine.fixedDtSeconds,
                diagnostics);
        }
        else
        {
            scriptLifecycle.requested = true;
            scriptLifecycle.mode = "FocusedStarterArenaLifecycle";
            diagnostics.push_back({
                "StarterArena.ScriptLifecycle.ProjectRequired",
                "StarterArena script lifecycle smoke requires a valid project."});
        }
    }

    StarterArenaLoopResult loop =
        RunStarterArenaLoop(commandLine, project, scene, diagnostics);
    const bool scriptBindingPassed =
        !scriptBinding.provided || scriptBinding.valid;
    const bool scriptInvocationPassed =
        !scriptInvocation.requested || scriptInvocation.passed;
    const bool scriptLifecyclePassed =
        !scriptLifecycle.requested || scriptLifecycle.passed;
    const bool canRun =
        loop.canRun && loop.expectationsPassed && scriptBindingPassed &&
        scriptInvocationPassed && scriptLifecyclePassed && diagnostics.empty();
    const std::string sceneSource = scene.has_value()
        ? "ProjectSceneReference"
        : "ProjectSceneReferenceMissing";

    const StarterArenaSmokeJson report = BuildStarterArenaSmokeReport({
        commandLine,
        project.has_value() ? &(*project) : nullptr,
        scene.has_value() ? &(*scene) : nullptr,
        scriptBinding,
        scriptInvocation,
        scriptLifecycle,
        loop,
        diagnostics,
        canRun,
        sceneSource});

    std::string error;
    if (!WriteJsonReport(commandLine.smokeReportOut, report, error))
    {
        LOG_ERROR(kLogTag, "%s", error.c_str());
        return 1;
    }

    if (!canRun)
    {
        for (const StarterArenaDiagnostic& diagnostic : diagnostics)
        {
            LOG_ERROR(
                kLogTag,
                "StarterArena smoke failed: %s: %s",
                diagnostic.code.c_str(),
                diagnostic.message.c_str());
        }
        return 1;
    }

    LOG_INFO(
        kLogTag,
        "StarterArena scene smoke passed: scene='%s' scriptBinding='%s' scriptInvocation='%s' scriptLifecycle='%s' frames=%u final=(%.3f, %.3f) clamps=%u report='%s'",
        scene.has_value() ? scene->sceneId.c_str() : "",
        scriptBinding.provided ? (scriptBinding.valid ? "Passed" : "Failed") : "NotProvided",
        scriptInvocation.requested ? (scriptInvocation.passed ? "Passed" : "Failed") : "NotRequested",
        scriptLifecycle.requested ? (scriptLifecycle.passed ? "Passed" : "Failed") : "NotRequested",
        commandLine.smokeFrames,
        loop.finalPosition.x,
        loop.finalPosition.y,
        loop.clampCount,
        commandLine.smokeReportOut.string().c_str());
    return 0;
}

} // namespace SagaRuntimeApp
