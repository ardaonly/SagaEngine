// SPDX-License-Identifier: MPL-2.0
/// @file BehaviorAttachmentDescriptor.hpp
/// @brief Authored scene binding for one stable behavior module.

#pragma once

#include "SagaShared/Behavior/BehaviorModuleId.hpp"

#include <string>
#include <vector>

namespace SagaShared::Behavior
{

struct BehaviorAttachmentDescriptor
{
    std::string attachmentId;
    BehaviorModuleId behaviorModuleId;
    std::string instanceConfigurationCanonicalJson = "{}";
    std::vector<std::string> requiredCapabilities;
    bool enabled = true;
};

} // namespace SagaShared::Behavior
