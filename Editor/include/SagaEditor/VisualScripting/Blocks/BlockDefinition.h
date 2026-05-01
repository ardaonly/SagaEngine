/// @file BlockDefinition.h
/// @brief Palette template — what a block *is* before it is placed.

#pragma once

#include "SagaEditor/VisualScripting/Blocks/BlockKind.h"
#include "SagaEditor/VisualScripting/Blocks/BlockSlot.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEditor::VisualScripting
{

// ─── Label Fragment ───────────────────────────────────────────────────────────

/// A piece of a block's inline label. A block's `labelFragments`
/// list is rendered left-to-right; each fragment is either a
/// literal string ("move") or a slot id reference ("STEPS"). The
/// renderer pulls the slot definition from the same `BlockDefinition`
/// to draw the inline literal editor when the slot is empty.
struct BlockLabelFragment
{
    enum class Kind : std::uint8_t
    {
        Literal,   ///< Plain text drawn as-is.
        SlotRef,   ///< Reference to a slot id in the same definition.
        LineBreak, ///< Force a wrap to the next line (for two-line blocks).
    };

    Kind        kind = Kind::Literal;
    std::string text;     ///< Literal label OR the slot id, depending on kind.

    [[nodiscard]] static BlockLabelFragment MakeLiteral(std::string text);
    [[nodiscard]] static BlockLabelFragment MakeSlot   (std::string slotId);
    [[nodiscard]] static BlockLabelFragment MakeLineBreak() noexcept;

    [[nodiscard]] bool operator==(const BlockLabelFragment& o) const noexcept
    {
        return kind == o.kind && text == o.text;
    }
};

// ─── Output Type ──────────────────────────────────────────────────────────────

/// Reporter / Boolean blocks declare an output type. The IR uses
/// this to type-check slot drops; the editor uses it to colour the
/// rounded / hexagonal block outline.
enum class BlockOutputType : std::uint8_t
{
    None,    ///< Stack / Hat / Cap / C / E blocks have no output.
    Number,  ///< Reporter returning a number.
    Text,    ///< Reporter returning text.
    Boolean, ///< Boolean block.
    Any,     ///< Polymorphic — used by the variable-getter reporter.
};

[[nodiscard]] const char*       BlockOutputTypeId(BlockOutputType type) noexcept;
[[nodiscard]] BlockOutputType   BlockOutputTypeFromId(const std::string& id) noexcept;

// ─── Block Definition ─────────────────────────────────────────────────────────

/// What lives in the palette and what the editor instantiates when
/// the user drags a block onto the canvas. Definitions are
/// reference-counted by id at the IR layer, so the editor uses the
/// `opcode` field as the canonical key.
struct BlockDefinition
{
    // Identity.
    std::string opcode;        ///< Stable lowercase id (`saga.block.motion.move_steps`).
    std::string displayName;   ///< User-visible label (also used in tooltips).
    std::string categoryId;    ///< Reference to a `BlockCategory::id`.
    std::string description;   ///< One-sentence help text shown in tooltips.

    // Shape & semantics.
    BlockKind         kind       = BlockKind::Stack;
    BlockOutputType   outputType = BlockOutputType::None;

    // Inline label.
    std::vector<BlockLabelFragment>  labelFragments;

    // Slots referenced by `labelFragments`.
    std::vector<BlockSlotDefinition> slots;

    // Mouth labels for C / E blocks (e.g. {"do"} or {"if true", "else"}).
    // The size must equal `BlockKindMouthCount(kind)`; an empty vector is
    // allowed and falls back to unlabelled mouths.
    std::vector<std::string>         mouthLabels;

    // Hat-only: the event hook this hat listens for. Empty for non-hats.
    std::string                      hatEventId;

    [[nodiscard]] bool operator==(const BlockDefinition& o) const noexcept;
    [[nodiscard]] bool operator!=(const BlockDefinition& o) const noexcept
    {
        return !(*this == o);
    }
};

} // namespace SagaEditor::VisualScripting
