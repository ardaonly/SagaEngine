/// @file BlockSlot.h
/// @brief Slot definition + literal-fill values for Scratch-style blocks.

#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace SagaEditor::VisualScripting
{

// ─── Slot Kind ────────────────────────────────────────────────────────────────

/// What a slot accepts. Slot kinds drive the literal editor the
/// canvas renders inline next to the block label and decide whether
/// the slot is also droppable as a target for reporter / boolean
/// expressions.
enum class BlockSlotKind : std::uint8_t
{
    Number,    ///< Numeric literal, accepts a Reporter.
    Text,      ///< Text literal, accepts a Reporter.
    Boolean,   ///< Hexagonal slot, accepts only a Boolean block.
    Dropdown,  ///< Closed list of named choices, no expression drop.
    Colour,    ///< RGB picker, no expression drop.
    Variable,  ///< Reference to a declared variable, accepts a Reporter.
    Procedure, ///< Reference to a custom-block (My Blocks) name.
    Statement, ///< Mouth slot (used by C-blocks / E-blocks); see BlockInstance.
};

[[nodiscard]] const char*    BlockSlotKindId(BlockSlotKind kind) noexcept;
[[nodiscard]] BlockSlotKind  BlockSlotKindFromId(const std::string& id) noexcept;

/// True when the slot may carry a reporter / boolean expression in
/// addition to (or instead of) a literal value. Dropdowns, colours,
/// and statement mouths cannot — their fills are structural.
[[nodiscard]] constexpr bool BlockSlotAcceptsExpression(BlockSlotKind kind) noexcept
{
    return kind == BlockSlotKind::Number ||
           kind == BlockSlotKind::Text   ||
           kind == BlockSlotKind::Boolean||
           kind == BlockSlotKind::Variable;
}

// ─── Dropdown Option ──────────────────────────────────────────────────────────

/// One entry in a dropdown slot's option list. Stable id + localised
/// label — the id is what the IR sees, the label is what the user
/// sees.
struct BlockDropdownOption
{
    std::string id;
    std::string label;

    [[nodiscard]] bool operator==(const BlockDropdownOption& o) const noexcept
    {
        return id == o.id && label == o.label;
    }
};

// ─── Slot Definition ──────────────────────────────────────────────────────────

/// Static description of a single slot on a block definition. A
/// `BlockDefinition` carries one of these per slot in its label
/// fragments; an instance carries a per-slot fill that conforms to
/// the definition's kind.
struct BlockSlotDefinition
{
    std::string                       id;          ///< Stable id within the block.
    std::string                       label;       ///< Optional caption next to the slot.
    BlockSlotKind                     kind = BlockSlotKind::Number;

    // Literal defaults.
    double                            defaultNumber  = 0.0;
    std::string                       defaultText;
    bool                              defaultBool    = false;
    std::uint32_t                     defaultColor   = 0xFF0000FFu; ///< RGBA.

    // Dropdown options (only meaningful when `kind == Dropdown`).
    std::vector<BlockDropdownOption>  options;

    [[nodiscard]] bool operator==(const BlockSlotDefinition& o) const noexcept;
    [[nodiscard]] bool operator!=(const BlockSlotDefinition& o) const noexcept
    {
        return !(*this == o);
    }
};

// ─── Slot Fill ────────────────────────────────────────────────────────────────

/// What a `BlockInstance` puts in one of its slots. A slot fill is
/// either a literal value, a reference to another block by id (when
/// the user dropped a Reporter / Boolean into the slot), or "empty"
/// (the editor renders the default literal for the kind).
struct BlockInstanceId; // forward-declared in BlockInstance.h

/// Sentinel id meaning "this slot is empty; fall back to the
/// definition's default literal". Stored as a 64-bit zero so the
/// literal-vs-reference branch is a cheap integer compare.
inline constexpr std::uint64_t kBlockInstanceIdNone = 0;

/// Tagged-union fill carried in a `BlockInstance::slotFills` slot.
/// `value` is one of: empty, literal-number, literal-text,
/// literal-boolean, literal-colour, dropdown-id, variable-id,
/// procedure-id, or a nested instance id (for reporter expressions).
struct BlockSlotFill
{
    /// Tag describing which alternative is active.
    enum class Kind : std::uint8_t
    {
        Empty,
        Number,
        Text,
        Boolean,
        Color,
        Dropdown,
        VariableRef,
        ProcedureRef,
        Expression, ///< nested reporter / boolean instance
    };

    Kind          kind        = Kind::Empty;
    double        numberValue = 0.0;
    std::string   textValue;
    bool          boolValue   = false;
    std::uint32_t colorValue  = 0xFFFFFFFFu;     ///< RGBA when kind == Color
    std::string   refId;                          ///< dropdown / variable / procedure id
    std::uint64_t expressionId = kBlockInstanceIdNone;

    [[nodiscard]] bool operator==(const BlockSlotFill& o) const noexcept;
    [[nodiscard]] bool operator!=(const BlockSlotFill& o) const noexcept
    {
        return !(*this == o);
    }

    // ─── Convenience Builders ─────────────────────────────────────────────────

    [[nodiscard]] static BlockSlotFill MakeEmpty() noexcept;
    [[nodiscard]] static BlockSlotFill MakeNumber(double v) noexcept;
    [[nodiscard]] static BlockSlotFill MakeText(std::string v);
    [[nodiscard]] static BlockSlotFill MakeBool(bool v) noexcept;
    [[nodiscard]] static BlockSlotFill MakeColor(std::uint32_t rgba) noexcept;
    [[nodiscard]] static BlockSlotFill MakeDropdown(std::string optionId);
    [[nodiscard]] static BlockSlotFill MakeVariable(std::string variableId);
    [[nodiscard]] static BlockSlotFill MakeProcedure(std::string procedureId);
    [[nodiscard]] static BlockSlotFill MakeExpression(std::uint64_t instanceId) noexcept;
};

} // namespace SagaEditor::VisualScripting
