/// @file BlockStack.h
/// @brief Vertically-snapped chain of block instances.

#pragma once

#include "SagaEditor/VisualScripting/Blocks/BlockInstance.h"

#include <vector>

namespace SagaEditor::VisualScripting
{

// ─── Block Stack ──────────────────────────────────────────────────────────────

/// An ordered list of `BlockInstance` ids the user has snapped
/// together top-to-bottom. The first id in `instances` is the head;
/// the last is the tail. Stacks are owned by `BlockScript` and
/// referenced from:
///
///   - the script's top-level `rootStacks` list (when the head is a
///     hat block, or a free-floating shelf stack the user is still
///     assembling), or
///   - a `BlockInstance::mouthStacks` slot (when the stack lives
///     inside a C / E block's mouth).
///
/// A stack may not be referenced from more than one place.
struct BlockStack
{
    BlockStackId                  id;
    std::vector<BlockInstanceId>  instances;

    /// Editor-only canvas position of the topmost block. Mouth-owned
    /// stacks ignore this — the canvas computes their position from
    /// the parent block's mouth offset at render time.
    float                         canvasX = 0.0f;
    float                         canvasY = 0.0f;

    [[nodiscard]] bool operator==(const BlockStack& o) const noexcept
    {
        return id        == o.id
            && instances == o.instances
            && canvasX   == o.canvasX
            && canvasY   == o.canvasY;
    }
    [[nodiscard]] bool operator!=(const BlockStack& o) const noexcept
    {
        return !(*this == o);
    }

    [[nodiscard]] bool          IsEmpty() const noexcept { return instances.empty(); }
    [[nodiscard]] std::size_t   Size()    const noexcept { return instances.size();  }
};

} // namespace SagaEditor::VisualScripting
