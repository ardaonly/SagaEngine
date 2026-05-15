/// @file EnumRegistry.cpp
/// @brief EnumRegistry implementation.

#include "SDE/Model/EnumRegistry.h"

#include <cassert>

namespace SDE
{

void EnumRegistry::Register(EnumDefinition definition)
{
    assert(!mFrozen && "EnumRegistry is frozen");
    if (mFrozen || definition.id.empty())
        return;
    mDefinitions[definition.id] = std::move(definition);
}

void EnumRegistry::Freeze()
{
    mFrozen = true;
}

bool EnumRegistry::IsFrozen() const noexcept
{
    return mFrozen;
}

const EnumDefinition* EnumRegistry::Find(const std::string& enumId) const noexcept
{
    auto it = mDefinitions.find(enumId);
    return (it != mDefinitions.end()) ? &it->second : nullptr;
}

std::vector<std::string> EnumRegistry::EnumIds() const
{
    std::vector<std::string> ids;
    ids.reserve(mDefinitions.size());
    for (const auto& [id, _] : mDefinitions)
        ids.push_back(id);
    return ids;
}

} // namespace SDE
