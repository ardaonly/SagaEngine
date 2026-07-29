// SPDX-License-Identifier: MPL-2.0
#include "RuntimeStateStore.h"

#include <tuple>
#include <cmath>
#include <limits>

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
    const auto typed = SnapshotValue(entity, behaviorId, key);
    if (!typed || !std::holds_alternative<bool>(typed->value)) return std::nullopt;
    return RuntimeBoolStateSnapshot{std::get<bool>(typed->value), typed->revision,
                                    typed->activeInstanceToken};
}

std::optional<RuntimeTypedStateSnapshot> RuntimeStateStore::SnapshotValue(
    const Scripting::ScriptEntityHandle entity, const std::string& behaviorId,
    const std::string& key) const
{
    const InstanceIdentity instance{entity.value, behaviorId};
    const auto active = active_.find(instance);
    const auto state = values_.find({instance, key});
    if (active == active_.end() || state == values_.end() ||
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
    const Context& context, const std::string& key, const std::int32_t delta)
{
    Scripting::ScriptStateInt32Result result;
    const auto policy = typedWrites_.find(context.behaviorId);
    if (!IsActive(context) || policy == typedWrites_.end() ||
        !policy->second.contains(key) || policy->second.at(key) != "Int32") return result;
    std::int32_t current = 0;
    if (const auto snapshot = SnapshotValue(context.entity, context.behaviorId, key))
    {
        if (!std::holds_alternative<std::int32_t>(snapshot->value)) return result;
        current = std::get<std::int32_t>(snapshot->value);
    }
    const auto sum = static_cast<std::int64_t>(current) + delta;
    if (sum < std::numeric_limits<std::int32_t>::min() ||
        sum > std::numeric_limits<std::int32_t>::max()) return result;
    result.value = static_cast<std::int32_t>(sum);
    values_[{{context.entity.value, context.behaviorId}, key}] =
        {result.value, nextRevision_++, context.activeInstanceToken};
    result.succeeded = true;
    return result;
}

Scripting::ScriptHostOperationResult RuntimeStateStore::SetBool(
    const Context& context, const std::string& key, const bool value)
{
    Scripting::ScriptHostOperationResult result;
    const auto behavior = boolWrites_.find(context.behaviorId);
    const auto typed = typedWrites_.find(context.behaviorId);
    const bool legacyAllowed =
        behavior != boolWrites_.end() && behavior->second.contains(key);
    const bool typedAllowed = typed != typedWrites_.end() &&
        typed->second.contains(key) && typed->second.at(key) == "Bool";
    if (!IsActive(context) || (!legacyAllowed && !typedAllowed)) return result;
    values_[{{context.entity.value, context.behaviorId}, key}] =
        {value, nextRevision_++, context.activeInstanceToken};
    result.succeeded = true;
    return result;
}

Scripting::ScriptHostOperationResult RuntimeStateStore::SetString(
    const Context& context, const std::string& key, const std::string& value)
{
    Scripting::ScriptHostOperationResult result;
    const auto policy = typedWrites_.find(context.behaviorId);
    if (!IsActive(context) || policy == typedWrites_.end() ||
        !policy->second.contains(key) || policy->second.at(key) != "String") return result;
    values_[{{context.entity.value, context.behaviorId}, key}] =
        {value, nextRevision_++, context.activeInstanceToken};
    result.succeeded = true;
    return result;
}

Scripting::ScriptStateInt32Result RuntimeStateStore::GetInt32(
    const Context& context, const std::string& key) const
{
    Scripting::ScriptStateInt32Result result;
    const auto value = SnapshotValue(context.entity, context.behaviorId, key);
    if (!IsActive(context) || !value ||
        !std::holds_alternative<std::int32_t>(value->value)) return result;
    result.value = std::get<std::int32_t>(value->value);
    result.succeeded = true;
    return result;
}

Scripting::ScriptStateDoubleResult RuntimeStateStore::GetDouble(
    const Context& context, const std::string& key) const
{
    Scripting::ScriptStateDoubleResult result;
    const auto value = SnapshotValue(context.entity, context.behaviorId, key);
    if (!IsActive(context) || !value ||
        !std::holds_alternative<double>(value->value)) return result;
    result.value = std::get<double>(value->value);
    result.succeeded = true;
    return result;
}

Scripting::ScriptStateStringResult RuntimeStateStore::GetString(
    const Context& context, const std::string& key) const
{
    Scripting::ScriptStateStringResult result;
    const auto value = SnapshotValue(context.entity, context.behaviorId, key);
    if (!IsActive(context) || !value ||
        !std::holds_alternative<std::string>(value->value)) return result;
    result.value = std::get<std::string>(value->value);
    result.succeeded = true;
    return result;
}

Scripting::ScriptHostOperationResult RuntimeStateStore::SetInt32(
    const Context& context, const std::string& key, const std::int32_t value)
{
    Scripting::ScriptHostOperationResult result;
    const auto policy = typedWrites_.find(context.behaviorId);
    if (!IsActive(context) || policy == typedWrites_.end() ||
        !policy->second.contains(key) || policy->second.at(key) != "Int32") return result;
    values_[{{context.entity.value, context.behaviorId}, key}] =
        {value, nextRevision_++, context.activeInstanceToken};
    result.succeeded = true;
    return result;
}

Scripting::ScriptHostOperationResult RuntimeStateStore::SetDouble(
    const Context& context, const std::string& key, const double value)
{
    Scripting::ScriptHostOperationResult result;
    const auto policy = typedWrites_.find(context.behaviorId);
    if (!IsActive(context) || !std::isfinite(value) || policy == typedWrites_.end() ||
        !policy->second.contains(key) || policy->second.at(key) != "Double") return result;
    values_[{{context.entity.value, context.behaviorId}, key}] =
        {value, nextRevision_++, context.activeInstanceToken};
    result.succeeded = true;
    return result;
}

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
    for (auto item = values_.begin(); item != values_.end();)
        if (item->first.instance.entity == identity.entity &&
            item->first.instance.behavior == identity.behavior)
            item = values_.erase(item);
        else ++item;
}

} // namespace SagaRuntimeApp
