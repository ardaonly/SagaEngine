/// @file EnumRegistry.h
/// @brief Frozen registry of enum definitions used by validation.

#pragma once

#include "SDE/Model/EnumDefinition.h"

#include <map>
#include <string>
#include <vector>

namespace SDE
{

class EnumRegistry
{
public:
    void Register(EnumDefinition definition);
    void Freeze();

    [[nodiscard]] bool IsFrozen() const noexcept;
    [[nodiscard]] const EnumDefinition* Find(const std::string& enumId) const noexcept;
    [[nodiscard]] std::vector<std::string> EnumIds() const;

private:
    std::map<std::string, EnumDefinition> mDefinitions;
    bool                                  mFrozen = false;
};

} // namespace SDE
