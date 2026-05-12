/// @file EnumDefinition.cpp
/// @brief EnumDefinition member lookup implementations.

#include "SDE/Model/EnumDefinition.h"

#include <algorithm>

namespace SDE
{

bool EnumDefinition::ContainsMember(const std::string& name) const noexcept
{
    return std::any_of(members.begin(), members.end(),
                       [&name](const EnumMember& m) { return m.name == name; });
}

std::optional<EnumMember> EnumDefinition::FindMember(const std::string& name) const noexcept
{
    auto it = std::find_if(members.begin(), members.end(),
                           [&name](const EnumMember& m) { return m.name == name; });
    if (it == members.end())
        return std::nullopt;
    return *it;
}

} // namespace SDE
