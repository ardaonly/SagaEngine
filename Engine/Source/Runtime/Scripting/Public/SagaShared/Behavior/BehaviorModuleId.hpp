// SPDX-License-Identifier: MPL-2.0
/// @file BehaviorModuleId.hpp
/// @brief Stable logical behavior-module identity.

#pragma once

#include <string>
#include <string_view>

namespace SagaShared::Behavior
{

struct BehaviorModuleId
{
    std::string value;

    [[nodiscard]] bool IsValid() const noexcept
    {
        constexpr std::string_view prefix = "behavior://";
        return value.starts_with(prefix) && value.size() > prefix.size();
    }
    bool operator==(const BehaviorModuleId&) const = default;
};

} // namespace SagaShared::Behavior
