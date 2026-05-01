/// @file BlockLibrary.h
/// @brief Registry of `BlockDefinition`s — the editor's block palette source.

#pragma once

#include "SagaEditor/VisualScripting/Blocks/BlockCategory.h"
#include "SagaEditor/VisualScripting/Blocks/BlockDefinition.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEditor::VisualScripting
{

// ─── Block Library ────────────────────────────────────────────────────────────

/// Owns every `BlockDefinition` and `BlockCategory` the editor knows
/// about. The palette panel walks the library and groups definitions
/// by category id; the canvas resolves opcodes to definitions when
/// rendering an instance. Extensions register their own categories
/// and definitions through the same surface.
class BlockLibrary
{
public:
    BlockLibrary()  = default;
    ~BlockLibrary() = default;

    BlockLibrary(const BlockLibrary&)            = delete;
    BlockLibrary& operator=(const BlockLibrary&) = delete;
    BlockLibrary(BlockLibrary&&)                 = default;
    BlockLibrary& operator=(BlockLibrary&&)      = default;

    // ─── Population ───────────────────────────────────────────────────────────

    /// Register the nine built-in categories. Idempotent.
    void RegisterBuiltinCategories();

    /// Register a starter set of Scratch-equivalent block definitions
    /// (move, turn, when-flag-clicked, repeat, if, say, plus number /
    /// boolean / variable reporters). Idempotent.
    void RegisterBuiltinDefinitions();

    /// Convenience: call both built-in registrations.
    void RegisterAllBuiltins();

    /// Add or replace a category. Returns false when the category id
    /// is empty.
    bool RegisterCategory(BlockCategory category);

    /// Add or replace a block definition. Returns false when the
    /// opcode is empty or the referenced category is unknown.
    bool RegisterDefinition(BlockDefinition definition);

    /// Drop everything. Used by tests between cases.
    void Clear() noexcept;

    // ─── Lookup ───────────────────────────────────────────────────────────────

    [[nodiscard]] bool                       HasCategory  (const std::string& id) const noexcept;
    [[nodiscard]] bool                       HasDefinition(const std::string& opcode) const noexcept;
    [[nodiscard]] const BlockCategory*       FindCategory (const std::string& id) const noexcept;
    [[nodiscard]] const BlockDefinition*     FindDefinition(const std::string& opcode) const noexcept;

    /// Snapshot of every category, sorted by `sortOrder` ascending
    /// then by `id` ascending.
    [[nodiscard]] std::vector<BlockCategory> GetAllCategories() const;

    /// Snapshot of every definition belonging to `categoryId`. The
    /// returned list is sorted by `displayName` ascending so the
    /// palette ordering is stable across runs.
    [[nodiscard]] std::vector<BlockDefinition>
        GetDefinitionsForCategory(const std::string& categoryId) const;

    /// Snapshot of every registered definition.
    [[nodiscard]] std::vector<BlockDefinition> GetAllDefinitions() const;

    [[nodiscard]] std::size_t CategoryCount()    const noexcept;
    [[nodiscard]] std::size_t DefinitionCount()  const noexcept;

private:
    std::unordered_map<std::string, BlockCategory>   m_categories;
    std::unordered_map<std::string, BlockDefinition> m_definitions;
};

} // namespace SagaEditor::VisualScripting
