/// @file main.cpp
/// @brief Temporary SagaRuntime adapter over the current client host.
///
/// v0.0.8 ships a role-based runtime binary while the runtime core is still
/// being separated from the older client entrypoint. Keep editor/tooling
/// dependencies out of this adapter; Phase 2 should split Runtime Core from
/// optional client UI semantics.

#include "ClientHost.h"
#include "RuntimeAssetStartupBootstrap.hpp"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Platform/PlatformFactory.h>
#include <SagaEngine/Resources/AssetRegistry.h>

#include <SagaRuntime/RuntimeServiceRegistry.hpp>
#include <SagaRuntime/RuntimeServiceRegistryDiagnostics.hpp>
#include <SagaRuntime/RuntimeStartupDiagnostics.hpp>
#include <SagaRuntime/RuntimeStartupSession.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr const char* kLogTag = "Runtime";
using Json = nlohmann::ordered_json;

struct RuntimeCommandLine
{
    SagaRuntime::RuntimeLaunchOptions launchOptions;
    std::filesystem::path projectPath;
    std::filesystem::path smokeReportOut;
    std::filesystem::path scriptManifestPath;
    std::filesystem::path scriptArtifactsPath;
    bool starterArenaSmoke = false;
    std::uint32_t smokeFrames = 30;
    double fixedDtSeconds = 1.0 / 60.0;
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
    std::string artifactId;
    std::string targetFramework;
    std::string assemblyPath;
    std::string runtimeConfigPath;
};

struct StarterArenaDiagnostic
{
    std::string code;
    std::string message;
};

class RuntimeBootstrapService final : public SagaRuntime::IRuntimeService
{
public:
    RuntimeBootstrapService()
    {
        m_descriptor.serviceId = "runtime.app.bootstrap";
        m_descriptor.displayName = "Runtime App Bootstrap";
        m_descriptor.category = "app";
    }

    const SagaRuntime::RuntimeServiceDescriptor& Descriptor() const noexcept override
    {
        return m_descriptor;
    }

    SagaRuntime::RuntimeServiceLifecycleResult Start() override
    {
        return SagaRuntime::RuntimeServiceLifecycle::Transition(
            m_descriptor,
            SagaRuntime::RuntimeServiceState::Registered,
            SagaRuntime::RuntimeServiceState::Started);
    }

    SagaRuntime::RuntimeServiceLifecycleResult Tick() override
    {
        SagaRuntime::RuntimeServiceLifecycleResult result;
        result.descriptor = m_descriptor;
        result.state = SagaRuntime::RuntimeServiceState::Started;
        return result;
    }

    SagaRuntime::RuntimeServiceLifecycleResult Stop() override
    {
        return SagaRuntime::RuntimeServiceLifecycle::Transition(
            m_descriptor,
            SagaRuntime::RuntimeServiceState::Started,
            SagaRuntime::RuntimeServiceState::Stopped);
    }

private:
    SagaRuntime::RuntimeServiceDescriptor m_descriptor;
};

void LogStartupDiagnostic(
    const SagaRuntime::RuntimeStartupDiagnosticView& diagnostic)
{
    const std::string manifestPath = diagnostic.manifestPath.string();
    const std::string resolvedPath =
        diagnostic.resolvedPath.has_value() ? diagnostic.resolvedPath->string() : "";

    LOG_ERROR(kLogTag,
              "Startup package validation failed: %s: %s (manifest='%s')",
              diagnostic.diagnosticId.c_str(),
              diagnostic.message.c_str(),
              manifestPath.c_str());
    if (!resolvedPath.empty())
    {
        LOG_ERROR(kLogTag, "Startup diagnostic resolved path: %s", resolvedPath.c_str());
    }
}

void LogAssetBootstrapDiagnostic(
    const SagaRuntime::RuntimeAssetBootstrapDiagnosticView& diagnostic)
{
    const std::string manifestPath = diagnostic.manifestPath.string();
    const std::string resolvedPath =
        diagnostic.resolvedPath.has_value() ? diagnostic.resolvedPath->string() : "";

    LOG_ERROR(kLogTag,
              "Startup asset bootstrap failed: %s: %s (manifest='%s')",
              diagnostic.diagnosticId.c_str(),
              diagnostic.message.c_str(),
              manifestPath.c_str());
    if (!resolvedPath.empty())
    {
        LOG_ERROR(kLogTag,
                  "Startup asset bootstrap resolved path: %s",
                  resolvedPath.c_str());
    }
}

