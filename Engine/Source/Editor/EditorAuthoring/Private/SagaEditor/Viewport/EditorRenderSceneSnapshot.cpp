// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Viewport/EditorRenderSceneSnapshot.h"

#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/SceneEditing/SceneDocument.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <set>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::json;
using Transform = SagaEngine::Math::Transform;

Transform LocalTransform(const SceneEntityReadModel& entity)
{
    Transform result;
    if (!entity.transform) return result;
    const auto& value = *entity.transform;
    result.position = {static_cast<float>(value.position[0]),
        static_cast<float>(value.position[1]), static_cast<float>(value.position[2])};
    result.rotation = {static_cast<float>(value.rotation[0]),
        static_cast<float>(value.rotation[1]), static_cast<float>(value.rotation[2]),
        static_cast<float>(value.rotation[3])};
    result.scale = {static_cast<float>(value.scale[0]),
        static_cast<float>(value.scale[1]), static_cast<float>(value.scale[2])};
    return result;
}

const SceneComponentReadModel* Component(const SceneEntityReadModel& entity,
                                         std::string_view type)
{
    const auto found = std::find_if(entity.components.begin(), entity.components.end(),
        [type](const auto& component) { return component.type == type; });
    return found == entity.components.end() ? nullptr : &*found;
}
}

EditorRenderSceneSnapshotResult BuildEditorRenderSceneSnapshot(const SceneDocument& document)
{
    EditorRenderSceneSnapshotResult result;
    result.snapshot.sceneId = document.SceneId();
    result.snapshot.revisionHash = HashDocumentBytes(document.Snapshot().baseDiskHash +
        std::to_string(document.Snapshot().inMemoryRevision));
    std::map<std::string, const SceneEntityReadModel*> byId;
    for (const auto& entity : document.Entities()) byId.emplace(entity.entityId, &entity);
    std::map<std::string, Transform> world;
    std::set<std::string> visiting;
    const auto resolveWorld = [&](const auto& self, const SceneEntityReadModel& entity) -> Transform
    {
        if (const auto found = world.find(entity.entityId); found != world.end()) return found->second;
        if (!visiting.insert(entity.entityId).second)
        {
            result.diagnostics.push_back("Scene snapshot encountered a parent cycle: " + entity.entityId);
            return {};
        }
        auto value = LocalTransform(entity);
        if (entity.parentEntityId)
        {
            const auto parent = byId.find(*entity.parentEntityId);
            if (parent == byId.end())
                result.diagnostics.push_back("Scene snapshot parent is missing: " + *entity.parentEntityId);
            else value = self(self, *parent->second).CombineWith(value);
        }
        visiting.erase(entity.entityId);
        world.emplace(entity.entityId, value);
        return value;
    };

    for (const auto& entity : document.Entities())
    {
        const auto worldTransform = resolveWorld(resolveWorld, entity);
        if (const auto* mesh = Component(entity, "MeshRenderer"))
        {
            try
            {
                const auto properties = Json::parse(mesh->canonicalProperties);
                EditorRenderEntitySnapshot renderEntity;
                renderEntity.sceneEntityId = entity.entityId;
                renderEntity.worldTransform = worldTransform;
                renderEntity.meshArtifactId = properties.value("meshAssetId", std::string{});
                renderEntity.materialArtifactId = properties.value("materialAssetId", std::string{});
                if (renderEntity.meshArtifactId.empty() || renderEntity.materialArtifactId.empty())
                    result.diagnostics.push_back("MeshRenderer lacks logical artifact identity: " + entity.entityId);
                else result.snapshot.entities.push_back(std::move(renderEntity));
            }
            catch (const Json::exception&)
            { result.diagnostics.push_back("MeshRenderer properties are malformed: " + entity.entityId); }
        }
        if (const auto* light = Component(entity, "DirectionalLight"))
        {
            try
            {
                const auto properties = Json::parse(light->canonicalProperties);
                const auto color = properties.at("color");
                EditorDirectionalLightSnapshot snapshot;
                snapshot.direction = worldTransform.rotation.Rotate({0.0f, -1.0f, 0.0f});
                snapshot.color = {color.at(0).get<float>(), color.at(1).get<float>(),
                    color.at(2).get<float>()};
                snapshot.intensity = properties.at("intensity").get<float>();
                result.snapshot.directionalLight = snapshot;
            }
            catch (const Json::exception&)
            { result.diagnostics.push_back("DirectionalLight properties are malformed: " + entity.entityId); }
        }
    }
    std::sort(result.snapshot.entities.begin(), result.snapshot.entities.end(),
        [](const auto& left, const auto& right)
        { return left.sceneEntityId < right.sceneEntityId; });
    return result;
}
} // namespace SagaEditor
