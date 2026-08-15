// SPDX-License-Identifier: MPL-2.0
/// @file BehaviorLanguageId.hpp
/// @brief Language/backend identity for a behavior module.

#pragma once

#include <string>

namespace SagaShared::Behavior
{

struct BehaviorLanguageId
{
    std::string value;

    [[nodiscard]] bool IsValid() const noexcept { return !value.empty(); }
    bool operator==(const BehaviorLanguageId&) const = default;
};

inline const BehaviorLanguageId CSharpBehaviorLanguage{"csharp"};

} // namespace SagaShared::Behavior
