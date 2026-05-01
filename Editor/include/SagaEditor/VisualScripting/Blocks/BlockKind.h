/// @file BlockKind.h
/// @brief Shape vocabulary for Scratch-style block authoring.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEditor::VisualScripting
{

// ─── Block Kind ───────────────────────────────────────────────────────────────

/// The seven block shapes the editor supports. Shape — and only
/// shape — decides what can connect to what at edit time. The
/// vocabulary mirrors Scratch so users transferring from Scratch can
/// rely on the visual cues they already know.
enum class BlockKind : std::uint8_t
{
    Hat,      ///< Top-only — starts a stack (e.g. "when flag clicked").
    Stack,    ///< Top + bottom — stacks vertically (e.g. "move N steps").
    CBlock,   ///< Wraps a single child stack in a mouth (e.g. "repeat").
    EBlock,   ///< Wraps two child stacks in two mouths (e.g. "if … else").
    Cap,      ///< Top-only — ends a stack (e.g. "stop all").
    Reporter, ///< Returns a value — slots into reporter slots (rounded).
    Boolean,  ///< Returns a boolean — slots into boolean slots (hexagonal).
};

/// Stable lowercase id for `kind`. Used by the JSON serializer and
/// the IR lifter; do not rename. Returns
/// `"saga.blockkind.unknown"` for invalid enumerators.
[[nodiscard]] const char* BlockKindId(BlockKind kind) noexcept;

/// Inverse of `BlockKindId`. Returns `BlockKind::Stack` when the id
/// is unrecognised — the editor never refuses to load a project
/// because of a stale opcode definition.
[[nodiscard]] BlockKind BlockKindFromId(const std::string& id) noexcept;

// ─── Connection Predicates ────────────────────────────────────────────────────

/// True when `kind` may sit at the top of a stack. Hat and Cap
/// blocks both qualify (a Cap-only stack is unusual but legal).
[[nodiscard]] constexpr bool BlockKindCanBeTop(BlockKind kind) noexcept
{
    return kind == BlockKind::Hat   || kind == BlockKind::Stack ||
           kind == BlockKind::CBlock|| kind == BlockKind::EBlock||
           kind == BlockKind::Cap;
}

/// True when `kind` may have something connect *below* it.
/// Hat / Stack / C / E blocks all have a bottom notch; Cap, Reporter,
/// and Boolean blocks do not.
[[nodiscard]] constexpr bool BlockKindHasBottomNotch(BlockKind kind) noexcept
{
    return kind == BlockKind::Hat   || kind == BlockKind::Stack ||
           kind == BlockKind::CBlock|| kind == BlockKind::EBlock;
}

/// True when `kind` may have something connect *above* it.
/// Stack / C / E / Cap blocks have a top notch; Hat, Reporter, and
/// Boolean blocks do not.
[[nodiscard]] constexpr bool BlockKindHasTopNotch(BlockKind kind) noexcept
{
    return kind == BlockKind::Stack || kind == BlockKind::CBlock ||
           kind == BlockKind::EBlock|| kind == BlockKind::Cap;
}

/// True when `kind` produces a value (Reporter or Boolean). The
/// editor uses this to decide whether the block can drop into a
/// value-accepting slot.
[[nodiscard]] constexpr bool BlockKindIsExpression(BlockKind kind) noexcept
{
    return kind == BlockKind::Reporter || kind == BlockKind::Boolean;
}

/// Number of mouths a `kind` exposes for child stacks. C-blocks
/// have one, E-blocks have two, every other kind has zero.
[[nodiscard]] constexpr std::uint8_t BlockKindMouthCount(BlockKind kind) noexcept
{
    if (kind == BlockKind::CBlock) return 1;
    if (kind == BlockKind::EBlock) return 2;
    return 0;
}

} // namespace SagaEditor::VisualScripting