const char* ToRuntimeServiceRegistryStateName(
    SagaRuntime::RuntimeServiceRegistryReportState state) noexcept
{
    switch (state)
    {
        case SagaRuntime::RuntimeServiceRegistryReportState::Ready:
            return "ready";
        case SagaRuntime::RuntimeServiceRegistryReportState::Blocked:
            return "blocked";
        case SagaRuntime::RuntimeServiceRegistryReportState::Idle:
            return "idle";
    }

    return "unknown";
}

void LogRuntimeServiceDiagnostic(
    const SagaRuntime::RuntimeServiceDiagnosticView& diagnostic)
{
    switch (diagnostic.severity)
    {
        case SagaRuntime::RuntimeServiceDiagnosticSeverity::Error:
            LOG_ERROR(kLogTag,
                      "Runtime service diagnostic: %s: %s",
                      diagnostic.serviceId.c_str(),
                      diagnostic.message.c_str());
            return;
        case SagaRuntime::RuntimeServiceDiagnosticSeverity::Warning:
            LOG_WARN(kLogTag,
                     "Runtime service diagnostic: %s: %s",
                     diagnostic.serviceId.c_str(),
                     diagnostic.message.c_str());
            return;
        case SagaRuntime::RuntimeServiceDiagnosticSeverity::Info:
            LOG_INFO(kLogTag,
                     "Runtime service diagnostic: %s: %s",
                     diagnostic.serviceId.c_str(),
                     diagnostic.message.c_str());
            return;
    }
}

void LogRuntimeServiceRegistryReport(
    const char* operationName,
    const SagaRuntime::RuntimeServiceRegistryReport& report)
{
    const auto summary =
        SagaRuntime::RuntimeServiceRegistryDiagnostics::Summarize(report);

    if (summary.state == SagaRuntime::RuntimeServiceRegistryReportState::Blocked)
    {
        LOG_ERROR(kLogTag,
                  "Runtime service %s blocked: services=%zu diagnostics=%zu errors=%zu warnings=%zu",
                  operationName,
                  summary.serviceResultCount,
                  summary.diagnosticCount,
                  summary.errorDiagnosticCount,
                  summary.warningDiagnosticCount);
    }
    else
    {
        LOG_INFO(kLogTag,
                 "Runtime service %s %s: services=%zu diagnostics=%zu",
                 operationName,
                 ToRuntimeServiceRegistryStateName(summary.state),
                 summary.serviceResultCount,
                 summary.diagnosticCount);
    }

    for (const SagaRuntime::RuntimeServiceDiagnosticView& diagnostic :
         SagaRuntime::RuntimeServiceRegistryDiagnostics::BuildDiagnosticViews(report))
    {
        LogRuntimeServiceDiagnostic(diagnostic);
    }
}

SagaRuntime::RuntimeServiceRegistryReport ReportForResult(
    SagaRuntime::RuntimeServiceLifecycleResult result)
{
    SagaRuntime::RuntimeServiceRegistryReport report;
    for (const SagaRuntime::RuntimeServiceDiagnostic& diagnostic :
         result.diagnostics)
    {
        report.diagnostics.push_back(diagnostic);
    }
    report.results.push_back(std::move(result));
    return report;
}

bool StartRuntimeServices(SagaRuntime::RuntimeServiceRegistry& registry)
{
    SagaRuntime::RuntimeServiceLifecycleResult registration =
        registry.Register(std::make_unique<RuntimeBootstrapService>());
    if (!registration.Succeeded())
    {
        LogRuntimeServiceRegistryReport(
            "registration",
            ReportForResult(std::move(registration)));
        return false;
    }

    SagaRuntime::RuntimeServiceRegistryReport startReport = registry.StartAll();
    LogRuntimeServiceRegistryReport("startup", startReport);
    return startReport.Succeeded();
}

void StopRuntimeServices(SagaRuntime::RuntimeServiceRegistry& registry)
{
    SagaRuntime::RuntimeServiceRegistryReport stopReport = registry.StopAll();
    LogRuntimeServiceRegistryReport("shutdown", stopReport);
}

Saga::ClientConfig ToClientConfig(
    const SagaRuntime::RuntimeSessionDescriptor& descriptor)
{
    Saga::ClientConfig config;
    config.serverHost = descriptor.serverHost;
    config.serverPort = descriptor.serverPort;
    config.headless = descriptor.headless;
    config.enableRuntimeUi = descriptor.enableStartupUi;
    config.uiContentRoot = descriptor.startupContentRoot;
    return config;
}

