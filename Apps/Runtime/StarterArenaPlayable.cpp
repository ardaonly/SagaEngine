/// @file StarterArenaPlayable.cpp
/// @brief Visible StarterArena runtime host and evidence report.

#include "StarterArenaPlayable.h"

#include "StarterArenaPlayableScene.h"
#include "StarterArenaSimulation.h"
#include "StarterArenaSmoke.h"

#include <SagaEngine/Core/Application/Application.h>
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Platform/IWindow.h>
#include <SagaEngine/Render/Backend/RenderBackendFactory.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
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

constexpr const char* kLogTag = "StarterArenaPlayable";
using Json = nlohmann::ordered_json;
namespace RB = SagaEngine::Render::Backend;

struct PlayableEvidence
{
    bool initialized = false;
    bool sceneReady = false;
    bool playerDrawSubmitted = false;
    bool shutdownComplete = false;
    std::string backend = "Unavailable";
    std::uint32_t viewportWidth = 0;
    std::uint32_t viewportHeight = 0;
    std::uint32_t requestedFrames = 0;
    std::uint32_t presentedFrames = 0;
    std::uint32_t resizeCount = 0;
    std::uint64_t totalDrawItems = 0;
    std::uint32_t lastFrameDrawItems = 0;
    std::uint32_t lastVisitedEntities = 0;
    std::uint32_t lastCulledEntities = 0;
    StarterArenaSimulationState simulation;
};

bool IsPathInside(const std::filesystem::path& child, const std::filesystem::path& parent)
{
    if (child.empty() || parent.empty())
    {
        return false;
    }
    const auto normalizedChild = std::filesystem::absolute(child).lexically_normal();
    const auto normalizedParent = std::filesystem::absolute(parent).lexically_normal();
    auto childPart = normalizedChild.begin();
    for (auto parentPart = normalizedParent.begin(); parentPart != normalizedParent.end();
         ++parentPart, ++childPart)
    {
        if (childPart == normalizedChild.end() || *childPart != *parentPart)
        {
            return false;
        }
    }
    return true;
}

Json Vec2Json(StarterArenaVec2 value)
{
    return {{"x", value.x}, {"y", value.y}};
}

Json BoundsJson(const StarterArenaBounds& bounds)
{
    return {
        {"minX", bounds.minX}, {"maxX", bounds.maxX}, {"minY", bounds.minY}, {"maxY", bounds.maxY}};
}

class StarterArenaPlayableHost final : public Saga::Application
{
public:
    StarterArenaPlayableHost(const StarterArenaScene& scene,
                             const RuntimeCommandLine& commandLine,
                             PlayableEvidence& evidence,
                             std::vector<StarterArenaDiagnostic>& diagnostics)
        : Application("StarterArena"), scene_(scene), commandLine_(commandLine),
          evidence_(evidence), diagnostics_(diagnostics)
    {
        evidence_.requestedFrames = commandLine.playableFrames;
        evidence_.simulation = MakeStarterArenaSimulation(scene);
    }

private:
    void Fail(std::string code, std::string message)
    {
        diagnostics_.push_back({std::move(code), std::move(message)});
        RequestClose();
    }

