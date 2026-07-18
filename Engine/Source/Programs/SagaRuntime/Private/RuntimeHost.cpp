// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "RuntimeHost.h"

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <SagaEngine/Scripting/ScriptLifecycleService.hpp>
#include <SagaEngine/Input/Backends/IPlatformInputBackend.h>
#include <SagaEngine/Input/Frames/RawInputFrame.h>
#include <SagaEngine/Math/Mat4.h>
#include <SagaEngine/Math/Transform.h>
#include <SagaEngine/Platform/IWindow.h>
#include <SagaEngine/Platform/PlatformFactory.h>
#include <SagaEngine/Render/Assets/CookedRenderAssetLoader.h>
#include <SagaEngine/Render/Backend/RenderBackendFactory.h>
#include <SagaEngine/Render/Scene/RenderView.h>
#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <nlohmann/json.hpp>
#include <SDL2/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace SagaRuntimeApp
{
namespace
{
using Json = nlohmann::json;
namespace Play = SagaRuntime::Play;
namespace Scripting = SagaEngine::Scripting;

std::string Read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream bytes;
    if (input) bytes << input.rdbuf();
    return bytes.str();
}

bool WriteJson(const std::filesystem::path& path, const Json& value)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        const auto bytes = value.dump(2) + "\n";
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!output) return false;
    }
    std::filesystem::rename(temporary, path, error);
    return !error;
}

std::filesystem::path FindBuildRoot(std::filesystem::path path)
{
    path = path.lexically_normal();
    for (auto cursor = path; !cursor.empty(); cursor = cursor.parent_path())
    {
        if (cursor.filename() == "Build") return cursor;
        if (cursor == cursor.root_path()) break;
    }
    return {};
}

bool Exact(const Json& value, std::initializer_list<std::string_view> fields)
{
    if (!value.is_object()) return false;
    const std::set<std::string_view> allowed(fields);
    for (auto item = value.cbegin(); item != value.cend(); ++item)
        if (!allowed.contains(item.key())) return false;
    return true;
}

struct EnvelopeReadResult
{
    Play::ResolvedLocalLaunchEnvelope envelope;
    std::vector<Play::RuntimeDiagnostic> diagnostics;
};

EnvelopeReadResult ReadEnvelope(const std::filesystem::path& path)
{
    EnvelopeReadResult result;
    Json root;
    try { root = Json::parse(Read(path)); }
    catch (const Json::exception& error)
    {
        result.diagnostics.push_back({"Runtime.Envelope.JsonInvalid", error.what(), path.string()});
        return result;
    }
    if (!Exact(root, {"artifacts", "canonicalManifestHash", "canonicalManifestPath",
                      "environment", "logPath", "playSessionId", "resultPath",
                      "runtimeExecutable", "schemaVersion", "startupReadyPath",
                      "stopPath", "workingDirectory"}) ||
        root.value("schemaVersion", -1) != 1 || !root.contains("artifacts") ||
        !root["artifacts"].is_object() || !root.contains("environment") ||
        !root["environment"].is_object())
    {
        result.diagnostics.push_back({"Runtime.Envelope.SchemaInvalid",
            "Local launch envelope schema version 1 is required.", path.string()});
        return result;
    }
    auto& envelope = result.envelope;
    envelope.schemaVersion = 1;
    envelope.playSessionId = root.value("playSessionId", std::string{});
    envelope.canonicalManifestPath = root.value("canonicalManifestPath", std::string{});
    envelope.canonicalManifestHash = root.value("canonicalManifestHash", std::string{});
    envelope.runtimeExecutable = root.value("runtimeExecutable", std::string{});
    envelope.workingDirectory = root.value("workingDirectory", std::string{});
    envelope.resultPath = root.value("resultPath", std::string{});
    envelope.logPath = root.value("logPath", std::string{});
    envelope.stopPath = root.value("stopPath", std::string{});
    envelope.startupReadyPath = root.value("startupReadyPath", std::string{});
    for (const auto& [id, value] : root["artifacts"].items())
        if (value.is_string() && !id.empty()) envelope.artifacts.emplace(id, value.get<std::string>());
        else result.diagnostics.push_back({"Runtime.Envelope.ArtifactInvalid",
            "Artifact entries require non-empty logical ids and paths.", id});
    for (const auto& [name, value] : root["environment"].items())
        if (value.is_string() && !name.empty()) envelope.environment.emplace(name, value.get<std::string>());
        else result.diagnostics.push_back({"Runtime.Envelope.EnvironmentInvalid",
            "Environment entries require string names and values.", name});
    const auto buildRoot = FindBuildRoot(envelope.workingDirectory);
    auto validation = Play::ValidateLocalEnvelope(envelope,
        {buildRoot, buildRoot.parent_path(), envelope.runtimeExecutable.parent_path()});
    result.diagnostics.insert(result.diagnostics.end(), validation.begin(), validation.end());
    return result;
}