SagaRuntime::RuntimeStartupReport PrepareRuntimeStartup(
    const SagaRuntime::RuntimeLaunchOptions& launchOptions)
{
    auto report = SagaRuntime::RuntimeStartupSession::Prepare(launchOptions);
    const auto summary = SagaRuntime::RuntimeStartupDiagnostics::Summarize(report);
    if (summary.state == SagaRuntime::RuntimeStartupReportState::PreflightSkipped)
    {
        LOG_INFO(kLogTag,
                 "No --package-manifest supplied; skipping startup package validation for dev compatibility.");
        return report;
    }

    if (summary.state == SagaRuntime::RuntimeStartupReportState::Ready)
    {
        if (report.sessionDescriptor.packageManifestPath.has_value())
        {
            LOG_INFO(kLogTag,
                     "Startup package validation accepted '%s'.",
                     report.sessionDescriptor.packageManifestPath->string().c_str());
        }
        else
        {
            LOG_INFO(kLogTag, "Startup package validation accepted.");
        }
        return report;
    }

    for (const auto& diagnostic :
         SagaRuntime::RuntimeStartupDiagnostics::BuildDiagnosticViews(report))
    {
        LogStartupDiagnostic(diagnostic);
    }

    return report;
}

bool BootstrapRuntimeAssets(
    const SagaRuntime::RuntimeSessionDescriptor& descriptor,
    SagaEngine::Resources::AssetRegistry& assetRegistry)
{
    const auto result =
        SagaRuntimeApp::RuntimeAssetStartupBootstrap::Bootstrap(
            descriptor,
            assetRegistry);

    if (result.bootstrapSkipped)
    {
        LOG_INFO(kLogTag,
                 "No --package-manifest supplied; skipping startup asset bootstrap for dev compatibility.");
        return true;
    }

    if (result.Succeeded())
    {
        LOG_INFO(kLogTag,
                 "Startup asset bootstrap accepted '%s': registeredAssets=%zu.",
                 result.summary.packageManifestPath.string().c_str(),
                 result.summary.registeredAssetCount);
        return true;
    }

    LOG_ERROR(kLogTag,
              "Startup asset bootstrap blocked: package='%s' diagnostics=%zu errors=%zu.",
              result.summary.packageManifestPath.string().c_str(),
              result.summary.diagnosticCount,
              result.summary.errorCount);
    for (const SagaRuntime::RuntimeAssetBootstrapDiagnosticView& diagnostic :
         result.diagnostics)
    {
        LogAssetBootstrapDiagnostic(diagnostic);
    }
    return false;
}

std::string GenericPath(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

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

bool NearlyEqual(double left, double right)
{
    return std::fabs(left - right) <= 0.000001;
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
        reference.absolutePath = (projectRoot / relative).lexically_normal();
        reference.kind = kind;

        if (!std::filesystem::is_regular_file(reference.absolutePath))
        {
            diagnostics.push_back({
                "StarterArena.Project.ScenePathMissing",
                "StarterArena scene reference is missing: " +
                    GenericPath(reference.relativePath)});
            ok = false;
        }

        scenes.push_back(std::move(reference));
    }

    return ok;
}