    void OnInit() override
    {
        Saga::IWindow* window = GetWindow();
        if (window == nullptr)
        {
            Fail("StarterArena.Playable.WindowMissing",
                 "Visible StarterArena mode requires an initialized window.");
            return;
        }
        evidence_.viewportWidth = window->GetWidth();
        evidence_.viewportHeight = window->GetHeight();
        void* nativeWindow = window->GetOSNativeHandle();
        if (nativeWindow == nullptr)
        {
            Fail("StarterArena.Playable.NativeWindowMissing",
                 "The render backend could not obtain an OS-native window handle.");
            return;
        }

        RB::RenderBackendConfig backendConfig;
        backendConfig.clearColor[0] = 0.035f;
        backendConfig.clearColor[1] = 0.045f;
        backendConfig.clearColor[2] = 0.055f;
        backend_ = RB::CreateRenderBackend(backendConfig);
        if (!backend_)
        {
            Fail("StarterArena.Playable.BackendMissing",
                 "The configured render backend could not be created.");
            return;
        }

        RB::SwapchainDesc swapchain;
        swapchain.nativeWindow = nativeWindow;
        swapchain.width = evidence_.viewportWidth;
        swapchain.height = evidence_.viewportHeight;
        swapchain.vsync = true;
        if (!backend_->Initialize(swapchain))
        {
            Fail("StarterArena.Playable.BackendInitializationFailed",
                 "The render backend failed to initialize the StarterArena swapchain.");
            return;
        }
        evidence_.initialized = true;
        const RB::RenderBackendStatus status = RB::GetRenderBackendStatus(*backend_);
        evidence_.backend = std::string(RB::ToString(status.selectedAPI));

        window->SetRHIOwnsPresent(true);
        window->SetOnResize([this](std::uint32_t width, std::uint32_t height) {
            if (!backend_ || width == 0u || height == 0u)
            {
                return;
            }
            backend_->OnResize(width, height);
            evidence_.viewportWidth = width;
            evidence_.viewportHeight = height;
            ++evidence_.resizeCount;
            playableScene_.camera = MakeStarterArenaPlayableCamera(scene_, width, height);
        });

        playableScene_ = CreateStarterArenaPlayableScene(
            *backend_, renderWorld_, scene_, evidence_.viewportWidth, evidence_.viewportHeight);
        if (!playableScene_.IsValid())
        {
            Fail("StarterArena.Playable.ResourceCreationFailed",
                 "StarterArena mesh, material, texture, or entity creation failed.");
            return;
        }
        evidence_.sceneReady = true;
    }

    void OnUpdate() override
    {
        if (!evidence_.sceneReady || !backend_)
        {
            RequestClose();
            return;
        }

        StepStarterArenaSimulation(evidence_.simulation,
                                   scene_.testInput,
                                   commandLine_.fixedDtSeconds);
        UpdateStarterArenaPlayerTransform(renderWorld_,
                                          playableScene_.player,
                                          evidence_.simulation.position);

        RenderScene::RenderView view = BuildStarterArenaPlayableView(renderWorld_,
                                                                     playableScene_.camera);
        evidence_.lastFrameDrawItems = static_cast<std::uint32_t>(view.DrawCount());
        evidence_.lastVisitedEntities = view.visited;
        evidence_.lastCulledEntities = view.culledFrustum + view.culledDistance +
                                       view.culledVisibility + view.culledHidden;
        evidence_.totalDrawItems += view.DrawCount();
        evidence_.playerDrawSubmitted = evidence_.playerDrawSubmitted ||
                                        ViewContainsEntity(view, playableScene_.player);

        backend_->BeginFrame();
        backend_->Submit(playableScene_.camera, view);
        backend_->EndFrame();
        ++evidence_.presentedFrames;

        if (commandLine_.playableFrames > 0u &&
            evidence_.presentedFrames >= commandLine_.playableFrames)
        {
            RequestClose();
        }
    }

    void OnShutdown() override
    {
        Saga::IWindow* window = GetWindow();
        if (window != nullptr)
        {
            window->SetOnResize({});
            window->SetRHIOwnsPresent(false);
        }
        if (backend_)
        {
            DestroyStarterArenaPlayableScene(*backend_, renderWorld_, playableScene_);
            backend_->Shutdown();
            backend_.reset();
        }
        evidence_.shutdownComplete = true;
    }

    const StarterArenaScene& scene_;
    const RuntimeCommandLine& commandLine_;
    PlayableEvidence& evidence_;
    std::vector<StarterArenaDiagnostic>& diagnostics_;
    std::unique_ptr<RB::IRenderBackend> backend_;
    RenderWorld::RenderWorld renderWorld_;
    StarterArenaPlayableScene playableScene_;
};

