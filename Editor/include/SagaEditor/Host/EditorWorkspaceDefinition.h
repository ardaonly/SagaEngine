/// @file EditorWorkspaceDefinition.h
/// @brief Prepared workspace input consumed by the editor host.

#pragma once

#include <filesystem>
#include <string>

namespace SagaEditor
{

/// Already-resolved workspace input prepared by the product layer.
struct EditorWorkspaceDefinition
{
    std::string           id;               ///< Product-stable workspace id.
    std::filesystem::path root;             ///< Resolved editor workspace root.
    std::string           initialProfileId; ///< Optional editor profile id.
    std::string           layoutPreset;     ///< Optional editor layout preset.
    bool                  sdeValidated = false; ///< True when product SDE validation passed.
};

} // namespace SagaEditor
