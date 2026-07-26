// SPDX-License-Identifier: MPL-2.0

#include "RuntimeRenderSession.h"

#include "RuntimeEvidenceSession.h"
#include "RuntimeLaunchSession.h"
#include "RuntimeScriptSession.h"
#include "RuntimeUiSession.h"

#include <SagaEngine/Input/Backends/IPlatformInputBackend.h>
#include <SagaEngine/Input/Frames/RawInputFrame.h>
#include <SagaEngine/Math/Mat4.h>
#include <SagaEngine/Math/Transform.h>
#include <SagaEngine/Platform/IWindow.h>
#include <SagaEngine/Platform/PlatformFactory.h>
#include <SagaEngine/Render/Assets/CookedRenderAssetLoader.h>
#include <SagaEngine/Render/Backend/RenderBackendFactory.h>
#include <SagaEngine/Render/Scene/RenderView.h>

#include <nlohmann/json.hpp>
#include <SDL2/SDL.h>

#include <algorithm>
#include <stdexcept>

namespace SagaRuntimeApp
{
namespace
{
using Json = nlohmann::json;
namespace Play = SagaRuntime::Play;

SagaEngine::Math::Transform ToMathTransform(const Play::RuntimeTransform& source)
{
    return {{static_cast<float>(source.position[0]), static_cast<float>(source.position[1]),
             static_cast<float>(source.position[2])},
            {static_cast<float>(source.rotation[0]), static_cast<float>(source.rotation[1]),
             static_cast<float>(source.rotation[2]), static_cast<float>(source.rotation[3])},
            {static_cast<float>(source.scale[0]), static_cast<float>(source.scale[1]),
             static_cast<float>(source.scale[2])}};
}
}

struct RuntimeRenderSession::Impl
{
    RuntimeScriptWorld& world;
    const std::map<std::string, std::filesystem::path>& artifacts;
    std::vector<Play::RuntimeDiagnostic>& diagnostics;
    std::unique_ptr<Saga::PlatformFactory> platform;
    std::unique_ptr<Saga::IWindow> window;
    std::unique_ptr<SagaEngine::Platform::IPlatformInputBackend> input;
    std::unique_ptr<SagaEngine::Render::Backend::IRenderBackend> backend;
    std::map<std::string, SagaEngine::Render::World::MeshId> meshes;
    std::map<std::string, SagaEngine::Render::World::MaterialId> materials;
    RuntimeEvidenceSession evidence;
    std::size_t lastDrawCount = 0;
    std::uint32_t viewportWidth = 0;
    std::uint32_t viewportHeight = 0;

    Impl(RuntimeScriptWorld& sourceWorld,
         const std::map<std::string, std::filesystem::path>& sourceArtifacts,
         std::vector<Play::RuntimeDiagnostic>& sourceDiagnostics,
         std::filesystem::path capturePath)
        : world(sourceWorld), artifacts(sourceArtifacts), diagnostics(sourceDiagnostics),
          evidence(std::move(capturePath)) {}

    bool Fail(std::string code)
    {
        diagnostics.push_back({std::move(code), "Runtime render session failed.", {}});
        return false;
    }

