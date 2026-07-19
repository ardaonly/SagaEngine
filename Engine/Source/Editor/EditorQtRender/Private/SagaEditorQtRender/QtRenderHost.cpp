// SPDX-License-Identifier: MPL-2.0

#include "SagaEditorQtRender/QtRenderHost.h"

#include "SagaEditor/Viewport/ViewportRay.h"
#include "SagaEngine/Math/Mat4.h"
#include "SagaEngine/Render/Assets/CookedRenderAssetLoader.h"
#include "SagaEngine/Render/Backend/RenderBackendFactory.h"
#include "SagaEngine/Render/Materials/Material.h"
#include "SagaEngine/Render/Scene/RenderView.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <stdexcept>

namespace SagaEditor
{
namespace
{
using Backend = SagaEngine::Render::Backend::IRenderBackend;
using MeshId = SagaEngine::Render::World::MeshId;
using MaterialId = SagaEngine::Render::World::MaterialId;
using RenderEntityId = SagaEngine::Render::World::RenderEntityId;
using Json = nlohmann::json;

bool IsContained(const std::filesystem::path& root, const std::filesystem::path& path)
{
    std::error_code error;
    const auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
    if (error) return false;
    const auto canonicalPath = std::filesystem::weakly_canonical(path, error);
    if (error) return false;
    const auto relative = canonicalPath.lexically_relative(canonicalRoot);
    return !error && !relative.empty() && !relative.is_absolute() &&
        *relative.begin() != std::filesystem::path("..");
}
}

struct QtRenderHost::Impl
{
    struct ResourceEntity
    {
        EditorRenderEntitySnapshot source;
        MeshId mesh = MeshId::kInvalid;
        MaterialId material = MaterialId::kInvalid;
        RenderEntityId renderId = RenderEntityId::kInvalid;
    };

    BackendFactory factory;
    std::unique_ptr<Backend> backend;
    QtRenderSurface surface;
    EditorRenderSceneSnapshot snapshot;
    std::filesystem::path contentRoot;
    std::vector<ResourceEntity> entities;
    std::map<std::string, MeshId> meshes;
    std::map<std::string, MaterialId> materials;
    std::map<std::string, MaterialId> highlightedMaterials;
    std::string highlighted;
    QtRenderHostStatus status;

    explicit Impl(BackendFactory value) : factory(std::move(value))
    {
        if (!factory) factory = [] { return SagaEngine::Render::Backend::CreateRenderBackend(); };
    }

    void Fail(std::string message) { status.diagnostics.push_back(std::move(message)); }

    void ResetSceneResources()
    {
        if (backend)
        {
            for (const auto& [_, mesh] : meshes) backend->DestroyMesh(mesh);
            for (const auto& [_, material] : materials) backend->DestroyMaterial(material);
            for (const auto& [_, material] : highlightedMaterials)
                backend->DestroyMaterial(material);
        }
        meshes.clear();
        materials.clear();
        highlightedMaterials.clear();
        entities.clear();
    }

    bool UploadScene()
    {
        if (!backend || !status.backendReady || contentRoot.empty()) return false;
        ResetSceneResources();
        std::ifstream input(contentRoot / "manifests/current.json", std::ios::binary);
        Json manifest;
        try { if (!input) throw std::runtime_error("missing current manifest"); input >> manifest; }
        catch (const std::exception& error)
        { Fail(std::string("EditorRender.ContentManifestInvalid: ") + error.what()); return false; }
        if (manifest.value("schemaVersion", -1) != 1 || !manifest.contains("artifacts") ||
            !manifest["artifacts"].is_array())
        { Fail("EditorRender.ContentManifestInvalid: schemaVersion 1 artifacts are required."); return false; }
        std::map<std::string, std::filesystem::path> paths;
        for (const auto& artifact : manifest["artifacts"])
        {
            const auto path = contentRoot / artifact.value("path", std::string{});
            if (!IsContained(contentRoot, path))
            { Fail("EditorRender.ContentPathEscaped"); return false; }
            paths.emplace(artifact.value("artifactId", std::string{}), path);
        }
        for (const auto& source : snapshot.entities)
        {
            if (!meshes.contains(source.meshArtifactId))
            {
                const auto path = paths.find(source.meshArtifactId);
                if (path == paths.end()) { Fail("EditorRender.MeshMissing: " + source.meshArtifactId); return false; }
                const auto loaded = SagaEngine::Render::LoadCookedMeshArtifact(path->second);
                if (!loaded.Succeeded()) { Fail("EditorRender.MeshInvalid: " + source.meshArtifactId); return false; }
                const auto handle = backend->CreateMesh(loaded.asset);
                if (handle == MeshId::kInvalid) { Fail("EditorRender.MeshUploadFailed"); return false; }
                meshes.emplace(source.meshArtifactId, handle);
            }
            if (!materials.contains(source.materialArtifactId))
            {
                const auto path = paths.find(source.materialArtifactId);
                if (path == paths.end()) { Fail("EditorRender.MaterialMissing: " + source.materialArtifactId); return false; }
                const auto loaded = SagaEngine::Render::LoadCookedMaterialArtifact(path->second,
                    static_cast<SagaEngine::Render::ShaderTemplateHandle>(1));
                if (!loaded.Succeeded()) { Fail("EditorRender.MaterialInvalid: " + source.materialArtifactId); return false; }
                const auto handle = backend->CreateMaterial(loaded.material);
                if (handle == MaterialId::kInvalid) { Fail("EditorRender.MaterialUploadFailed"); return false; }
                materials.emplace(source.materialArtifactId, handle);

                auto selectedMaterial = loaded.material;
                selectedMaterial.materialId ^= 0x53454c4543544544ULL;
                selectedMaterial.vectors[0] = {1.0f, 0.72f, 0.08f, 1.0f};
                const auto selectedHandle = backend->CreateMaterial(selectedMaterial);
                if (selectedHandle == MaterialId::kInvalid)
                { Fail("EditorRender.HighlightMaterialUploadFailed"); return false; }
                highlightedMaterials.emplace(source.materialArtifactId, selectedHandle);
            }
        }
        entities.clear();
        std::uint32_t nextId = 0;
        for (const auto& source : snapshot.entities)
            entities.push_back({source, meshes.at(source.meshArtifactId),
                materials.at(source.materialArtifactId), static_cast<RenderEntityId>(nextId++)});
        return true;
    }