Json BuildReport(const RuntimeCommandLine& commandLine,
                 const StarterArenaProject* project,
                 const StarterArenaScene* scene,
                 const PlayableEvidence& evidence,
                 const std::vector<StarterArenaDiagnostic>& diagnostics,
                 bool passed)
{
    Json diagnosticArray = Json::array();
    for (const StarterArenaDiagnostic& diagnostic : diagnostics)
    {
        diagnosticArray.push_back({{"code", diagnostic.code}, {"message", diagnostic.message}});
    }

    const bool frameExpectation = evidence.presentedFrames > 0u &&
                                  (evidence.requestedFrames == 0u ||
                                   evidence.presentedFrames == evidence.requestedFrames);
    return {
        {"status", passed ? "Passed" : "Failed"},
        {"project",
         {{"projectId", project ? project->projectId : ""},
          {"projectPath", project ? GenericPath(project->manifestPath) : ""},
          {"sceneSource", project ? "ProjectSceneReference" : "ProjectSceneReferenceMissing"}}},
        {"scene",
         {{"sceneId", scene ? scene->sceneId : ""},
          {"source", scene ? GenericPath(scene->absolutePath) : ""},
          {"playerSpawn", scene ? Vec2Json(scene->playerSpawn) : Json(nullptr)},
          {"bounds", scene ? BoundsJson(scene->bounds) : Json(nullptr)},
          {"camera",
           scene ? Json{{"mode", scene->camera.mode},
                        {"x", scene->camera.x},
                        {"y", scene->camera.y},
                        {"width", scene->camera.width},
                        {"height", scene->camera.height}}
                 : Json(nullptr)}}},
        {"simulation",
         {{"status", evidence.sceneReady ? "Passed" : "NotExecuted"},
          {"ticks", evidence.simulation.ticks},
          {"fixedDtSeconds", commandLine.fixedDtSeconds},
          {"inputVector", scene ? Vec2Json(scene->testInput) : Json(nullptr)},
          {"finalPosition", Vec2Json(evidence.simulation.position)},
          {"clampCount", evidence.simulation.clampCount}}},
        {"render",
         {{"status", passed ? "Passed" : (evidence.initialized ? "Failed" : "NotInitialized")},
          {"backend", evidence.backend},
          {"initialized", evidence.initialized},
          {"requestedFrames", evidence.requestedFrames},
          {"presentedFrames", evidence.presentedFrames},
          {"drawItemsLastFrame", evidence.lastFrameDrawItems},
          {"totalDrawItems", evidence.totalDrawItems},
          {"visitedEntitiesLastFrame", evidence.lastVisitedEntities},
          {"culledEntitiesLastFrame", evidence.lastCulledEntities},
          {"playerDrawSubmitted", evidence.playerDrawSubmitted},
          {"placeholderViewCount", 0},
          {"viewport", {{"width", evidence.viewportWidth}, {"height", evidence.viewportHeight}}},
          {"resizeCount", evidence.resizeCount},
          {"shutdown", evidence.shutdownComplete ? "Completed" : "Incomplete"}}},
        {"settings",
         {{"bounded", evidence.requestedFrames > 0u},
          {"playableFrames", evidence.requestedFrames},
          {"fixedDtSeconds", commandLine.fixedDtSeconds}}},
        {"expectations",
         {{"projectSceneLoaded", project != nullptr && scene != nullptr},
          {"renderInitialized", evidence.initialized},
          {"framesPresented", frameExpectation},
          {"playerDrawSubmitted", evidence.playerDrawSubmitted},
          {"cleanShutdown", evidence.shutdownComplete}}},
        {"nonClaims",
         Json::array({"No interactive input proof",
                      "No C# world mutation proof",
                      "No editor workflow proof",
                      "No networking or multiplayer proof",
                      "No package install or distribution proof",
                      "No production readiness claim"})},
        {"diagnostics", std::move(diagnosticArray)}};
}

bool WriteReport(const std::filesystem::path& path, const Json& report, std::string& error)
{
    if (path.empty())
    {
        return true;
    }
    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, ec);
        if (ec)
        {
            error = "Failed to create playable report directory: " + ec.message();
            return false;
        }
    }
    std::ofstream output(path);
    if (!output)
    {
        error = "Failed to open playable report: " + GenericPath(path);
        return false;
    }
    output << report.dump(2) << '\n';
    if (!output)
    {
        error = "Failed to write playable report: " + GenericPath(path);
        return false;
    }
    return true;
}

} // namespace

