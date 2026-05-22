/// @file ComposerEditCommand.h
/// @brief Pending source edit and checkpoint writer for SagaEditorComposer.

#pragma once

#include "SagaEditor/Composer/ComposerSourceIndex.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor
{

/// Supported field value kinds for controlled Composer source edits.
enum class ComposerEditValueKind
{
    Text,
    Boolean
};

/// One pending controlled edit to a source field.
struct ComposerPendingEdit
{
    std::filesystem::path file; ///< Source file to update.
    std::string model;         ///< Source model id.
    std::string itemId;        ///< Source instance id.
    std::string fieldName;     ///< Field name being edited.
    std::string oldValue;      ///< Current parsed value.
    std::string newValue;      ///< Replacement parsed value.
    std::size_t valueStart = 0;///< Zero-based inclusive value offset.
    std::size_t valueEnd = 0;  ///< Zero-based exclusive value offset.
    bool quoted = false;       ///< True when replacement must be quoted.
};

/// Result returned from edit planning and source persistence operations.
struct ComposerEditResult
{
    bool ok = false;                                      ///< True when the operation succeeded.
    std::string code;                                    ///< Stable result or diagnostic code.
    std::string message;                                 ///< Human-readable result.
    std::vector<ComposerPendingEdit> pendingEdits;       ///< Pending edit list after the operation.
};

/// Maintains pending controlled edits against an indexed source workspace.
class ComposerEditSession
{
public:
    /// Replace the source index used for validation and clear pending edits.
    void Reset(ComposerSourceIndex index);

    /// Queue or replace a supported field edit.
    [[nodiscard]] ComposerEditResult SetField(std::string itemId,
                                              std::string fieldName,
                                              std::string newValue);

    /// Clear pending edits without touching source files.
    void Revert();

    /// Save all pending edits after creating source checkpoints.
    [[nodiscard]] ComposerEditResult Save(
        const ComposerWorkspacePaths& paths);

    /// Return all pending edits.
    [[nodiscard]] const std::vector<ComposerPendingEdit>& PendingEdits() const noexcept;

private:
    [[nodiscard]] ComposerEditResult ValidateEdit(const ComposerSourceItem& item,
                                                  const ComposerSourceField& field,
                                                  const std::string& newValue) const;

    ComposerSourceIndex m_index;
    std::vector<ComposerPendingEdit> m_pendingEdits;
};

} // namespace SagaEditor