bool VerifyFile(const std::map<std::string, std::filesystem::path>& paths,
                const Play::ArtifactIdentity& identity,
                std::vector<Play::RuntimeDiagnostic>& diagnostics)
{
    const auto item = paths.find(identity.logicalId);
    if (item == paths.end() || Play::Sha256Bytes(Read(item->second)) != identity.hash)
    {
        diagnostics.push_back({"Runtime.Launch.ArtifactHashMismatch",
            "Resolved artifact bytes do not match the canonical launch identity.",
            identity.logicalId});
        return false;
    }
    return true;
}

class RuntimeScriptWorld final : public Scripting::ISagaScriptWorld
{
public:
    explicit RuntimeScriptWorld(Play::RuntimeSceneArtifact scene) : scene_(std::move(scene))
    {
        for (std::size_t index = 0; index < scene_.entities.size(); ++index)
            byId_.emplace(scene_.entities[index].entityId, index);
    }

    Scripting::ScriptEntityHandle Handle(std::string_view id) const
    {
        const auto item = byId_.find(std::string(id));
        return item == byId_.end() ? Scripting::ScriptEntityHandle{} :
            Scripting::ScriptEntityHandle{item->second + 1};
    }

    const Play::RuntimeSceneArtifact& Scene() const noexcept { return scene_; }

    Scripting::ScriptEntityCreateResult CreateEntity(const std::string& name) override
    {
        Scripting::ScriptEntityCreateResult result;
        Play::RuntimeSceneEntity entity;
        entity.entityId = "runtime-entity-" + std::to_string(scene_.entities.size() + 1);
        entity.name = name;
        byId_.emplace(entity.entityId, scene_.entities.size());
        scene_.entities.push_back(std::move(entity));
        result.succeeded = true;
        result.entity.value = scene_.entities.size();
        return result;
    }

    Scripting::ScriptHostOperationResult SetPosition(
        Scripting::ScriptEntityHandle entity, Scripting::ScriptVector3 position) override
    {
        Scripting::ScriptHostOperationResult result;
        const auto index = entity.value == 0 ? scene_.entities.size() : entity.value - 1;
        if (index >= scene_.entities.size() || !std::isfinite(position.x) ||
            !std::isfinite(position.y) || !std::isfinite(position.z)) return result;
        scene_.entities[index].transform.position = {position.x, position.y, position.z};
        result.succeeded = true;
        return result;
    }

    Scripting::ScriptEntityPositionResult GetPosition(
        Scripting::ScriptEntityHandle entity) const override
    {
        Scripting::ScriptEntityPositionResult result;
        const auto index = entity.value == 0 ? scene_.entities.size() : entity.value - 1;
        if (index >= scene_.entities.size()) return result;
        const auto& position = scene_.entities[index].transform.position;
        result.position = {static_cast<float>(position[0]),
                           static_cast<float>(position[1]),
                           static_cast<float>(position[2])};
        result.succeeded = true;
        return result;
    }

private:
    Play::RuntimeSceneArtifact scene_;
    std::map<std::string, std::size_t> byId_;
};

SagaEngine::Math::Transform ToMathTransform(const Play::RuntimeTransform& source)
{
    return {{static_cast<float>(source.position[0]), static_cast<float>(source.position[1]),
             static_cast<float>(source.position[2])},
            {static_cast<float>(source.rotation[0]), static_cast<float>(source.rotation[1]),
             static_cast<float>(source.rotation[2]), static_cast<float>(source.rotation[3])},
            {static_cast<float>(source.scale[0]), static_cast<float>(source.scale[1]),
             static_cast<float>(source.scale[2])}};
}

