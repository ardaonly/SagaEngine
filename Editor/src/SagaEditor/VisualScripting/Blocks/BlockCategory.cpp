/// @file BlockCategory.cpp
/// @brief Implementation of the block library categories.

#include "SagaEditor/VisualScripting/Blocks/BlockCategory.h"

namespace SagaEditor::VisualScripting
{

// ─── Equality ─────────────────────────────────────────────────────────────────

bool BlockCategory::operator==(const BlockCategory& o) const noexcept
{
    return id          == o.id
        && displayName == o.displayName
        && brandColor  == o.brandColor
        && sortOrder   == o.sortOrder;
}

// ─── Built-in Categories ──────────────────────────────────────────────────────
//
// Brand colours are picked to be perceptually distinct and adjacent
// to the colours users coming from Scratch will recognise. They are
// the *base* colours; the persona's `BlockVisualStyle::saturationScale`
// is what tints them per persona at render time.

BlockCategory MakeMotionCategory()
{
    return { "saga.cat.motion",   "Motion",   { 76, 151, 255 },  0 }; // blue
}

BlockCategory MakeLooksCategory()
{
    return { "saga.cat.looks",    "Looks",    {153,  102, 255 }, 1 }; // purple
}

BlockCategory MakeSoundCategory()
{
    return { "saga.cat.sound",    "Sound",    {207,  99, 207 },  2 }; // magenta
}

BlockCategory MakeEventsCategory()
{
    return { "saga.cat.events",   "Events",   {255, 191,   0 }, 3 };  // amber (hats)
}

BlockCategory MakeControlCategory()
{
    return { "saga.cat.control",  "Control",  {255, 171,  25 }, 4 };  // orange
}

BlockCategory MakeSensingCategory()
{
    return { "saga.cat.sensing",  "Sensing",  { 92, 177, 214 }, 5 };  // teal
}

BlockCategory MakeOperatorsCategory()
{
    return { "saga.cat.operators","Operators",{ 89, 192,  89 }, 6 };  // green
}

BlockCategory MakeVariablesCategory()
{
    return { "saga.cat.variables","Variables",{255, 140,  26 }, 7 };  // tangerine
}

BlockCategory MakeMyBlocksCategory()
{
    return { "saga.cat.myblocks", "My Blocks",{255,  98, 152 }, 8 };  // pink
}

std::vector<BlockCategory> MakeBuiltinCategories()
{
    return {
        MakeMotionCategory(),
        MakeLooksCategory(),
        MakeSoundCategory(),
        MakeEventsCategory(),
        MakeControlCategory(),
        MakeSensingCategory(),
        MakeOperatorsCategory(),
        MakeVariablesCategory(),
        MakeMyBlocksCategory(),
    };
}

} // namespace SagaEditor::VisualScripting
