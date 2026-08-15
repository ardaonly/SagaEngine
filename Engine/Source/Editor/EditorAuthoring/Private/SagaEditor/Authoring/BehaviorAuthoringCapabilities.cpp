// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Authoring/BehaviorAuthoringCapabilities.h"

namespace SagaEditor::Authoring
{

BehaviorAuthoringCapabilities ResolveBehaviorAuthoringCapabilities(
    const BehaviorDocumentIdentity& document)
{
    if (document.behaviorModuleId.empty() || document.sourcePath.empty())
        return {false, false, false, false, false, false,
            "Behavior document module and source identities are required."};
    if (document.language != "csharp")
        return {false, false, false, false, false, false,
            "No authoring backend is registered for behavior language '" +
                document.language + "'."};
    if (document.sourceSymbolId.empty() || document.sourcePath.extension() != ".cs")
        return {false, false, false, false, false, false,
            "The C# behavior document source identity is invalid."};
    return {true, true, true, true, true, true, {}};
}

} // namespace SagaEditor::Authoring
