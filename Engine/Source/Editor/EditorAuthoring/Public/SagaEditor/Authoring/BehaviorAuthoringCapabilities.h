// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <filesystem>
#include <string>

namespace SagaEditor::Authoring
{

struct BehaviorDocumentIdentity
{
    std::string behaviorModuleId;
    std::string language;
    std::string sourceSymbolId;
    std::filesystem::path sourcePath;
};

struct BehaviorAuthoringCapabilities
{
    bool available = false;
    bool sourceEditing = false;
    bool diagnostics = false;
    bool navigation = false;
    bool blocksProjection = false;
    bool controlledBlockEditing = false;
    std::string unavailableReason;
};

/// Fail-closed capability routing for Code Box and behavior views.
[[nodiscard]] BehaviorAuthoringCapabilities ResolveBehaviorAuthoringCapabilities(
    const BehaviorDocumentIdentity& document);

} // namespace SagaEditor::Authoring