class RuntimeRenderSession
{
public:
    RuntimeRenderSession(RuntimeScriptWorld& world,
                         const std::map<std::string, std::filesystem::path>& artifacts,
                         std::vector<Play::RuntimeDiagnostic>& diagnostics,
                         std::filesystem::path capturePath)
        : world_(world), artifacts_(artifacts), diagnostics_(diagnostics),
          capturePath_(std::move(capturePath)) {}

    bool Initialize()
    {
        platform_ = Saga::CreateSDLPlatformFactory();
        window_ = platform_ ? platform_->CreateWindow() : nullptr;
        Saga::WindowDesc windowDesc;
        windowDesc.title = "SagaRuntime Play";
        windowDesc.width = 1280;
        windowDesc.height = 720;
        windowDesc.vsync = true;
        windowDesc.resizable = true;
        if (!window_ || !window_->Init(windowDesc)) return Fail("Runtime.Render.WindowInitFailed");
        input_ = SagaEngine::Platform::CreateSDLInputBackend(window_->GetNativeHandle());
        if (!input_ || !input_->Initialize()) return Fail("Runtime.Input.BackendInitFailed");
        SagaEngine::Render::Backend::RenderBackendConfig config;
        config.preferredAPI = SagaEngine::Render::Backend::GraphicsBackendAPI::kNativePortable;
        backend_ = SagaEngine::Render::Backend::CreateRenderBackend(config);
        SagaEngine::Render::Backend::SwapchainDesc swapchain;
        swapchain.nativeWindow = window_->GetOSNativeHandle();
        swapchain.width = window_->GetWidth();
        swapchain.height = window_->GetHeight();
        swapchain.vsync = true;
        if (!backend_ || !backend_->Initialize(swapchain)) return Fail("Runtime.Render.BackendInitFailed");
        window_->SetRHIOwnsPresent(true);
        auto* backend = backend_.get();
        window_->SetOnResize([backend](std::uint32_t width, std::uint32_t height)
        { if (width && height) backend->OnResize(width, height); });
        return UploadResources();
    }

    void Shutdown()
    {
        if (backend_)
        {
            for (const auto& [_, mesh] : meshes_) backend_->DestroyMesh(mesh);
            for (const auto& [_, material] : materials_) backend_->DestroyMaterial(material);
            backend_->Shutdown();
            backend_.reset();
        }
        if (input_) { input_->Shutdown(); input_.reset(); }
        if (window_) { window_->Shutdown(); window_.reset(); }
        platform_.reset();
    }

    ~RuntimeRenderSession() { Shutdown(); }

    bool ShouldClose() const noexcept { return !window_ || window_->ShouldClose(); }

    std::vector<std::string> PollLogicalActions(
        const Play::RuntimeInputMapArtifact& inputMap, bool injectSmokeKey)
    {
        if (injectSmokeKey)
        {
            const auto binding = std::find_if(inputMap.actions.begin(), inputMap.actions.end(),
                [](const auto& action) { return !action.bindings.empty(); });
            if (binding == inputMap.actions.end()) return {};
            const auto& control = binding->bindings.front().control;
            const auto scancode = SDL_GetScancodeFromName(control.c_str());
            if (scancode == SDL_SCANCODE_UNKNOWN) return {};
            SDL_Event event{};
            event.type = SDL_KEYDOWN;
            event.key.state = SDL_PRESSED;
            event.key.repeat = 0;
            event.key.keysym.scancode = scancode;
            event.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
            SDL_PushEvent(&event);
        }
        window_->PollEvents();
        SagaEngine::Input::RawInputFrame raw;
        input_->PollRawEvents(raw);
        std::vector<std::string> actions;
        for (const auto& key : raw.keys)
        {
            if (!key.pressed || key.repeat) continue;
            for (const auto& action : inputMap.actions)
                for (const auto& binding : action.bindings)
                {
                    if (binding.device != "Keyboard" || binding.control.size() != 1) continue;
                    const auto expected = static_cast<SagaEngine::Input::KeyCode>(
                        static_cast<unsigned>(SagaEngine::Input::KeyCode::A) +
                        static_cast<unsigned>(binding.control.front() - 'A'));
                    if (key.code == expected) actions.push_back(action.actionId);
                }
        }
        return actions;
    }

