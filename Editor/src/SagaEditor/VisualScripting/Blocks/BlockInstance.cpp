/// @file BlockInstance.cpp
/// @brief Implementation of placed-block-instance equality.

#include "SagaEditor/VisualScripting/Blocks/BlockInstance.h"

namespace SagaEditor::VisualScripting
{

// ─── Equality ─────────────────────────────────────────────────────────────────

bool BlockInstance::operator==(const BlockInstance& o) const noexcept
{
    if (id != o.id || opcode != o.opcode || comment != o.comment ||
        canvasX != o.canvasX || canvasY != o.canvasY)
    {
        return false;
    }

    if (slotFills.size() != o.slotFills.size())
    {
        return false;
    }
    for (const auto& [k, v] : slotFills)
    {
        const auto it = o.slotFills.find(k);
        if (it == o.slotFills.end() || !(it->second == v))
        {
            return false;
        }
    }

    if (mouthStacks.size() != o.mouthStacks.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < mouthStacks.size(); ++i)
    {
        if (mouthStacks[i] != o.mouthStacks[i])
        {
            return false;
        }
    }
    return true;
}

} // namespace SagaEditor::VisualScripting