std::optional<StarterArenaProject> LoadStarterArenaProject(
    const std::filesystem::path& projectInput,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
    const std::size_t diagnosticStart = diagnostics.size();
    const std::optional<std::filesystem::path> manifest =
        ResolveProjectManifest(projectInput, diagnostics);
    if (!manifest.has_value())
    {
        return std::nullopt;
    }

    std::ifstream input(*manifest);
    if (!input)
    {
        diagnostics.push_back({
            "StarterArena.Project.OpenFailed",
            "Failed to open project manifest: " + GenericPath(*manifest)});
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
            "StarterArena.Project.ParseFailed",
            std::string("Project manifest JSON is invalid: ") +
                exception.what()});
        return std::nullopt;
    }

    if (!root.is_object())
    {
        diagnostics.push_back({
            "StarterArena.Project.RootInvalid",
            "Project manifest root must be a JSON object."});
        return std::nullopt;
    }

    const auto schemaVersion = root.find("schemaVersion");
    if (schemaVersion == root.end() || !schemaVersion->is_number_integer() ||
        schemaVersion->get<int>() != 0)
    {
        diagnostics.push_back({
            "StarterArena.Project.SchemaVersionUnsupported",
            "StarterArena smoke requires .sagaproj schemaVersion 0."});
    }

    StarterArenaProject project;
    project.manifestPath = *manifest;
    project.projectRoot = manifest->parent_path();
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

    const auto paths = root.find("paths");
    if (paths == root.end() || !paths->is_object())
    {
        diagnostics.push_back({
            "StarterArena.Project.PathsInvalid",
            "Project manifest field 'paths' must be an object."});
    }
    else
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

    ReadSceneReferences(
        root,
        project.projectRoot,
        project.scenes,
        diagnostics);
    project.sceneCount = static_cast<std::uint32_t>(project.scenes.size());
    ValidateArrayField(root, "assets", nullptr, diagnostics);
    ValidateArrayField(root, "scriptFolders", nullptr, diagnostics);
    ValidateArrayField(root, "launchProfiles", nullptr, diagnostics);
    ValidateArrayField(root, "packageProfiles", nullptr, diagnostics);

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
                GenericPath(reference.relativePath)});
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

    if (!reference.id.empty() && !scene.sceneId.empty() &&
        scene.sceneId != reference.id)
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

Json Vec2ToJson(const StarterArenaVec2& value)
{
    return Json{{"x", value.x}, {"y", value.y}};
}

Json BoundsToJson(const StarterArenaBounds& bounds)
{
    return Json{
        {"minX", bounds.minX},
        {"maxX", bounds.maxX},
        {"minY", bounds.minY},
        {"maxY", bounds.maxY}};
}

Json CameraToJson(const StarterArenaCamera& camera)
{
    return Json{
        {"mode", camera.mode},
        {"x", camera.x},
        {"y", camera.y},
        {"width", camera.width},
        {"height", camera.height}};
}

Json ExpectedToJson(const StarterArenaExpected& expected)
{
    return Json{
        {"finalX", expected.finalX},
        {"finalY", expected.finalY},
        {"clampCount", expected.clampCount}};
}