    bool RenderFrame()
    {
        const auto transforms = WorldTransforms();
        SagaEngine::Render::Scene::RenderView view;
        std::uint32_t nextEntity = 0;
        for (const auto& entity : world_.Scene().entities)
        {
            if (!entity.meshRenderer) continue;
            SagaEngine::Render::Scene::DrawItem item;
            item.entity = static_cast<SagaEngine::Render::World::RenderEntityId>(nextEntity++);
            item.mesh = meshes_.at(entity.meshRenderer->mesh.logicalId);
            item.material = materials_.at(entity.meshRenderer->material.logicalId);
            item.model = SagaEngine::Math::Mat4::FromTransform(transforms.at(entity.entityId));
            view.drawItems.push_back(item);
        }
        const Play::RuntimeSceneEntity* cameraEntity = nullptr;
        const Play::RuntimeSceneEntity* lightEntity = nullptr;
        for (const auto& entity : world_.Scene().entities)
        {
            if (entity.primaryCamera) cameraEntity = &entity;
            if (entity.directionalLight) lightEntity = &entity;
        }
        if (!cameraEntity) return Fail("Runtime.Render.PrimaryCameraMissing");
        const auto& cameraTransform = transforms.at(cameraEntity->entityId);
        SagaEngine::Render::Scene::Camera camera;
        camera.position = cameraTransform.position;
        const auto forward = cameraTransform.rotation.Rotate({0.0f, 0.0f, -1.0f});
        const auto up = cameraTransform.rotation.Rotate({0.0f, 1.0f, 0.0f});
        camera.view = SagaEngine::Math::Mat4::LookAtRH(camera.position,
            camera.position + forward, up);
        constexpr float pi = 3.14159265358979323846f;
        camera.projection = SagaEngine::Math::Mat4::PerspectiveRH_ZO(
            static_cast<float>(cameraEntity->verticalFovDegrees) * pi / 180.0f,
            window_->GetHeight() ? static_cast<float>(window_->GetWidth()) / window_->GetHeight() : 1.0f,
            static_cast<float>(cameraEntity->nearClip), static_cast<float>(cameraEntity->farClip));
        camera.RecomputeViewProj();
        camera.RecomputeFrustum();
        if (lightEntity)
        {
            const auto& transform = transforms.at(lightEntity->entityId);
            view.lighting.directionalEnabled = true;
            view.lighting.shadowsEnabled = lightEntity->castsShadows;
            view.lighting.directional.direction = transform.rotation.Rotate({0.0f, -1.0f, 0.0f});
            view.lighting.directional.color = {
                static_cast<float>(lightEntity->lightColor[0]),
                static_cast<float>(lightEntity->lightColor[1]),
                static_cast<float>(lightEntity->lightColor[2])};
            view.lighting.directional.intensity = static_cast<float>(lightEntity->lightIntensity);
            view.lighting.ambient = {{1.0f, 1.0f, 1.0f}, 0.12f};
        }
        backend_->BeginFrame();
        backend_->Submit(camera, view);
        if (!capturedMeaningfulPixels_)
        {
            SagaEngine::Render::Backend::RenderFrameCapture capture;
            if (backend_->CaptureCurrentColorFrame(capture) && capture.pixels.size() >= 8)
            {
                for (std::size_t offset = 4; offset + 3 < capture.pixels.size(); offset += 4)
                    if (!std::equal(capture.pixels.begin(), capture.pixels.begin() + 4,
                                    capture.pixels.begin() + static_cast<std::ptrdiff_t>(offset)))
                    {
                        capturedMeaningfulPixels_ = true;
                        std::ofstream output(capturePath_, std::ios::binary | std::ios::trunc);
                        output << "P6\n" << capture.width << " " << capture.height << "\n255\n";
                        for (std::uint32_t y = 0; y < capture.height; ++y)
                            for (std::uint32_t x = 0; x < capture.width; ++x)
                            {
                                const auto pixel = static_cast<std::size_t>(y) * capture.rowPitch +
                                    static_cast<std::size_t>(x) * 4U;
                                output.write(reinterpret_cast<const char*>(capture.pixels.data() + pixel), 3);
                            }
                        break;
                    }
            }
        }
        backend_->EndFrame();
        lastDrawCount_ = view.DrawCount();
        return true;
    }