int RunStarterArenaPlayable(const RuntimeCommandLine& commandLine)
{
    std::vector<StarterArenaDiagnostic> diagnostics;
    if (commandLine.launchOptions.headless)
    {
        diagnostics.push_back({"StarterArena.Playable.VisibleRequired",
                               "--starter-arena-playable cannot be combined with --headless; use "
                               "--starter-arena-smoke for headless evidence."});
    }
    if (commandLine.fixedDtSeconds <= 0.0)
    {
        diagnostics.push_back(
            {"StarterArena.Playable.FixedDtInvalid", "--fixed-dt must be greater than zero."});
    }
    if (commandLine.playableFramesProvided && commandLine.playableFrames == 0u)
    {
        diagnostics.push_back({"StarterArena.Playable.FrameCountInvalid",
                               "--playable-frames must be greater than zero when provided."});
    }
    if (commandLine.invokeStarterArenaScript || commandLine.runStarterArenaScriptLifecycle ||
        !commandLine.scriptManifestPath.empty() || !commandLine.scriptArtifactsPath.empty())
    {
        diagnostics.push_back(
            {"StarterArena.Playable.ScriptModeDeferred",
             "Visible StarterArena script execution is not part of this frame-only mode; use the "
             "existing headless script smoke commands."});
    }

    std::optional<StarterArenaProject> project = LoadStarterArenaProject(commandLine.projectPath,
                                                                         diagnostics);
    std::optional<StarterArenaScene> scene;
    if (project)
    {
        scene = LoadStarterArenaScene(*project, diagnostics);
        if (IsPathInside(commandLine.playableReportOut, project->projectRoot))
        {
            diagnostics.push_back(
                {"StarterArena.Playable.ReportInsideSample",
                 "Generated playable reports must remain outside the StarterArena sample root."});
        }
    }

    PlayableEvidence evidence;
    if (scene)
    {
        evidence.simulation = MakeStarterArenaSimulation(*scene);
    }
    if (diagnostics.empty() && scene)
    {
        StarterArenaPlayableHost host(*scene, commandLine, evidence, diagnostics);
        host.Run();
        if (!evidence.initialized && diagnostics.empty())
        {
            diagnostics.push_back({"StarterArena.Playable.WindowInitializationFailed",
                                   "The StarterArena window did not initialize."});
        }
    }

    const bool framesPassed = evidence.presentedFrames > 0u &&
                              (commandLine.playableFrames == 0u ||
                               evidence.presentedFrames == commandLine.playableFrames);
    const bool passed = diagnostics.empty() && project && scene && evidence.initialized &&
                        evidence.sceneReady && evidence.playerDrawSubmitted && framesPassed &&
                        evidence.shutdownComplete;
    Json report = BuildReport(commandLine,
                              project ? &*project : nullptr,
                              scene ? &*scene : nullptr,
                              evidence,
                              diagnostics,
                              passed);
    std::string reportError;
    if (!WriteReport(commandLine.playableReportOut, report, reportError))
    {
        LOG_ERROR(kLogTag, "%s", reportError.c_str());
        return 1;
    }

    if (!passed)
    {
        for (const StarterArenaDiagnostic& diagnostic : diagnostics)
        {
            LOG_ERROR(kLogTag, "%s: %s", diagnostic.code.c_str(), diagnostic.message.c_str());
        }
        return 1;
    }
    LOG_INFO(kLogTag,
             "Visible StarterArena frame passed: backend=%s frames=%u draws=%llu player=(%.3f, "
             "%.3f) report='%s'",
             evidence.backend.c_str(),
             evidence.presentedFrames,
             static_cast<unsigned long long>(evidence.totalDrawItems),
             evidence.simulation.position.x,
             evidence.simulation.position.y,
             commandLine.playableReportOut.string().c_str());
    return 0;
}

} // namespace SagaRuntimeApp