    bool UploadResources()
    {
        for (const auto& entity : world.Scene().entities)
        {
            if (!entity.meshRenderer) continue;
            const auto& renderer = *entity.meshRenderer;
            for (const auto& identity : {renderer.mesh, renderer.material})
            {
                const auto found = artifacts.find(identity.logicalId);
                Json artifact;
                try
                {
                    if (found == artifacts.end()) throw std::runtime_error("asset path missing");
                    artifact = Json::parse(RuntimeLaunchSession::ReadFile(found->second));
                }
                catch (const std::exception&) { return Fail("Runtime.Render.AssetHashMismatch"); }
                if (artifact.value("artifactId", std::string{}) != identity.logicalId ||
                    artifact.value("contentHash", std::string{}) != identity.hash)
                    return Fail("Runtime.Render.AssetHashMismatch");
            }
            if (!meshes.contains(renderer.mesh.logicalId))
            {
                const auto loaded = SagaEngine::Render::LoadCookedMeshArtifact(
                    artifacts.at(renderer.mesh.logicalId));
                if (!loaded.Succeeded()) return Fail("Runtime.Render.MeshInvalid");
                const auto handle = backend->CreateMesh(loaded.asset);
                if (handle == SagaEngine::Render::World::MeshId::kInvalid)
                    return Fail("Runtime.Render.MeshUploadFailed");
                meshes.emplace(renderer.mesh.logicalId, handle);
            }
            if (!materials.contains(renderer.material.logicalId))
            {
                const auto loaded = SagaEngine::Render::LoadCookedMaterialArtifact(
                    artifacts.at(renderer.material.logicalId),
                    static_cast<SagaEngine::Render::ShaderTemplateHandle>(1));
                if (!loaded.Succeeded()) return Fail("Runtime.Render.MaterialInvalid");
                const auto handle = backend->CreateMaterial(loaded.material);
                if (handle == SagaEngine::Render::World::MaterialId::kInvalid)
                    return Fail("Runtime.Render.MaterialUploadFailed");
                materials.emplace(renderer.material.logicalId, handle);
            }
        }
        return !meshes.empty() && !materials.empty();
    }

