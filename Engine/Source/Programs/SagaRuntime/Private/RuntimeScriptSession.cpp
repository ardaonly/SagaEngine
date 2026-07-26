// SPDX-License-Identifier: MPL-2.0

#include "RuntimeScriptSession.h"

#include <cmath>

namespace SagaRuntimeApp
{
namespace Scripting = SagaEngine::Scripting;
namespace Play = SagaRuntime::Play;

RuntimeScriptWorld::RuntimeScriptWorld(Play::RuntimeSceneArtifact scene)
    : scene_(std::move(scene))
{
    for (std::size_t index = 0; index < scene_.entities.size(); ++index)
        byId_.emplace(scene_.entities[index].entityId, index);
}

Scripting::ScriptEntityHandle RuntimeScriptWorld::Handle(std::string_view id) const
{
    const auto item = byId_.find(std::string(id));
    return item == byId_.end() ? Scripting::ScriptEntityHandle{} :
        Scripting::ScriptEntityHandle{item->second + 1};
}

Scripting::ScriptEntityCreateResult RuntimeScriptWorld::CreateEntity(const std::string& name)
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

Scripting::ScriptHostOperationResult RuntimeScriptWorld::SetPosition(
    Scripting::ScriptEntityHandle entity, Scripting::ScriptVector3 position)
{
    Scripting::ScriptHostOperationResult result;
    const auto index = entity.value == 0 ? scene_.entities.size() : entity.value - 1;
    if (index >= scene_.entities.size() || !std::isfinite(position.x) ||
        !std::isfinite(position.y) || !std::isfinite(position.z)) return result;
    scene_.entities[index].transform.position = {position.x, position.y, position.z};
    result.succeeded = true;
    return result;
}

Scripting::ScriptEntityPositionResult RuntimeScriptWorld::GetPosition(
    Scripting::ScriptEntityHandle entity) const
{
    Scripting::ScriptEntityPositionResult result;
    const auto index = entity.value == 0 ? scene_.entities.size() : entity.value - 1;
    if (index >= scene_.entities.size()) return result;
    const auto& position = scene_.entities[index].transform.position;
    result.position = {static_cast<float>(position[0]), static_cast<float>(position[1]),
                       static_cast<float>(position[2])};
    result.succeeded = true;
    return result;
}

RuntimeNamedActionTarget::RuntimeNamedActionTarget(Scripting::CSharpScriptHost& host,
    std::map<std::pair<std::string, std::string>, Scripting::ScriptInstanceHandle> instances,
    std::map<std::pair<std::string, std::string>, std::string> methods)
    : host_(host), instances_(std::move(instances)), methods_(std::move(methods)) {}

bool RuntimeNamedActionTarget::InvokeNamedAction(std::string_view behaviorTypeId,
    std::string_view namedActionId, std::string_view entityId)
{
    const auto instance = instances_.find({std::string(entityId), std::string(behaviorTypeId)});
    const auto method = methods_.find({std::string(behaviorTypeId), std::string(namedActionId)});
    if (instance == instances_.end() || method == methods_.end()) return false;
    return host_.InvokeNamedAction(
        {instance->second, method->second, std::string(namedActionId)}).Succeeded();
}

} // namespace SagaRuntimeApp
