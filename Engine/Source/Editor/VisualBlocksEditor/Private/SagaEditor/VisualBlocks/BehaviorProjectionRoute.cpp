// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/VisualBlocks/BehaviorProjectionRoute.h"

namespace SagaEditor::VisualBlocks
{

BehaviorProjectionRoute ResolveBehaviorProjection(
    const BehaviorProjectionIdentity& identity)
{
    if (!identity.behaviorModuleId.starts_with("behavior://"))
        return {false, false, false, {},
            "A canonical behavior module identity is required for Blocks projection."};
    if (identity.language != "csharp")
        return {false, false, false, {},
            "No Blocks projection backend is registered for behavior language '" +
                identity.language + "'."};
    return {true, true, true, "csharp-source-projection-v1", {}};
}

} // namespace SagaEditor::VisualBlocks
