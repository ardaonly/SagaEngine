/// @file BlockCategory.h
/// @brief Block library categories — colour, label, and ordering.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEditor::VisualScripting
{

// ─── Brand Colour ─────────────────────────────────────────────────────────────

/// Per-category brand colour stored as packed RGB. Alpha is
/// implicit-opaque; the persona's `BlockVisualStyle::saturationScale`
/// is what scales the colour at render time.
struct BlockBrandColor
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    [[nodiscard]] bool operator==(const BlockBrandColor& o) const noexcept
    {
        return r == o.r && g == o.g && b == o.b;
    }
    [[nodiscard]] bool operator!=(const BlockBrandColor& o) const noexcept
    {
        return !(*this == o);
    }
};

// ─── Block Category ───────────────────────────────────────────────────────────

/// One section of the block palette. The eight built-in categories
/// match Scratch's well-known palette (Motion, Looks, Sound, Events,
/// Control, Sensing, Operators, Variables) plus a ninth user-authored
/// section, "My Blocks". Extensions register additional categories
/// through `BlockLibrary::RegisterCategory`.
struct BlockCategory
{
    std::string     id;          ///< Stable lowercase id (`saga.cat.motion`).
    std::string     displayName; ///< User-visible label (localised).
    BlockBrandColor brandColor;  ///< Single brand colour for every block in this category.
    std::int32_t    sortOrder = 0; ///< Smaller values appear first in the palette.

    [[nodiscard]] bool operator==(const BlockCategory& o) const noexcept;
    [[nodiscard]] bool operator!=(const BlockCategory& o) const noexcept
    {
        return !(*this == o);
    }
};

// ─── Built-in Categories ──────────────────────────────────────────────────────

[[nodiscard]] BlockCategory MakeMotionCategory();      ///< Movement blocks.
[[nodiscard]] BlockCategory MakeLooksCategory();       ///< Appearance blocks.
[[nodiscard]] BlockCategory MakeSoundCategory();       ///< Audio blocks.
[[nodiscard]] BlockCategory MakeEventsCategory();      ///< Hat blocks live here.
[[nodiscard]] BlockCategory MakeControlCategory();     ///< If / repeat / wait.
[[nodiscard]] BlockCategory MakeSensingCategory();     ///< Touching / key / mouse.
[[nodiscard]] BlockCategory MakeOperatorsCategory();   ///< +, -, =, and / or / not.
[[nodiscard]] BlockCategory MakeVariablesCategory();   ///< Variables and lists.
[[nodiscard]] BlockCategory MakeMyBlocksCategory();    ///< User-defined procedures.

/// All nine built-in categories in their canonical palette order.
[[nodiscard]] std::vector<BlockCategory> MakeBuiltinCategories();

} // namespace SagaEditor::VisualScripting
