/// @file CompiledModelGraph.cpp
/// @brief CompiledModelGraph and ValueSlab implementations.

#include "SDE/Compilation/CompiledModelGraph.h"

#include <algorithm>
#include <cassert>

namespace SDE
{

// ─── CompiledValue ────────────────────────────────────────────────────────────

bool CompiledValue::IsNull()   const noexcept { return std::holds_alternative<CompiledNull>(data); }
bool CompiledValue::IsUsable() const noexcept { return !IsNull(); }

// ─── ValueSlab ────────────────────────────────────────────────────────────────

uint32_t ValueSlab::Count() const noexcept
{
    return static_cast<uint32_t>(mSlots.size());
}

const CompiledValue& ValueSlab::At(uint32_t index) const
{
    assert(index < mSlots.size() && "ValueSlab index out of range");
    return mSlots[index];
}

uint32_t ValueSlab::Append(CompiledValue value)
{
    uint32_t idx = static_cast<uint32_t>(mSlots.size());
    mSlots.push_back(std::move(value));
    return idx;
}

SlabRange ValueSlab::Reserve(uint32_t count)
{
    SlabRange range;
    range.offset = static_cast<uint32_t>(mSlots.size());
    range.count  = count;
    mSlots.resize(mSlots.size() + count);
    return range;
}

// ─── CompiledInstance ─────────────────────────────────────────────────────────

const CompiledValue* CompiledInstance::GetField(const std::string& fieldId) const noexcept
{
    auto it = fields.find(fieldId);
    return (it != fields.end()) ? &it->second : nullptr;
}

// ─── CompiledModelGraph ───────────────────────────────────────────────────────

const CompiledInstance* CompiledModelGraph::Find(const std::string& modelId,
                                                   const std::string& instanceId) const noexcept
{
    auto it = mInstances.find({modelId, instanceId});
    return (it != mInstances.end()) ? &it->second : nullptr;
}

std::vector<const CompiledInstance*> CompiledModelGraph::AllOf(const std::string& modelId) const
{
    std::vector<const CompiledInstance*> result;
    for (const auto& [key, inst] : mInstances)
    {
        if (key.first == modelId)
            result.push_back(&inst);
    }
    return result;
}

std::vector<std::string> CompiledModelGraph::ModelIds() const
{
    std::vector<std::string> ids;
    for (const auto& [key, _] : mInstances)
    {
        if (std::find(ids.begin(), ids.end(), key.first) == ids.end())
            ids.push_back(key.first);
    }
    return ids;
}

std::size_t CompiledModelGraph::TotalCount() const noexcept
{
    return mInstances.size();
}

const ValueSlab& CompiledModelGraph::Slab() const noexcept
{
    return mSlab;
}

void CompiledModelGraph::AddInstance(CompiledInstance instance)
{
    InstanceKey key{instance.modelId, instance.instanceId};
    mInstances[std::move(key)] = std::move(instance);
}

} // namespace SDE
