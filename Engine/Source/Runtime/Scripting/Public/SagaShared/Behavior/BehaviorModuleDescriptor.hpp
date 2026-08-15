// SPDX-License-Identifier: MPL-2.0
/// @file BehaviorModuleDescriptor.hpp
/// @brief Language-neutral behavior module catalog entry.

#pragma once

#include "SagaShared/Behavior/BehaviorModuleId.hpp"
#include "SagaShared/Behavior/BehaviorSourceIdentity.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaShared::Behavior
{

struct BehaviorModuleDescriptor
{
    BehaviorModuleId moduleId;
    BehaviorSourceIdentity source;
    std::string projectId;
    std::string packageId;
    std::string packageGenerationId;
    std::uint32_t apiLevel = 0;
    std::vector<std::string> sourceInventory;
    std::vector<std::string> requestedCapabilities;
};

} // namespace SagaShared::Behavior
