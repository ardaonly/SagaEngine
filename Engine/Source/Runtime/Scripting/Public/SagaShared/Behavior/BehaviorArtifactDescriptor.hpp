// SPDX-License-Identifier: MPL-2.0
/// @file BehaviorArtifactDescriptor.hpp
/// @brief Immutable generic behavior artifact identity and backend payload references.

#pragma once

#include "SagaShared/Behavior/BehaviorLanguageId.hpp"

#include <map>
#include <string>
#include <vector>

namespace SagaShared::Behavior
{

struct BehaviorArtifactDescriptor
{
    std::string artifactId;
    BehaviorLanguageId language;
    std::string format;
    std::string artifactHash;
    std::string packageGenerationId;
    std::map<std::string, std::string> payloadArtifacts;
    std::vector<std::string> behaviorModuleIds;
};

} // namespace SagaShared::Behavior
