// SPDX-License-Identifier: MPL-2.0
#include "RuntimeStateStore.h"

#include <tuple>

namespace SagaRuntimeApp
{
namespace Scripting = SagaEngine::Scripting;

bool RuntimeStateStore::InstanceIdentity::operator<(
    const InstanceIdentity& other) const noexcept
{
    return std::tie(entity, behavior) < std::tie(other.entity, other.behavior);
}

bool RuntimeStateStore::StateIdentity::operator<(
    const StateIdentity& other) const noexcept
{
    return std::tie(instance, key) < std::tie(other.instance, other.key);
}

std::optional<RuntimeBoolStateSnapshot> RuntimeStateStore::Snapshot(
    const Scripting::ScriptEntityHandle entity, const std::string& behaviorId,
    const std::string& key) const
{
    const InstanceIdentity instance{entity.value, behaviorId};
    const auto active = active_.find(instance);
    const auto state = bools_.find({instance, key});
    if (active == active_.end() || state == bools_.end() ||
        active->second != state->second.activeInstanceToken) return std::nullopt;
    return state->second;
}

Scripting::ScriptStateBoolResult RuntimeStateStore::GetBool(const std::string&) const
{ return {}; }
Scripting::ScriptStateInt32Result RuntimeStateStore::AddInt32(const std::string&, std::int32_t)
{ return {}; }
Scripting::ScriptHostOperationResult RuntimeStateStore::SetBool(const std::string&, bool)
{ return {}; }
Scripting::ScriptHostOperationResult RuntimeStateStore::SetString(
    const std::string&, const std::string&)
{ return {}; }

bool RuntimeStateStore::IsActive(const Context& context) const
{
    const auto active = active_.find({context.entity.value, context.behaviorId});
    return context.instance.IsValid() && context.entity.IsValid() &&
        context.activeInstanceToken != 0 && active != active_.end() &&
        active->second == context.activeInstanceToken;
}

Scripting::ScriptStateBoolResult RuntimeStateStore::GetBool(
    const Context& context, const std::string& key) const
{
    Scripting::ScriptStateBoolResult result;
    const auto snapshot = Snapshot(context.entity, context.behaviorId, key);
    if (!IsActive(context) || !snapshot) return result;
    result.succeeded = true;
    result.value = snapshot->value;
    return result;
}

Scripting::ScriptStateInt32Result RuntimeStateStore::AddInt32(
    const Context&, const std::string&, std::int32_t)
{ return {}; }

Scripting::ScriptHostOperationResult RuntimeStateStore::SetBool(
    const Context& context, const std::string& key, const bool value)
{
    Scripting::ScriptHostOperationResult result;
    if (!IsActive(context) || key != "door.target-open") return result;
    bools_[{{context.entity.value, context.behaviorId}, key}] =
        {value, nextRevision_++, context.activeInstanceToken};
    result.succeeded = true;
    return result;
}

Scripting::ScriptHostOperationResult RuntimeStateStore::SetString(
    const Context&, const std::string&, const std::string&)
{ return {}; }

void RuntimeStateStore::OnInstanceActivated(const Context& context)
{
    if (!context.instance.IsValid() || !context.entity.IsValid() ||
        context.behaviorId.empty() || context.activeInstanceToken == 0) return;
    active_[{context.entity.value, context.behaviorId}] = context.activeInstanceToken;
}

void RuntimeStateStore::OnInstanceDeactivated(const Context& context)
{
    const InstanceIdentity identity{context.entity.value, context.behaviorId};
    const auto active = active_.find(identity);
    if (active == active_.end() || active->second != context.activeInstanceToken) return;
    active_.erase(active);
    for (auto item = bools_.begin(); item != bools_.end();)
        if (item->first.instance.entity == identity.entity &&
            item->first.instance.behavior == identity.behavior)
            item = bools_.erase(item);
        else ++item;
}

} // namespace SagaRuntimeApp