std::string JsonStringField(const Json& object, const char* field)
{
    const auto iterator = object.find(field);
    return iterator != object.end() && iterator->is_string()
        ? iterator->get<std::string>()
        : "";
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

Json ScriptBindingToJson(const StarterArenaScriptBindingMetadata& metadata)
{
    Json callableMethods = Json::array();
    for (const std::string& method : metadata.callableMethods)
    {
        callableMethods.push_back(method);
    }

    return Json{
        {"status", metadata.provided ? (metadata.valid ? "Passed" : "Failed") : "NotProvided"},
        {"manifestPath", metadata.manifestPath.empty() ? "" : GenericPath(metadata.manifestPath)},
        {"artifactsPath", metadata.artifactsPath.empty() ? "" : GenericPath(metadata.artifactsPath)},
        {"scriptId", metadata.scriptId},
        {"bindingId", metadata.bindingId},
        {"typeName", metadata.typeName},
        {"authority", metadata.authority},
        {"callableMethods", callableMethods},
        {"artifactId", metadata.artifactId},
        {"targetFramework", metadata.targetFramework},
        {"assemblyPath", metadata.assemblyPath},
        {"runtimeConfigPath", metadata.runtimeConfigPath},
        {"execution", "NotExecuted"}};
}

Json DiagnosticsToJson(const std::vector<StarterArenaDiagnostic>& diagnostics)
{
    Json result = Json::array();
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

bool WriteJsonReport(
    const std::filesystem::path& outputPath,
    const Json& report,
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

    const StarterArenaVec2 spawn =
        scene.has_value() ? scene->playerSpawn : StarterArenaVec2{};
    const StarterArenaBounds bounds =
        scene.has_value() ? scene->bounds : StarterArenaBounds{};
    const StarterArenaVec2 testInput =
        scene.has_value() ? scene->testInput : StarterArenaVec2{};
    const StarterArenaExpected expected =
        scene.has_value() ? scene->expected : StarterArenaExpected{};

    double x = spawn.x;
    double y = spawn.y;
    std::uint32_t clampCount = 0;
    const bool loopCanRun =
        diagnostics.empty() && project.has_value() && scene.has_value();

    if (loopCanRun)
    {
        for (std::uint32_t frame = 0; frame < commandLine.smokeFrames; ++frame)
        {
            const double nextX = x + testInput.x * commandLine.fixedDtSeconds;
            const double nextY = y + testInput.y * commandLine.fixedDtSeconds;
            const double clampedX =
                std::clamp(nextX, bounds.minX, bounds.maxX);
            const double clampedY =
                std::clamp(nextY, bounds.minY, bounds.maxY);
            if (clampedX != nextX || clampedY != nextY)
            {
                ++clampCount;
            }
            x = clampedX;
            y = clampedY;
        }
    }

    const bool expectationsPassed =
        loopCanRun &&
        NearlyEqual(x, expected.finalX) &&
        NearlyEqual(y, expected.finalY) &&
        clampCount == expected.clampCount;
    if (loopCanRun && !expectationsPassed)
    {
        diagnostics.push_back({
            "StarterArena.Smoke.ExpectedMismatch",
            "StarterArena smoke result did not match scene expected values."});
    }

    const bool scriptBindingPassed =
        !scriptBinding.provided || scriptBinding.valid;
    const bool canRun =
        loopCanRun && expectationsPassed && scriptBindingPassed &&
        diagnostics.empty();
    const std::string sceneSource = scene.has_value()
        ? "ProjectSceneReference"
        : "ProjectSceneReferenceMissing";
    const Json sceneReport = scene.has_value()
        ? Json{
            {"consumed", true},
            {"sceneId", scene->sceneId},
            {"displayName", scene->displayName},
            {"relativePath", GenericPath(scene->relativePath)},
            {"schemaVersion", scene->schemaVersion},
            {"camera", CameraToJson(scene->camera)}}
        : Json{
            {"consumed", false},
            {"sceneId", ""},
            {"displayName", ""},
            {"relativePath", ""},
            {"schemaVersion", 0},
            {"camera", Json::object()}};
    const Json expectationsReport = {
        {"status", loopCanRun ? (expectationsPassed ? "Passed" : "Failed") : "NotRun"},
        {"expected", ExpectedToJson(expected)},
        {"actual", {
            {"finalX", x},
            {"finalY", y},
            {"clampCount", clampCount}
        }}};

    Json report = {
        {"schemaVersion", 1},
        {"tool", "SagaRuntime"},
        {"command", "starter-arena-smoke"},
        {"status", canRun ? "Passed" : "Failed"},
        {"project", {
            {"projectId", project.has_value() ? project->projectId : ""},
            {"displayName", project.has_value() ? project->displayName : ""},
            {"projectRoot", project.has_value() ? GenericPath(project->projectRoot) : ""},
            {"manifestPath", project.has_value() ? GenericPath(project->manifestPath) : ""},
            {"sceneReferenceCount", project.has_value() ? project->sceneCount : 0},
            {"sceneSource", sceneSource}
        }},
        {"scene", sceneReport},
        {"scriptBinding", ScriptBindingToJson(scriptBinding)},
        {"settings", {
            {"headless", commandLine.launchOptions.headless},
            {"frames", commandLine.smokeFrames},
            {"fixedDtSeconds", commandLine.fixedDtSeconds}
        }},
        {"loop", {
            {"spawn", Vec2ToJson(spawn)},
            {"finalPosition", {{"x", x}, {"y", y}}},
            {"bounds", BoundsToJson(bounds)},
            {"inputVector", Vec2ToJson(testInput)},
            {"clampCount", clampCount},
            {"restartBehavior", "Deferred"},
            {"quitBehavior", "DeterministicEndOfSmoke"}
        }},
        {"expectations", expectationsReport},
        {"nonClaims", Json::array({
            "No interactive gameplay proof",
            "No renderer proof",
            "No client or network dependency",
            "No server-authoritative multiplayer",
            "No C# script execution",
            "No runtime C# gameplay binding",
            "No Visual Blocks",
            "No editor workflow",
            "No package or distribution output"
        })},
        {"diagnostics", DiagnosticsToJson(diagnostics)}
    };

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
        "StarterArena scene smoke passed: scene='%s' scriptBinding='%s' frames=%u final=(%.3f, %.3f) clamps=%u report='%s'",
        scene.has_value() ? scene->sceneId.c_str() : "",
        scriptBinding.provided ? (scriptBinding.valid ? "Passed" : "Failed") : "NotProvided",
        commandLine.smokeFrames,
        x,
        y,
        clampCount,
        commandLine.smokeReportOut.string().c_str());
    return 0;
}

} // namespace

