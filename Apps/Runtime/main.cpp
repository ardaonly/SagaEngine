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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
    bool starterArenaSmoke = false;
    std::uint32_t smokeFrames = 30;
    double fixedDtSeconds = 1.0 / 60.0;
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

std::optional<StarterArenaProject> LoadStarterArenaProject(
    const std::filesystem::path& projectInput,
    std::vector<StarterArenaDiagnostic>& diagnostics)
{
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

    ValidateArrayField(root, "scenes", &project.sceneCount, diagnostics);
    ValidateArrayField(root, "assets", nullptr, diagnostics);
    ValidateArrayField(root, "scriptFolders", nullptr, diagnostics);
    ValidateArrayField(root, "launchProfiles", nullptr, diagnostics);
    ValidateArrayField(root, "packageProfiles", nullptr, diagnostics);

    if (!diagnostics.empty())
    {
        return std::nullopt;
    }

    return project;
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

    const bool canRun = diagnostics.empty() && project.has_value();
    double x = 0.0;
    double y = 0.0;
    std::uint32_t clampCount = 0;
    const double minX = -1.0;
    const double maxX = 1.0;
    const double minY = -1.0;
    const double maxY = 1.0;
    const double inputX = 4.0;
    const double inputY = 2.0;

    if (canRun)
    {
        for (std::uint32_t frame = 0; frame < commandLine.smokeFrames; ++frame)
        {
            const double nextX = x + inputX * commandLine.fixedDtSeconds;
            const double nextY = y + inputY * commandLine.fixedDtSeconds;
            const double clampedX = std::min(std::max(nextX, minX), maxX);
            const double clampedY = std::min(std::max(nextY, minY), maxY);
            if (clampedX != nextX || clampedY != nextY)
            {
                ++clampCount;
            }
            x = clampedX;
            y = clampedY;
        }
    }

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
            {"sceneSource", "BuiltInStarterArenaSmoke"}
        }},
        {"settings", {
            {"headless", commandLine.launchOptions.headless},
            {"frames", commandLine.smokeFrames},
            {"fixedDtSeconds", commandLine.fixedDtSeconds}
        }},
        {"loop", {
            {"spawn", {{"x", 0.0}, {"y", 0.0}}},
            {"finalPosition", {{"x", x}, {"y", y}}},
            {"bounds", {{"minX", minX}, {"maxX", maxX}, {"minY", minY}, {"maxY", maxY}}},
            {"inputVector", {{"x", inputX}, {"y", inputY}}},
            {"clampCount", clampCount},
            {"restartBehavior", "Deferred"},
            {"quitBehavior", "DeterministicEndOfSmoke"}
        }},
        {"nonClaims", Json::array({
            "No interactive gameplay proof",
            "No renderer proof",
            "No client or network dependency",
            "No server-authoritative multiplayer",
            "No C# gameplay scripts",
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
        "StarterArena smoke passed: frames=%u final=(%.3f, %.3f) clamps=%u report='%s'",
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