    std::map<std::string, SagaEngine::Math::Transform> WorldTransforms() const
    {
        std::map<std::string, const Play::RuntimeSceneEntity*> byId;
        for (const auto& entity : world.Scene().entities) byId.emplace(entity.entityId, &entity);
        std::map<std::string, SagaEngine::Math::Transform> result;
        const auto resolve = [&](const auto& self, const Play::RuntimeSceneEntity& entity)
            -> SagaEngine::Math::Transform
        {
            if (const auto found = result.find(entity.entityId); found != result.end()) return found->second;
            auto value = ToMathTransform(entity.transform);
            if (entity.parentEntityId)
                value = self(self, *byId.at(*entity.parentEntityId)).CombineWith(value);
            result.emplace(entity.entityId, value);
            return value;
        };
        for (const auto& entity : world.Scene().entities) (void)resolve(resolve, entity);
        return result;
    }
};

RuntimeRenderSession::RuntimeRenderSession(RuntimeScriptWorld& world,
    const std::map<std::string, std::filesystem::path>& artifacts,
    std::vector<Play::RuntimeDiagnostic>& diagnostics, std::filesystem::path capturePath)
    : impl_(std::make_unique<Impl>(world, artifacts, diagnostics, std::move(capturePath))) {}

RuntimeRenderSession::~RuntimeRenderSession() { Shutdown(); }

bool RuntimeRenderSession::Initialize()
{
    impl_->platform = Saga::CreateSDLPlatformFactory();
    impl_->window = impl_->platform ? impl_->platform->CreateWindow() : nullptr;
    Saga::WindowDesc windowDesc;
    windowDesc.title = "SagaRuntime Play";
    windowDesc.width = 1280;
    windowDesc.height = 720;
    windowDesc.vsync = true;
    windowDesc.resizable = true;
    if (!impl_->window || !impl_->window->Init(windowDesc))
        return impl_->Fail("Runtime.Render.WindowInitFailed");
    impl_->input = SagaEngine::Platform::CreateSDLInputBackend(impl_->window->GetNativeHandle());
    if (!impl_->input || !impl_->input->Initialize())
        return impl_->Fail("Runtime.Input.BackendInitFailed");
    SagaEngine::Render::Backend::RenderBackendConfig config;
    config.preferredAPI = SagaEngine::Render::Backend::GraphicsBackendAPI::kNativePortable;
    impl_->backend = SagaEngine::Render::Backend::CreateRenderBackend(config);
    SagaEngine::Render::Backend::SwapchainDesc swapchain;
    swapchain.nativeWindow = impl_->window->GetOSNativeHandle();
    swapchain.width = impl_->window->GetWidth();
    swapchain.height = impl_->window->GetHeight();
    swapchain.vsync = true;
    if (!impl_->backend || !impl_->backend->Initialize(swapchain))
        return impl_->Fail("Runtime.Render.BackendInitFailed");
    impl_->viewportWidth = swapchain.width;
    impl_->viewportHeight = swapchain.height;
    impl_->window->SetRHIOwnsPresent(true);
    auto* session = impl_.get();
    impl_->window->SetOnResize([session](std::uint32_t width, std::uint32_t height)
    {
        if (!width || !height) return;
        session->viewportWidth = width;
        session->viewportHeight = height;
        session->backend->OnResize(width, height);
    });
    return impl_->UploadResources();
}

void RuntimeRenderSession::Shutdown()
{
    if (!impl_) return;
    if (impl_->window) impl_->window->SetOnResize({});
    if (impl_->backend)
    {
        for (const auto& [_, mesh] : impl_->meshes) impl_->backend->DestroyMesh(mesh);
        for (const auto& [_, material] : impl_->materials) impl_->backend->DestroyMaterial(material);
        impl_->backend->Shutdown();
        impl_->backend.reset();
    }
    if (impl_->input) { impl_->input->Shutdown(); impl_->input.reset(); }
    if (impl_->window) { impl_->window->Shutdown(); impl_->window.reset(); }
    impl_->platform.reset();
}

bool RuntimeRenderSession::ShouldClose() const noexcept
{ return !impl_->window || impl_->window->ShouldClose(); }

std::vector<std::string> RuntimeRenderSession::PollLogicalActions(
    const Play::RuntimeInputMapArtifact& inputMap, bool injectSmokeKey)
{
    if (injectSmokeKey)
    {
        const auto binding = std::find_if(inputMap.actions.begin(), inputMap.actions.end(),
            [](const auto& action) { return !action.bindings.empty(); });
        if (binding == inputMap.actions.end()) return {};
        const auto scancode = SDL_GetScancodeFromName(binding->bindings.front().control.c_str());
        if (scancode == SDL_SCANCODE_UNKNOWN) return {};
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.state = SDL_PRESSED;
        event.key.keysym.scancode = scancode;
        event.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
        SDL_PushEvent(&event);
    }
    impl_->window->PollEvents();
    SagaEngine::Input::RawInputFrame raw;
    impl_->input->PollRawEvents(raw);
    std::vector<std::string> actions;
    for (const auto& key : raw.keys)
        if (key.pressed && !key.repeat)
            for (const auto& action : inputMap.actions)
                for (const auto& binding : action.bindings)
                {
                    if (binding.device != "Keyboard" || binding.control.size() != 1) continue;
                    const auto expected = static_cast<SagaEngine::Input::KeyCode>(
                        static_cast<unsigned>(SagaEngine::Input::KeyCode::A) +
                        static_cast<unsigned>(binding.control.front() - 'A'));
                    if (key.code == expected) actions.push_back(action.actionId);
                }
    return actions;
}

SagaEngine::Render::Backend::IRenderOverlaySubmission*
RuntimeRenderSession::OverlaySubmission() noexcept
{
    return dynamic_cast<SagaEngine::Render::Backend::IRenderOverlaySubmission*>(impl_->backend.get());
}

std::uint32_t RuntimeRenderSession::Width() const noexcept
{ return impl_->window ? impl_->viewportWidth : 0; }
std::uint32_t RuntimeRenderSession::Height() const noexcept
{ return impl_->window ? impl_->viewportHeight : 0; }

bool RuntimeRenderSession::RequestResize(const std::uint32_t width, const std::uint32_t height)
{
    if (!impl_->window || width == 0 || height == 0) return false;
    SDL_SetWindowSize(static_cast<SDL_Window*>(impl_->window->GetNativeHandle()),
                      static_cast<int>(width), static_cast<int>(height));
    impl_->window->PollEvents();
    if (impl_->viewportWidth != width || impl_->viewportHeight != height)
    {
        impl_->viewportWidth = width;
        impl_->viewportHeight = height;
        impl_->backend->OnResize(width, height);
    }
    return true;
}

bool RuntimeRenderSession::RenderFrame(
    RuntimeUiSession* ui, const RuntimeEvidenceCaptureRequest* evidence)
{
    const auto transforms = impl_->WorldTransforms();
    SagaEngine::Render::Scene::RenderView view;
    std::uint32_t nextEntity = 0;
    const Play::RuntimeSceneEntity* cameraEntity = nullptr;
    const Play::RuntimeSceneEntity* lightEntity = nullptr;
    for (const auto& entity : impl_->world.Scene().entities)
    {
        if (entity.primaryCamera) cameraEntity = &entity;
        if (entity.directionalLight) lightEntity = &entity;
        if (!entity.meshRenderer) continue;
        SagaEngine::Render::Scene::DrawItem item;
        item.entity = static_cast<SagaEngine::Render::World::RenderEntityId>(nextEntity++);
        item.mesh = impl_->meshes.at(entity.meshRenderer->mesh.logicalId);
        item.material = impl_->materials.at(entity.meshRenderer->material.logicalId);
        item.model = SagaEngine::Math::Mat4::FromTransform(transforms.at(entity.entityId));
        view.drawItems.push_back(item);
    }
    if (!cameraEntity) return impl_->Fail("Runtime.Render.PrimaryCameraMissing");
    const auto& transform = transforms.at(cameraEntity->entityId);
    SagaEngine::Render::Scene::Camera camera;
    camera.position = transform.position;
    const auto forward = transform.rotation.Rotate({0.0f, 0.0f, -1.0f});
    const auto up = transform.rotation.Rotate({0.0f, 1.0f, 0.0f});
    camera.view = SagaEngine::Math::Mat4::LookAtRH(camera.position, camera.position + forward, up);
    constexpr float pi = 3.14159265358979323846f;
    camera.projection = SagaEngine::Math::Mat4::PerspectiveRH_ZO(
        static_cast<float>(cameraEntity->verticalFovDegrees) * pi / 180.0f,
        Height() ? static_cast<float>(Width()) / Height() : 1.0f,
        static_cast<float>(cameraEntity->nearClip), static_cast<float>(cameraEntity->farClip));
    camera.RecomputeViewProj();
    camera.RecomputeFrustum();
    if (lightEntity)
    {
        const auto& lightTransform = transforms.at(lightEntity->entityId);
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = lightEntity->castsShadows;
        view.lighting.directional.direction = lightTransform.rotation.Rotate({0.0f, -1.0f, 0.0f});
        view.lighting.directional.color = {static_cast<float>(lightEntity->lightColor[0]),
            static_cast<float>(lightEntity->lightColor[1]), static_cast<float>(lightEntity->lightColor[2])};
        view.lighting.directional.intensity = static_cast<float>(lightEntity->lightIntensity);
        view.lighting.ambient = {{1.0f, 1.0f, 1.0f}, 0.12f};
    }
    impl_->backend->BeginFrame();
    impl_->backend->Submit(camera, view);
    if (ui && !ui->PrepareLayout(Width(), Height(), 1.0F, 1.0 / 60.0))
        return impl_->Fail("Runtime.UI.RenderFailed");
    if (evidence)
    {
        if (!ui) return impl_->Fail("Runtime.Render.EvidenceUiMissing");
        auto frameEvidence = *evidence;
        const auto bounds = ui->PromptBounds();
        if (!bounds || !impl_->evidence.BeginNamedFrame(
                *impl_->backend, frameEvidence, *bounds, ui->PixelRatio()))
            return impl_->Fail("Runtime.Render.PreOverlayEvidenceCaptureFailed");
    }
    if (ui && !ui->SubmitOverlay()) return impl_->Fail("Runtime.UI.RenderFailed");
    if (evidence && !impl_->evidence.EndNamedFrame(
            *impl_->backend, ui->LastDrawCallCount()))
        return impl_->Fail("Runtime.Render.PostOverlayEvidenceCaptureFailed");
    impl_->backend->EndFrame();
    impl_->lastDrawCount = view.DrawCount();
    return true;
}

bool RuntimeRenderSession::HasMeaningfulPixels() const noexcept
{ return impl_->evidence.HasMeaningfulPixels(); }
std::size_t RuntimeRenderSession::LastDrawCount() const noexcept { return impl_->lastDrawCount; }
const std::vector<RuntimeEvidenceCaptureRecord>&
RuntimeRenderSession::EvidenceRecords() const noexcept { return impl_->evidence.Records(); }

} // namespace SagaRuntimeApp