// ─── Command-line parsing ─────────────────────────────────────────────────────

static RuntimeCommandLine ParseArgs(int argc, char* argv[])
{
    RuntimeCommandLine commandLine;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if ((arg == "--server" || arg == "-s") && i + 1 < argc)
        {
            commandLine.launchOptions.serverHost = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            commandLine.launchOptions.serverPort =
                static_cast<std::uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--headless" || arg == "-h")
        {
            commandLine.launchOptions.headless = true;
        }
        else if (arg == "--package-manifest" && i + 1 < argc)
        {
            commandLine.launchOptions.packageManifestPath =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--package-base-directory" && i + 1 < argc)
        {
            commandLine.launchOptions.packageBaseDirectory =
                std::filesystem::path(argv[++i]);
        }
        else if (arg == "--project" && i + 1 < argc)
        {
            commandLine.projectPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--starter-arena-smoke")
        {
            commandLine.starterArenaSmoke = true;
        }
        else if (arg == "--script-manifest" && i + 1 < argc)
        {
            commandLine.scriptManifestPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--script-artifacts" && i + 1 < argc)
        {
            commandLine.scriptArtifactsPath = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--smoke-report-out" && i + 1 < argc)
        {
            commandLine.smokeReportOut = std::filesystem::path(argv[++i]);
        }
        else if (arg == "--smoke-frames" && i + 1 < argc)
        {
            commandLine.smokeFrames =
                static_cast<std::uint32_t>(std::stoul(argv[++i]));
        }
        else if (arg == "--fixed-dt" && i + 1 < argc)
        {
            commandLine.fixedDtSeconds = std::stod(argv[++i]);
        }
        else if (arg == "--help" || arg == "--?")
        {
            std::printf("Usage: SagaRuntime [options]\n"
                        "  --server, -s <host>   Server address (default: 127.0.0.1)\n"
                        "  --port <port>         Server port (default: 7777)\n"
                        "  --headless, -h        Run without window / renderer\n"
                        "  --package-manifest <path>\n"
                        "                         Validate startup package manifest before initialization\n"
                        "  --package-base-directory <path>\n"
                        "                         Resolve package-relative manifest and asset paths from this directory\n"
                        "  --project <path>      .sagaproj path for bounded sample smoke modes\n"
                        "  --starter-arena-smoke Run bounded local StarterArena smoke and exit\n"
                        "  --script-manifest <path>\n"
                        "                         Optional StarterArena script_bindings.json smoke input\n"
                        "  --script-artifacts <path>\n"
                        "                         Optional StarterArena script_artifacts.json smoke input\n"
                        "  --smoke-report-out <path>\n"
                        "                         Write StarterArena smoke report JSON\n"
                        "  --smoke-frames <n>    StarterArena smoke frame count (default: 30)\n"
                        "  --fixed-dt <seconds>  StarterArena fixed timestep (default: 0.0166667)\n"
                        "  --help, --?           Show this help\n");
            std::exit(0);
        }
    }

    return commandLine;
}

// ─── Entry point ──────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    SagaEngine::Core::Log::Init();

    auto commandLine = ParseArgs(argc, argv);
    if (commandLine.starterArenaSmoke)
    {
        const int result = RunStarterArenaSmoke(commandLine);
        SagaEngine::Core::Log::Shutdown();
        return result;
    }

    auto startupReport = PrepareRuntimeStartup(commandLine.launchOptions);
    if (!startupReport.Succeeded())
    {
        SagaEngine::Core::Log::Shutdown();
        return 1;
    }

    SagaEngine::Resources::AssetRegistry assetRegistry;
    if (!BootstrapRuntimeAssets(
            startupReport.sessionDescriptor,
            assetRegistry))
    {
        SagaEngine::Core::Log::Shutdown();
        return 1;
    }

    SagaRuntime::RuntimeServiceRegistry runtimeServices;
    if (!StartRuntimeServices(runtimeServices))
    {
        SagaEngine::Core::Log::Shutdown();
        return 1;
    }

    auto platformFactory = Saga::CreateSDLPlatformFactory();
    Saga::PlatformFactory::Set(platformFactory.get());

    Saga::ClientHost host(ToClientConfig(startupReport.sessionDescriptor));
    host.Run();

    StopRuntimeServices(runtimeServices);

    SagaEngine::Core::Log::Shutdown();
    return 0;
}