    [[nodiscard]] bool HasMeaningfulPixels() const noexcept { return capturedMeaningfulPixels_; }
    [[nodiscard]] std::size_t LastDrawCount() const noexcept { return lastDrawCount_; }
    [[nodiscard]] const std::filesystem::path& CapturePath() const noexcept { return capturePath_; }

private:
    bool Fail(std::string code)
    {
        diagnostics_.push_back({std::move(code), "Runtime render session failed.", {}});
        return false;
    }

    bool UploadResources()
    {
        for (const auto& entity : world_.Scene().entities)
        {
            if (!entity.meshRenderer) continue;
            const auto& renderer = *entity.meshRenderer;
            for (const auto& identity : {renderer.mesh, renderer.material})
            {
                const auto found = artifacts_.find(identity.logicalId);
                Json artifact;
                try
                {
                    if (found == artifacts_.end()) throw std::runtime_error("asset path missing");
                    artifact = Json::parse(Read(found->second));
                }
                catch (const std::exception&)
                { return Fail("Runtime.Render.AssetHashMismatch"); }
                if (artifact.value("artifactId", std::string{}) != identity.logicalId ||
                    artifact.value("contentHash", std::string{}) != identity.hash)
                    return Fail("Runtime.Render.AssetHashMismatch");
            }
            if (!meshes_.contains(renderer.mesh.logicalId))
            {
                const auto loaded = SagaEngine::Render::LoadCookedMeshArtifact(
                    artifacts_.at(renderer.mesh.logicalId));
                if (!loaded.Succeeded()) return Fail("Runtime.Render.MeshInvalid");
                const auto handle = backend_->CreateMesh(loaded.asset);
                if (handle == SagaEngine::Render::World::MeshId::kInvalid)
                    return Fail("Runtime.Render.MeshUploadFailed");
                meshes_.emplace(renderer.mesh.logicalId, handle);
            }
            if (!materials_.contains(renderer.material.logicalId))
            {
                const auto loaded = SagaEngine::Render::LoadCookedMaterialArtifact(
                    artifacts_.at(renderer.material.logicalId),
                    static_cast<SagaEngine::Render::ShaderTemplateHandle>(1));
                if (!loaded.Succeeded()) return Fail("Runtime.Render.MaterialInvalid");
                const auto handle = backend_->CreateMaterial(loaded.material);
                if (handle == SagaEngine::Render::World::MaterialId::kInvalid)
                    return Fail("Runtime.Render.MaterialUploadFailed");
                materials_.emplace(renderer.material.logicalId, handle);
            }
        }
        return !meshes_.empty() && !materials_.empty();
    }

    std::map<std::string, SagaEngine::Math::Transform> WorldTransforms() const
    {
        std::map<std::string, const Play::RuntimeSceneEntity*> byId;
        for (const auto& entity : world_.Scene().entities) byId.emplace(entity.entityId, &entity);
        std::map<std::string, SagaEngine::Math::Transform> result;
        const auto resolve = [&](const auto& self, const Play::RuntimeSceneEntity& entity)
            -> SagaEngine::Math::Transform
        {
            if (const auto found = result.find(entity.entityId); found != result.end()) return found->second;
            auto value = ToMathTransform(entity.transform);
            if (entity.parentEntityId) value = self(self, *byId.at(*entity.parentEntityId)).CombineWith(value);
            result.emplace(entity.entityId, value);
            return value;
        };
        for (const auto& entity : world_.Scene().entities) (void)resolve(resolve, entity);
        return result;
    }

