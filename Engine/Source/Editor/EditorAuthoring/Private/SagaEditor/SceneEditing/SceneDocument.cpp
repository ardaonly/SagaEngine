// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/SceneEditing/SceneDocument.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <set>
#include <map>
#include <string_view>
#include <unordered_map>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::json;

bool FiniteArray(const Json& value, std::size_t size)
{
    if (!value.is_array() || value.size() != size) return false;
    for (const auto& item : value)
        if (!item.is_number() || !std::isfinite(item.get<double>())) return false;
    return true;
}

bool ContainsOnly(const Json& value, std::initializer_list<std::string_view> allowed)
{
    for (const auto& [key, ignored] : value.items())
    {
        (void)ignored;
        bool found = false;
        for (const auto candidate : allowed)
            if (key == candidate) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

bool NonEmptyString(const Json& value, std::string_view key)
{
    const auto iterator = value.find(key);
    return iterator != value.end() && iterator->is_string() &&
        !iterator->get_ref<const std::string&>().empty();
}

bool StringArray(const Json& value)
{
    if (!value.is_array()) return false;
    for (const auto& item : value)
        if (!item.is_string() || item.get_ref<const std::string&>().empty()) return false;
    return true;
}

std::vector<DocumentDiagnostic> ValidateScene(const std::string& text)
{
    std::vector<DocumentDiagnostic> diagnostics;
    Json root;
    try { root = Json::parse(text); }
    catch (const Json::exception& error)
    {
        return {{"Scene.JsonInvalid", error.what()}};
    }
    if (!root.is_object() || !ContainsOnly(root, {"schemaVersion", "sceneId", "entities"}) ||
        root.value("schemaVersion", -1) != 1 ||
        !root.contains("sceneId") || !root["sceneId"].is_string() ||
        root["sceneId"].get<std::string>().empty() ||
        !root.contains("entities") || !root["entities"].is_array())
        return {{"Scene.SchemaInvalid", "Scene source must use schema version 1 with sceneId and entities."}};

    std::set<std::string> ids;
    std::unordered_map<std::string, std::string> parents;
    for (const auto& entity : root["entities"])
    {
        if (!entity.is_object() ||
            !ContainsOnly(entity, {"entityId", "name", "parentEntityId", "components"}) ||
            !NonEmptyString(entity, "entityId") || !NonEmptyString(entity, "name") ||
            !entity.contains("components") || !entity["components"].is_array())
            return {{"Scene.EntityInvalid", "Every entity requires entityId, name, and components."}};
        const auto id = entity["entityId"].get<std::string>();
        if (id.empty() || !ids.insert(id).second)
            return {{"Scene.EntityIdDuplicate", "Entity ids must be non-empty and unique."}};
        if (entity.contains("parentEntityId"))
        {
            if (!entity["parentEntityId"].is_string())
                return {{"Scene.ParentInvalid", "parentEntityId must be a string."}};
            parents[id] = entity["parentEntityId"].get<std::string>();
        }
        std::set<std::string> componentTypes;
        for (const auto& component : entity["components"])
        {
            if (!component.is_object() || !component.contains("type") || !component["type"].is_string())
                return {{"Scene.ComponentInvalid", "Every component requires a type."}};
            const auto type = component["type"].get<std::string>();
            if (!componentTypes.insert(type).second)
                return {{"Scene.ComponentDuplicate", "Singular component types may occur only once per entity."}};
            if (type == "Transform")
            {
                if (!ContainsOnly(component, {"type", "position", "rotation", "scale"}) ||
                    !FiniteArray(component.value("position", Json{}), 3) ||
                    !FiniteArray(component.value("rotation", Json{}), 4) ||
                    !FiniteArray(component.value("scale", Json{}), 3))
                    return {{"Scene.TransformInvalid", "Transform vectors must be finite and correctly sized."}};
            }
            else if (type == "Camera")
            {
                if (!ContainsOnly(component, {"type", "projection", "primary", "verticalFovDegrees", "nearClip", "farClip"}) ||
                    component.value("projection", std::string{}) != "Perspective" ||
                    !component.contains("primary") || !component["primary"].is_boolean() ||
                    !component.contains("verticalFovDegrees") || !component["verticalFovDegrees"].is_number() ||
                    !component.contains("nearClip") || !component["nearClip"].is_number() ||
                    !component.contains("farClip") || !component["farClip"].is_number())
                    return {{"Scene.CameraInvalid", "Camera values are invalid."}};
                const auto fov = component["verticalFovDegrees"].get<double>();
                const auto nearClip = component["nearClip"].get<double>();
                const auto farClip = component["farClip"].get<double>();
                if (!std::isfinite(fov) || !std::isfinite(nearClip) || !std::isfinite(farClip) ||
                    fov <= 0 || fov >= 180 || nearClip <= 0 || farClip <= nearClip)
                    return {{"Scene.CameraInvalid", "Camera values are invalid."}};
            }
            else if (type == "DirectionalLight")
            {
                if (!ContainsOnly(component, {"type", "color", "intensity", "castsShadows"}) ||
                    !FiniteArray(component.value("color", Json{}), 3) ||
                    !component.contains("intensity") || !component["intensity"].is_number() ||
                    !std::isfinite(component["intensity"].get<double>()) ||
                    component["intensity"].get<double>() < 0 ||
                    !component.contains("castsShadows") || !component["castsShadows"].is_boolean())
                    return {{"Scene.DirectionalLightInvalid", "DirectionalLight values are invalid."}};
                for (const auto& channel : component["color"])
                    if (channel.get<double>() < 0)
                        return {{"Scene.DirectionalLightInvalid", "DirectionalLight color cannot be negative."}};
            }
            else if (type == "MeshRenderer")
            {
                if (!ContainsOnly(component, {"type", "meshAssetId", "materialAssetId"}) ||
                    !NonEmptyString(component, "meshAssetId") ||
                    !NonEmptyString(component, "materialAssetId"))
                    return {{"Scene.MeshRendererInvalid", "MeshRenderer asset identities are required."}};
            }
            else if (type == "ScriptBehavior")
            {
                if (!ContainsOnly(component, {"type", "behaviorTypeId", "authoringSource", "instanceConfiguration", "requiredCapabilities"}) ||
                    !NonEmptyString(component, "behaviorTypeId") ||
                    !NonEmptyString(component, "authoringSource") ||
                    !component.contains("instanceConfiguration") || !component["instanceConfiguration"].is_object() ||
                    !component.contains("requiredCapabilities") || !StringArray(component["requiredCapabilities"]))
                    return {{"Scene.ScriptBehaviorInvalid", "ScriptBehavior values are invalid."}};
            }
            else if (type == "ScriptActionBinding")
            {
                if (!ContainsOnly(component, {"type", "inputActionId", "behaviorTypeId", "namedActionId"}) ||
                    !NonEmptyString(component, "inputActionId") ||
                    !NonEmptyString(component, "behaviorTypeId") ||
                    !NonEmptyString(component, "namedActionId"))
                    return {{"Scene.ScriptActionBindingInvalid", "ScriptActionBinding identities are required."}};
            }
            else
                return {{"Scene.ComponentUnknown", "Unknown scene component: " + type}};
        }
    }
    for (const auto& [id, parent] : parents)
        if (!ids.contains(parent))
            return {{"Scene.ParentMissing", "Every parentEntityId must identify an entity in the scene."}};
    for (const auto& [id, parent] : parents)
    {
        std::set<std::string> visited{id};
        auto current = parent;
        while (parents.contains(current))
        {
            if (!visited.insert(current).second)
                return {{"Scene.ParentCycle", "Entity parenting must be acyclic."}};
            current = parents.at(current);
        }
    }
    return diagnostics;
}

template <std::size_t N>
std::array<double, N> ReadArray(const Json& value)
{
    std::array<double, N> result{};
    for (std::size_t i = 0; i < N; ++i) result[i] = value[i].get<double>();
    return result;
}

Json* FindEntityJson(Json& root, std::string_view id)
{
    for (auto& entity : root["entities"])
        if (entity.value("entityId", std::string{}) == id) return &entity;
    return nullptr;
}
}

struct SceneDocument::Impl
{
    TextDocument text;
    Json root;
    std::string sceneId;
    std::vector<SceneEntityReadModel> entities;
    std::map<ChangeSubscription, ChangeCallback> callbacks;
    ChangeSubscription nextSubscription = 1;

    explicit Impl(std::filesystem::path path)
        : text("scene:" + path.filename().string(), DocumentKind::Scene,
               std::move(path), 1, true, ValidateScene) {}
};

SceneDocument::SceneDocument(std::filesystem::path sourcePath)
    : impl_(std::make_unique<Impl>(std::move(sourcePath))) {}
SceneDocument::~SceneDocument() = default;

const DocumentSnapshot& SceneDocument::Snapshot() const noexcept { return impl_->text.Snapshot(); }
const std::vector<DocumentDiagnostic>& SceneDocument::Diagnostics() const noexcept { return impl_->text.Diagnostics(); }
const std::string& SceneDocument::SceneId() const noexcept { return impl_->sceneId; }
const std::vector<SceneEntityReadModel>& SceneDocument::Entities() const noexcept { return impl_->entities; }

const SceneEntityReadModel* SceneDocument::FindEntity(std::string_view entityId) const noexcept
{
    for (const auto& entity : impl_->entities)
        if (entity.entityId == entityId) return &entity;
    return nullptr;
}

bool SceneDocument::RefreshReadModel()
{
    try { impl_->root = Json::parse(impl_->text.Text()); }
    catch (const Json::exception&) { return false; }
    impl_->sceneId = impl_->root["sceneId"].get<std::string>();
    impl_->entities.clear();
    for (const auto& source : impl_->root["entities"])
    {
        SceneEntityReadModel entity;
        entity.entityId = source["entityId"].get<std::string>();
        entity.name = source["name"].get<std::string>();
        if (source.contains("parentEntityId"))
            entity.parentEntityId = source["parentEntityId"].get<std::string>();
        for (const auto& component : source["components"])
        {
            const auto type = component["type"].get<std::string>();
            entity.components.push_back({type, component.dump()});
            if (type == "Transform")
            {
                SceneTransformReadModel transform;
                transform.position = ReadArray<3>(component["position"]);
                transform.rotation = ReadArray<4>(component["rotation"]);
                transform.scale = ReadArray<3>(component["scale"]);
                entity.transform = transform;
            }
        }
        impl_->entities.push_back(std::move(entity));
    }
    const auto callbacks = impl_->callbacks;
    for (const auto& [subscription, callback] : callbacks)
    {
        (void)subscription;
        if (callback) callback();
    }
    return true;
}

bool SceneDocument::Load() { return impl_->text.Load() && RefreshReadModel(); }

bool SceneDocument::RenameEntity(std::string_view entityId, std::string name)
{
    if (name.empty()) return false;
    auto next = impl_->root;
    auto* entity = FindEntityJson(next, entityId);
    if (!entity) return false;
    (*entity)["name"] = std::move(name);
    if (!impl_->text.ReplaceText(next.dump(2) + "\n")) return false;
    return RefreshReadModel();
}

bool SceneDocument::SetTransform(std::string_view entityId,
                                 const SceneTransformReadModel& transform)
{
    auto finite = [](double value) { return std::isfinite(value); };
    for (double value : transform.position) if (!finite(value)) return false;
    for (double value : transform.rotation) if (!finite(value)) return false;
    for (double value : transform.scale) if (!finite(value)) return false;
    auto next = impl_->root;
    auto* entity = FindEntityJson(next, entityId);
    if (!entity) return false;
    for (auto& component : (*entity)["components"])
    {
        if (component.value("type", std::string{}) != "Transform") continue;
        component["position"] = transform.position;
        component["rotation"] = transform.rotation;
        component["scale"] = transform.scale;
        if (!impl_->text.ReplaceText(next.dump(2) + "\n")) return false;
        return RefreshReadModel();
    }
    return false;
}

DocumentSaveStatus SceneDocument::Save() { return impl_->text.Save(); }
bool SceneDocument::Reload() { return impl_->text.Reload() && RefreshReadModel(); }
DocumentExternalChangeState SceneDocument::CheckExternalChange() { return impl_->text.CheckExternalChange(); }
DocumentCloseStatus SceneDocument::RequestClose() const noexcept { return impl_->text.RequestClose(); }
bool SceneDocument::CanUndo() const noexcept { return impl_->text.CanUndo(); }
bool SceneDocument::CanRedo() const noexcept { return impl_->text.CanRedo(); }
bool SceneDocument::Undo() { return impl_->text.Undo() && RefreshReadModel(); }
bool SceneDocument::Redo() { return impl_->text.Redo() && RefreshReadModel(); }

SceneDocument::ChangeSubscription SceneDocument::SubscribeChanges(ChangeCallback callback)
{
    if (!callback) return InvalidChangeSubscription;
    const auto subscription = impl_->nextSubscription++;
    impl_->callbacks.emplace(subscription, std::move(callback));
    return subscription;
}

void SceneDocument::UnsubscribeChanges(ChangeSubscription subscription)
{
    impl_->callbacks.erase(subscription);
}

} // namespace SagaEditor
