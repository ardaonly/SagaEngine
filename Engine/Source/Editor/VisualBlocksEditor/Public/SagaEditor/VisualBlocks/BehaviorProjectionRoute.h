// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <string>

namespace SagaEditor::VisualBlocks
{

struct BehaviorProjectionIdentity
{
    std::string behaviorModuleId;
    std::string language;
};

struct BehaviorProjectionRoute
{
    bool available = false;
    bool sourceProjection = false;
    bool controlledEditing = false;
    std::string backendId;
    std::string unavailableReason;
};

/// Resolves Blocks as an authoring projection capability, never as runtime code.
[[nodiscard]] BehaviorProjectionRoute ResolveBehaviorProjection(
    const BehaviorProjectionIdentity& identity);

} // namespace SagaEditor::VisualBlocks