    RuntimeScriptWorld& world_;
    const std::map<std::string, std::filesystem::path>& artifacts_;
    std::vector<Play::RuntimeDiagnostic>& diagnostics_;
    std::unique_ptr<Saga::PlatformFactory> platform_;
    std::unique_ptr<Saga::IWindow> window_;
    std::unique_ptr<SagaEngine::Platform::IPlatformInputBackend> input_;
    std::unique_ptr<SagaEngine::Render::Backend::IRenderBackend> backend_;
    std::map<std::string, SagaEngine::Render::World::MeshId> meshes_;
    std::map<std::string, SagaEngine::Render::World::MaterialId> materials_;
    bool capturedMeaningfulPixels_ = false;
    std::size_t lastDrawCount_ = 0;
    std::filesystem::path capturePath_;
};

class NamedActionTarget final : public Play::INamedActionTarget
{
public:
    NamedActionTarget(Scripting::CSharpScriptHost& host,
        std::map<std::pair<std::string, std::string>, Scripting::ScriptInstanceHandle> instances,
        std::map<std::pair<std::string, std::string>, std::string> methods)
        : host_(host), instances_(std::move(instances)), methods_(std::move(methods)) {}

    bool InvokeNamedAction(std::string_view behaviorTypeId,
                           std::string_view namedActionId,
                           std::string_view entityId) override
    {
        const auto instance = instances_.find({std::string(entityId), std::string(behaviorTypeId)});
        const auto method = methods_.find({std::string(behaviorTypeId),
                                           std::string(namedActionId)});
        if (instance == instances_.end() || method == methods_.end()) return false;
        return host_.InvokeNamedAction({instance->second, method->second,
            std::string(namedActionId)}).Succeeded();
    }

private:
    Scripting::CSharpScriptHost& host_;
    std::map<std::pair<std::string, std::string>, Scripting::ScriptInstanceHandle> instances_;
    std::map<std::pair<std::string, std::string>, std::string> methods_;
};

RuntimeHostResult Failure(std::string code, std::string message)
{
    RuntimeHostResult result;
    result.exitCode = 1;
    result.diagnosticId = std::move(code);
    result.message = std::move(message);
    return result;
}
} // namespace

RuntimeHost::RuntimeHost(std::filesystem::path playEnvelope, bool headless,
                         bool smokeInteractions)
    : playEnvelope_(std::move(playEnvelope))
    , headless_(headless)
    , smokeInteractions_(smokeInteractions)
    , legacyGenericMode_(false)
{
}