    SagaEngine::Render::Scene::Camera Camera(const ViewportCamera& source) const
    {
        SagaEngine::Render::Scene::Camera camera;
        camera.position = source.GetPosition();
        camera.view = SagaEngine::Math::Mat4::LookAtRH(camera.position, source.pivot, source.GetUp());
        const float aspect = surface.height ? static_cast<float>(surface.width) / surface.height : 1.0f;
        camera.projection = source.mode == ProjectionMode::Perspective ?
            SagaEngine::Math::Mat4::PerspectiveRH_ZO(source.fovYRadians, aspect,
                source.nearPlane, source.farPlane) :
            SagaEngine::Math::Mat4::OrthoRH_ZO(-source.orthoHalfHeight * aspect,
                source.orthoHalfHeight * aspect, -source.orthoHalfHeight,
                source.orthoHalfHeight, source.nearPlane, source.farPlane);
        camera.RecomputeViewProj();
        camera.RecomputeFrustum();
        return camera;
    }

    SagaEngine::Render::Scene::RenderView BuildView() const
    {
        SagaEngine::Render::Scene::RenderView view;
        for (const auto& entity : entities)
        {
            SagaEngine::Render::Scene::DrawItem item;
            item.entity = entity.renderId;
            item.mesh = entity.mesh;
            item.material = entity.source.sceneEntityId == highlighted ?
                highlightedMaterials.at(entity.source.materialArtifactId) : entity.material;
            item.model = SagaEngine::Math::Mat4::FromTransform(entity.source.worldTransform);
            view.drawItems.push_back(item);
        }
        if (snapshot.directionalLight)
        {
            view.lighting.directionalEnabled = true;
            view.lighting.directional.direction = snapshot.directionalLight->direction;
            view.lighting.directional.color = snapshot.directionalLight->color;
            view.lighting.directional.intensity = snapshot.directionalLight->intensity;
            view.lighting.ambient = {{1.0f, 1.0f, 1.0f}, 0.12f};
        }
        return view;
    }
};

QtRenderHost::QtRenderHost(BackendFactory factory) : impl_(std::make_unique<Impl>(std::move(factory))) {}
QtRenderHost::~QtRenderHost() { Destroy(); }

bool QtRenderHost::Attach(const QtRenderSurface& surface)
{
    if (impl_->status.state == QtRenderHostState::Destroyed || surface.width == 0 ||
        surface.height == 0 || surface.native.backend == Saga::NativeWindowBackend::Unknown)
    { impl_->Fail("EditorRender.SurfaceInvalid"); return false; }
    impl_->surface = surface;
    impl_->backend = impl_->factory();
    SagaEngine::Render::Backend::SwapchainDesc desc;
    desc.nativeWindow = &impl_->surface.native;
    desc.width = surface.width; desc.height = surface.height; desc.vsync = true;
    if (!impl_->backend || !impl_->backend->Initialize(desc))
    { impl_->backend.reset(); impl_->Fail("EditorRender.BackendInitializeFailed"); return false; }
    impl_->status.state = QtRenderHostState::Attached;
    impl_->status.backendReady = true;
    if (!impl_->snapshot.sceneId.empty()) return impl_->UploadScene();
    return true;
}

void QtRenderHost::Resize(std::uint32_t width, std::uint32_t height, float ratio)
{
    impl_->surface.width = width; impl_->surface.height = height;
    impl_->surface.devicePixelRatio = std::max(ratio, 1.0f);
    if (impl_->backend && impl_->status.backendReady && width && height)
        impl_->backend->OnResize(width, height);
}
void QtRenderHost::Show()
{ if (impl_->status.backendReady) impl_->status.state = QtRenderHostState::Attached; }
void QtRenderHost::Hide()
{ if (impl_->status.backendReady) impl_->status.state = QtRenderHostState::Hidden; }

bool QtRenderHost::LoadScene(const EditorRenderSceneSnapshot& snapshot,
                             const std::filesystem::path& cookedContentRoot)
{
    impl_->snapshot = snapshot; impl_->contentRoot = cookedContentRoot;
    impl_->status.diagnostics.clear();
    return !impl_->status.backendReady || impl_->UploadScene();
}

bool QtRenderHost::RequestFrame(const ViewportCamera& camera)
{
    if (!impl_->backend || !impl_->status.backendReady ||
        impl_->status.state != QtRenderHostState::Attached) return false;
    const auto view = impl_->BuildView();
    const auto renderCamera = impl_->Camera(camera);
    impl_->backend->BeginFrame(); impl_->backend->Submit(renderCamera, view); impl_->backend->EndFrame();
    ++impl_->status.framesRendered; impl_->status.drawCount = view.DrawCount();
    return true;
}

bool QtRenderFrameEvidence::HasMeaningfulPixels() const noexcept
{
    if (width == 0 || height == 0 || rgba8.size() !=
        static_cast<std::size_t>(width) * height * 4U) return false;
    const auto first = std::array<std::uint8_t, 4>{rgba8[0], rgba8[1], rgba8[2], rgba8[3]};
    for (std::size_t index = 4; index + 3 < rgba8.size(); index += 4)
        if (rgba8[index] != first[0] || rgba8[index + 1] != first[1] ||
            rgba8[index + 2] != first[2] || rgba8[index + 3] != first[3]) return true;
    return false;
}

std::optional<QtRenderFrameEvidence> QtRenderHost::CaptureFrame(
    const ViewportCamera& camera)
{
    if (!impl_->backend || !impl_->status.backendReady ||
        impl_->status.state != QtRenderHostState::Attached) return std::nullopt;
    const auto view = impl_->BuildView();
    const auto renderCamera = impl_->Camera(camera);
    impl_->backend->BeginFrame();
    impl_->backend->Submit(renderCamera, view);
    SagaEngine::Render::Backend::RenderFrameCapture capture;
    const bool captured = impl_->backend->CaptureCurrentColorFrame(capture);
    impl_->backend->EndFrame();
    ++impl_->status.framesRendered;
    impl_->status.drawCount = view.DrawCount();
    if (!captured) return std::nullopt;
    return QtRenderFrameEvidence{capture.width, capture.height, std::move(capture.pixels)};
}

std::optional<std::string> QtRenderHost::Pick(float x, float y,
                                               const ViewportCamera& camera) const
{
    const auto ray = ScreenToWorldRay(camera, x, y,
        static_cast<float>(impl_->surface.width), static_cast<float>(impl_->surface.height));
    std::optional<std::pair<float, std::string>> nearest;
    for (const auto& entity : impl_->entities)
        if (const auto distance = IntersectRaySphere(ray, entity.source.worldTransform.position,
                entity.source.boundsRadius); distance && (!nearest || *distance < nearest->first))
            nearest = std::pair<float, std::string>{*distance, entity.source.sceneEntityId};
    return nearest ? std::optional<std::string>(nearest->second) : std::nullopt;
}
void QtRenderHost::SetHighlightedEntity(std::string id) { impl_->highlighted = std::move(id); }
void QtRenderHost::DeviceLost()
{ Detach(); impl_->status.state = QtRenderHostState::DeviceLost; }
bool QtRenderHost::Recover(const QtRenderSurface& surface)
{ return impl_->status.state == QtRenderHostState::DeviceLost && Attach(surface); }
void QtRenderHost::Detach()
{
    impl_->ResetSceneResources();
    if (impl_->backend) impl_->backend->Shutdown();
    impl_->backend.reset();
    impl_->status.backendReady = false;
    if (impl_->status.state != QtRenderHostState::Destroyed)
        impl_->status.state = QtRenderHostState::Detached;
}
void QtRenderHost::Destroy()
{ if (impl_->status.state != QtRenderHostState::Destroyed) { Detach(); impl_->status.state = QtRenderHostState::Destroyed; } }
const QtRenderHostStatus& QtRenderHost::Status() const noexcept { return impl_->status; }
} // namespace SagaEditor
