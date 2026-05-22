/// @file ComposerSourceIndex.h
/// @brief Conservative saga.editor source index used by SagaEditorComposer.

#pragma once

#include "SagaEditor/Composer/ComposerWorkspace.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

/// Supported source item categories surfaced by the Composer MVP.
enum class ComposerSourceKind
{
    Composition,
    Panel,
    Action,
    Menu,
    MenuItem,
    Toolbar,
    Shortcut,
    WorkspaceLayout,
    Workspace,
    Mode,
    Unknown
};

/// One scalar field assignment inside an indexed .sde instance block.
struct ComposerSourceField
{
    std::string name;             ///< Field name as written in source.
    std::string value;            ///< Parsed value without surrounding quotes for strings.
    std::size_t line = 0;         ///< One-based source line.
    std::size_t valueStart = 0;   ///< Zero-based inclusive value offset in file text.
    std::size_t valueEnd = 0;     ///< Zero-based exclusive value offset in file text.
    bool quoted = false;          ///< True when the field value is a quoted string literal.
};

/// Indexed .sde instance block with enough location data for safe field edits.
struct ComposerSourceItem
{
    ComposerSourceKind kind = ComposerSourceKind::Unknown; ///< Recognized saga.editor model kind.
    std::string model;                                    ///< Source model id, for example EditorPanel.
    std::string id;                                       ///< Instance id.
    std::filesystem::path file;                           ///< Source file containing the instance.
    std::size_t line = 0;                                 ///< One-based line of the instance declaration.
    std::size_t blockStart = 0;                           ///< Zero-based block start offset.
    std::size_t blockEnd = 0;                             ///< Zero-based block end offset.
    std::vector<ComposerSourceField> fields;              ///< Scalar field assignments indexed in the block.
};

/// Source indexing diagnostic.
struct ComposerSourceDiagnostic
{
    std::string code;              ///< Stable diagnostic code.
    std::string message;           ///< Human-readable diagnostic message.
    std::filesystem::path file;    ///< Related source file when available.
    std::size_t line = 0;          ///< One-based line when available.
};

/// Full indexed source workspace state.
struct ComposerSourceIndex
{
    std::vector<ComposerSourceItem> items;             ///< Recognized source instances.
    std::vector<ComposerSourceDiagnostic> diagnostics; ///< Indexing diagnostics.
};

/// Convert a source kind to the UI group label.
[[nodiscard]] std::string ComposerSourceKindLabel(ComposerSourceKind kind);

/// Parse every .sde file under the workspace source root.
[[nodiscard]] ComposerSourceIndex BuildComposerSourceIndex(
    const ComposerWorkspacePaths& paths);

/// Find an item by id in an index.
[[nodiscard]] const ComposerSourceItem* FindComposerSourceItem(
    const ComposerSourceIndex& index,
    const std::string& id);

/// Find a field by name in an indexed item.
[[nodiscard]] const ComposerSourceField* FindComposerSourceField(
    const ComposerSourceItem& item,
    const std::string& fieldName);

} // namespace SagaEditor