RuntimeHostResult RuntimeHost::Run() const
{
    if (legacyGenericMode_)
    {
        auto result = Failure(
            RuntimeHostDiagnostics::GenericProjectModeUnsupported,
            "Generic project execution is unsupported; use a verified Play envelope.");
        result.exitCode = 2;
        return result;
    }
    if (playEnvelope_.empty())
        return Failure(RuntimeHostDiagnostics::PlayEnvelopeRequired,
            "SagaRuntime requires --play-envelope for canonical project Play.");

    auto envelopeResult = ReadEnvelope(playEnvelope_);
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
        WriteJson(envelope.resultPath, {{"canonicalLaunchManifestHash", envelope.canonicalManifestHash},
            {"crashArtifactPath", nullptr},
            {"exitClassification", classification}, {"exitCode", status == "Succeeded" ? 0 : 1},
            {"framesExecuted", frames}, {"interactionActionCount", actions},
            {"logPath", envelope.logPath.string()},
            {"observedState", observed}, {"playSessionId", envelope.playSessionId},
            {"runtimeDiagnostics", diagnostics}, {"schemaVersion", 1},
            {"startupDiagnostics", Json::array()}, {"status", status},
            {"shutdownClassification", classification == "GracefulStop" ?
                "GracefulStop" : (status == "Succeeded" ? "Clean" : "StartupFailure")}});
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
        Read(envelope.canonicalManifestPath), envelope.canonicalManifestHash);
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
    for (const auto* identity : {&launch.scene, &launch.input, &launch.scriptAssembly,
            &launch.scriptRuntimeConfig, &launch.scriptAssemblyManifest,
            &launch.scriptReferenceManifest, &launch.behaviors,
            &launch.namedActions, &launch.capabilities})
        VerifyFile(envelope.artifacts, *identity, envelopeResult.diagnostics);
    const Play::ArtifactIdentity bridge{launch.runtimeBridge.logicalReferenceId,
                                        launch.runtimeBridge.assemblyHash};
    VerifyFile(envelope.artifacts, bridge, envelopeResult.diagnostics);
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
        packageCurrent = Json::parse(Read(packageCurrentPath->second));
        packageManifest = Json::parse(Read(packageManifestPath->second));
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
            Play::Sha256Bytes(Read(packageManifestPath->second)) !=
                packageCurrent.value("generationManifestHash", std::string{}) ||
            Read(generationLockPath->second) != Read(trackedLockPath->second))
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
    if (!sceneResult.Succeeded() || !inputResult.Succeeded())
    {
        writeResult("Failed", "ArtifactFailure", 0, 0, Json::object());
        return Failure("Runtime.Play.ArtifactLoadFailed",
            "Runtime scene or input artifact failed strict loading.");
    }
    Json namedActions, behaviorManifest, capabilityManifest;
    try
    {
        namedActions = Json::parse(Read(envelope.artifacts.at(launch.namedActions.logicalId)));
        behaviorManifest = Json::parse(Read(envelope.artifacts.at(launch.behaviors.logicalId)));
        capabilityManifest = Json::parse(Read(envelope.artifacts.at(launch.capabilities.logicalId)));
    }
    catch (const Json::exception&)
    {
        return structuredFailure("Runtime.Script.NamedActionsInvalid",
            "Named-action manifest is malformed.", "ScriptLoadFailure");
    }
    RuntimeScriptWorld world(*sceneResult.value);
    Scripting::CSharpScriptHost host({envelope.artifacts.at(
        launch.runtimeBridge.logicalReferenceId), {}, &world, nullptr, false, {}});
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

    NamedActionTarget target(host, instances, std::move(methods));
    Play::ScriptActionBindingDispatcher dispatcher(target);
    const auto bindingDiagnostics = dispatcher.Build(world.Scene());
    if (!bindingDiagnostics.empty())
        return structuredFailure(bindingDiagnostics.front().code,
            bindingDiagnostics.front().message, "ActionBindingFailure");

    const auto publishReady = [&]
    {
        return WriteJson(envelope.startupReadyPath,
            {{"canonicalLaunchManifestHash", envelope.canonicalManifestHash},
             {"playSessionId", envelope.playSessionId}, {"schemaVersion", 1},
             {"status", "Ready"}});
    };

    std::uint64_t frames = 0;
    std::vector<double> observedHeights;
    const auto observedEntity = std::find_if(world.Scene().entities.begin(), world.Scene().entities.end(),
        [](const auto& entity) { return !entity.actionBindings.empty(); });
    if (observedEntity == world.Scene().entities.end())
        return structuredFailure("Runtime.Script.ActionBindingMissing",
            "Runtime scene has no observable script action binding.", "ActionBindingFailure");
    const auto record = [&]
    {
        const auto position = world.GetPosition(world.Handle(observedEntity->entityId));
        if (position.Succeeded()) observedHeights.push_back(position.position.y);
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
    if (headless_)
    {
        if (!publishReady())
            return structuredFailure("Runtime.Startup.ReadyPublishFailed",
                "The session-bound startup-ready record could not be published.",
                "LaunchValidationFailure");
        if (smokeInteractions_)
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
            const bool inject = smokeInteractions_ && (frames == 30 || frames == 190);
            const auto actions = render.PollLogicalActions(*inputResult.value, inject);
            for (const auto& action : actions)
            {
                if (smokeInteractions_ && dispatcher.DispatchCount() == 1 && !recordedOpen)
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
            if (!render.RenderFrame())
            {
                render.Shutdown();
                return structuredFailure("Runtime.Render.FrameFailed",
                    "Runtime renderer failed a frame.", "RendererFailure");
            }
            ++frames;
            if (smokeInteractions_ && frames >= 350) break;
        }
        record();
        renderEvidence = render.HasMeaningfulPixels();
        drawCount = render.LastDrawCount();
        if (renderEvidence)
            envelopeResult.diagnostics.push_back({"Runtime.Render.FrameCaptureEvidence",
                "Renderer produced a non-clear frame capture.", render.CapturePath().string()});
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
        {"transition", transitioned ? "ClosedOpenClosed" : "Incomplete"}};
    if ((smokeInteractions_ && !transitioned) || (!headless_ && !renderEvidence))
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
