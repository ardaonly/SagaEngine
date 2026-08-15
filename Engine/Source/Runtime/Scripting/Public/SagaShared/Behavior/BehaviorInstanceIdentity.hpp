// SPDX-License-Identifier: MPL-2.0
/// @file BehaviorInstanceIdentity.hpp
/// @brief Launch/world-scoped identity of one behavior instance.

#pragma once

#include "SagaShared/Behavior/BehaviorModuleId.hpp"

#include <string>

namespace SagaShared::Behavior
{

struct BehaviorInstanceIdentity
{
    std::string value;
    BehaviorModuleId behaviorModuleId;
    std::string attachmentId;
    std::string worldGenerationId;
};

} // namespace SagaShared::Behavior
