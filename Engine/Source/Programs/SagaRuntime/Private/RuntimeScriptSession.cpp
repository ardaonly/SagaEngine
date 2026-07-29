// SPDX-License-Identifier: MPL-2.0

#include "RuntimeScriptSession.h"

#include <cmath>
#include <algorithm>
#include <iterator>

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

Scripting::ScriptEntityCreateResult RuntimeScriptWorld::FindEntity(
    const std::string& stableId) const
{
    Scripting::ScriptEntityCreateResult result;
    result.entity = Handle(stableId);
    result.succeeded = result.entity.IsValid();
    return result;
}

Scripting::ScriptHostOperationResult RuntimeScriptWorld::SetActive(
    const Scripting::ScriptEntityHandle entity, const bool active)
{
    Scripting::ScriptHostOperationResult result;
    const auto index = entity.value == 0 ? scene_.entities.size() : entity.value - 1;
    if (index >= scene_.entities.size()) return result;
    scene_.entities[index].active = active;
    result.succeeded = true;
    return result;
}

Scripting::ScriptStateBoolResult RuntimeScriptWorld::GetActive(
    const Scripting::ScriptEntityHandle entity) const
{
    Scripting::ScriptStateBoolResult result;
    const auto index = entity.value == 0 ? scene_.entities.size() : entity.value - 1;
    if (index >= scene_.entities.size()) return result;
    result.value = scene_.entities[index].active;
    result.succeeded = true;
    return result;
}

void RuntimeScriptInputPort::Advance(
    std::set<std::string> pressed, std::set<std::string> held)
{
    released_.clear();
    std::set_difference(held_.begin(), held_.end(), held.begin(), held.end(),
        std::inserter(released_, released_.end()));
    pressed_ = std::move(pressed);
    held_ = std::move(held);
}

void RuntimeScriptInputPort::Clear()
{
    pressed_.clear();
    held_.clear();
    released_.clear();
}

bool RuntimeScriptInputPort::Allowed(
    const Scripting::ScriptStateContext& context, const std::string& action) const
{
    const auto behavior = policy_.find(context.behaviorId);
    return behavior != policy_.end() && behavior->second.contains(action);
}

Scripting::ScriptStateBoolResult RuntimeScriptInputPort::Query(
    const Scripting::ScriptStateContext& context, const std::string& action,
    const Scripting::ScriptLogicalInputState state) const
{
    Scripting::ScriptStateBoolResult result;
    if (!Allowed(context, action)) return result;
    result.succeeded = true;
    result.value = state == Scripting::ScriptLogicalInputState::Pressed
        ? pressed_.contains(action)
        : state == Scripting::ScriptLogicalInputState::Held
            ? held_.contains(action) : released_.contains(action);
    return result;
}

Scripting::ScriptStateDoubleResult RuntimeScriptInputPort::GetValue(
    const Scripting::ScriptStateContext& context, const std::string& action) const
{
    Scripting::ScriptStateDoubleResult result;
    if (!Allowed(context, action)) return result;
    result.succeeded = true;
    result.value = held_.contains(action) ? 1.0 : 0.0;
    return result;
}

Scripting::ScriptHostOperationResult RuntimeExitPort::RequestExit(
    const Scripting::ScriptStateContext&)
{
    requested_ = true;
    Scripting::ScriptHostOperationResult result;
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
