/// @file BlockSnapRules.cpp
/// @brief Implementation of the block-canvas snap-rule predicate.

#include "SagaEditor/VisualScripting/Blocks/BlockSnapRules.h"

#include "SagaEditor/VisualScripting/Blocks/BlockLibrary.h"
#include "SagaEditor/VisualScripting/Blocks/BlockScript.h"

#include <algorithm>
#include <utility>

namespace SagaEditor::VisualScripting
{

// ─── Identity Strings ─────────────────────────────────────────────────────────

const char* DropTargetKindId(DropTargetKind kind) noexcept
{
    switch (kind)
    {
        case DropTargetKind::EmptyCanvas:    return "saga.drop.empty";
        case DropTargetKind::StackTop:       return "saga.drop.stack.top";
        case DropTargetKind::StackBottom:    return "saga.drop.stack.bottom";
        case DropTargetKind::StackBetween:   return "saga.drop.stack.between";
        case DropTargetKind::ExpressionSlot: return "saga.drop.slot";
        case DropTargetKind::Mouth:          return "saga.drop.mouth";
    }
    return "saga.drop.unknown";
}

DropTargetKind DropTargetKindFromId(const std::string& id) noexcept
{
    if (id == "saga.drop.empty")         return DropTargetKind::EmptyCanvas;
    if (id == "saga.drop.stack.top")     return DropTargetKind::StackTop;
    if (id == "saga.drop.stack.bottom")  return DropTargetKind::StackBottom;
    if (id == "saga.drop.stack.between") return DropTargetKind::StackBetween;
    if (id == "saga.drop.slot")          return DropTargetKind::ExpressionSlot;
    if (id == "saga.drop.mouth")         return DropTargetKind::Mouth;
    return DropTargetKind::EmptyCanvas;
}

const char* SnapRejectionId(SnapRejection r) noexcept
{
    switch (r)
    {
        case SnapRejection::Accepted:               return "saga.snap.accepted";
        case SnapRejection::UnknownOpcode:          return "saga.snap.unknown_opcode";
        case SnapRejection::HatMustStartStack:      return "saga.snap.hat_top_only";
        case SnapRejection::HatNotAllowedHere:      return "saga.snap.hat_not_here";
        case SnapRejection::CapMustEndStack:        return "saga.snap.cap_end_only";
        case SnapRejection::CapCannotPrecedeStack:  return "saga.snap.cap_blocks_below";
        case SnapRejection::ExpressionInStack:      return "saga.snap.expr_in_stack";
        case SnapRejection::StackBlockInExpression: return "saga.snap.stack_in_expr";
        case SnapRejection::SlotTypeMismatch:       return "saga.snap.slot_type";
        case SnapRejection::SlotDoesNotAccept:      return "saga.snap.slot_no_expr";
        case SnapRejection::MouthIndexOutOfRange:   return "saga.snap.mouth_oor";
        case SnapRejection::HostHasNoSuchSlot:      return "saga.snap.no_slot";
        case SnapRejection::InvalidStack:           return "saga.snap.bad_stack";
        case SnapRejection::InvalidHost:            return "saga.snap.bad_host";
        case SnapRejection::InsertIndexOutOfRange:  return "saga.snap.index_oor";
    }
    return "saga.snap.unknown";
}

// ─── Drop Target Builders ─────────────────────────────────────────────────────

BlockDropTarget BlockDropTarget::MakeEmptyCanvas() noexcept
{
    BlockDropTarget t;
    t.kind = DropTargetKind::EmptyCanvas;
    return t;
}

BlockDropTarget BlockDropTarget::MakeStackTop(BlockStackId stack) noexcept
{
    BlockDropTarget t;
    t.kind        = DropTargetKind::StackTop;
    t.targetStack = stack;
    return t;
}

BlockDropTarget BlockDropTarget::MakeStackBottom(BlockStackId stack) noexcept
{
    BlockDropTarget t;
    t.kind        = DropTargetKind::StackBottom;
    t.targetStack = stack;
    return t;
}

BlockDropTarget BlockDropTarget::MakeStackBetween(BlockStackId stack,
                                                  std::size_t  index) noexcept
{
    BlockDropTarget t;
    t.kind        = DropTargetKind::StackBetween;
    t.targetStack = stack;
    t.insertIndex = index;
    return t;
}

BlockDropTarget BlockDropTarget::MakeExpressionSlot(BlockInstanceId host,
                                                    std::string     slotId)
{
    BlockDropTarget t;
    t.kind         = DropTargetKind::ExpressionSlot;
    t.hostInstance = host;
    t.slotId       = std::move(slotId);
    return t;
}

BlockDropTarget BlockDropTarget::MakeMouth(BlockInstanceId host,
                                            std::uint8_t    mouthIndex) noexcept
{
    BlockDropTarget t;
    t.kind         = DropTargetKind::Mouth;
    t.hostInstance = host;
    t.mouthIndex   = mouthIndex;
    return t;
}

// ─── Helper Predicates ────────────────────────────────────────────────────────

namespace
{

[[nodiscard]] SnapVerdict Accept() noexcept
{
    SnapVerdict v;
    v.accepted  = true;
    v.rejection = SnapRejection::Accepted;
    return v;
}

[[nodiscard]] SnapVerdict Reject(SnapRejection r) noexcept
{
    SnapVerdict v;
    v.accepted  = false;
    v.rejection = r;
    return v;
}

[[nodiscard]] bool SlotTypeMatches(BlockSlotKind   slotKind,
                                   BlockOutputType outputType) noexcept
{
    if (outputType == BlockOutputType::Any)
    {
        // Variable getters are polymorphic — accepted by any value-bearing slot.
        return BlockSlotAcceptsExpression(slotKind);
    }
    switch (slotKind)
    {
        case BlockSlotKind::Number:
            return outputType == BlockOutputType::Number;
        case BlockSlotKind::Text:
            return outputType == BlockOutputType::Text ||
                   outputType == BlockOutputType::Number; // numbers print as text
        case BlockSlotKind::Boolean:
            return outputType == BlockOutputType::Boolean;
        case BlockSlotKind::Variable:
            return true; // variable slots accept any reporter
        case BlockSlotKind::Dropdown:
        case BlockSlotKind::Colour:
        case BlockSlotKind::Procedure:
        case BlockSlotKind::Statement:
            return false;
    }
    return false;
}

/// Find a slot definition on the host's block definition by id.
[[nodiscard]] const BlockSlotDefinition*
FindSlot(const BlockDefinition& def, const std::string& slotId) noexcept
{
    const auto it = std::find_if(def.slots.begin(), def.slots.end(),
        [&](const BlockSlotDefinition& s) noexcept { return s.id == slotId; });
    return it == def.slots.end() ? nullptr : &*it;
}

[[nodiscard]] bool StackEndsInCap(const BlockStack&   stack,
                                  const BlockScript&  script,
                                  const BlockLibrary& library) noexcept
{
    if (stack.IsEmpty()) return false;
    const auto* tail = script.FindInstance(stack.instances.back());
    if (tail == nullptr) return false;
    const auto* def = library.FindDefinition(tail->opcode);
    return def != nullptr && def->kind == BlockKind::Cap;
}

} // namespace

// ─── CanSnap ──────────────────────────────────────────────────────────────────

SnapVerdict CanSnap(const std::string&     draggedOpcode,
                    const BlockDropTarget& target,
                    const BlockScript&     script,
                    const BlockLibrary&    library) noexcept
{
    const BlockDefinition* draggedDef = library.FindDefinition(draggedOpcode);
    if (draggedDef == nullptr)
    {
        return Reject(SnapRejection::UnknownOpcode);
    }

    const BlockKind kind = draggedDef->kind;

    switch (target.kind)
    {
        // ─── EmptyCanvas ──────────────────────────────────────────────────────

        case DropTargetKind::EmptyCanvas:
        {
            // Expression blocks need a slot — they cannot live as a
            // free-standing stack head.
            if (BlockKindIsExpression(kind))
            {
                return Reject(SnapRejection::ExpressionInStack);
            }
            return Accept();
        }

        // ─── Stack Targets ────────────────────────────────────────────────────

        case DropTargetKind::StackTop:
        {
            const auto* stack = script.FindStack(target.targetStack);
            if (stack == nullptr) return Reject(SnapRejection::InvalidStack);
            if (BlockKindIsExpression(kind))
            {
                return Reject(SnapRejection::ExpressionInStack);
            }
            // Hat displaces; only Stack / C / E / Cap go above an existing head.
            if (kind == BlockKind::Hat)
            {
                return Reject(SnapRejection::HatNotAllowedHere);
            }
            return Accept();
        }

        case DropTargetKind::StackBottom:
        {
            const auto* stack = script.FindStack(target.targetStack);
            if (stack == nullptr) return Reject(SnapRejection::InvalidStack);
            if (BlockKindIsExpression(kind))
            {
                return Reject(SnapRejection::ExpressionInStack);
            }
            if (kind == BlockKind::Hat)
            {
                return Reject(SnapRejection::HatMustStartStack);
            }
            // A stack that already terminates in a Cap accepts no more
            // blocks beneath it — the cap is the end.
            if (StackEndsInCap(*stack, script, library))
            {
                return Reject(SnapRejection::CapCannotPrecedeStack);
            }
            return Accept();
        }

        case DropTargetKind::StackBetween:
        {
            const auto* stack = script.FindStack(target.targetStack);
            if (stack == nullptr) return Reject(SnapRejection::InvalidStack);
            if (target.insertIndex > stack->instances.size())
            {
                return Reject(SnapRejection::InsertIndexOutOfRange);
            }
            if (BlockKindIsExpression(kind))
            {
                return Reject(SnapRejection::ExpressionInStack);
            }
            if (kind == BlockKind::Hat)
            {
                return Reject(SnapRejection::HatMustStartStack);
            }
            // Cap may only land at the end — a between-position past the
            // last index is StackBottom; anything earlier is illegal.
            if (kind == BlockKind::Cap &&
                target.insertIndex < stack->instances.size())
            {
                return Reject(SnapRejection::CapMustEndStack);
            }
            return Accept();
        }

        // ─── Mouth ────────────────────────────────────────────────────────────

        case DropTargetKind::Mouth:
        {
            const auto* host = script.FindInstance(target.hostInstance);
            if (host == nullptr) return Reject(SnapRejection::InvalidHost);
            const auto* hostDef = library.FindDefinition(host->opcode);
            if (hostDef == nullptr) return Reject(SnapRejection::InvalidHost);
            if (target.mouthIndex >= BlockKindMouthCount(hostDef->kind))
            {
                return Reject(SnapRejection::MouthIndexOutOfRange);
            }
            if (BlockKindIsExpression(kind))
            {
                return Reject(SnapRejection::ExpressionInStack);
            }
            if (kind == BlockKind::Hat)
            {
                return Reject(SnapRejection::HatMustStartStack);
            }
            return Accept();
        }

        // ─── ExpressionSlot ───────────────────────────────────────────────────

        case DropTargetKind::ExpressionSlot:
        {
            const auto* host = script.FindInstance(target.hostInstance);
            if (host == nullptr) return Reject(SnapRejection::InvalidHost);
            const auto* hostDef = library.FindDefinition(host->opcode);
            if (hostDef == nullptr) return Reject(SnapRejection::InvalidHost);

            const auto* slot = FindSlot(*hostDef, target.slotId);
            if (slot == nullptr)
            {
                return Reject(SnapRejection::HostHasNoSuchSlot);
            }
            if (!BlockSlotAcceptsExpression(slot->kind))
            {
                return Reject(SnapRejection::SlotDoesNotAccept);
            }
            if (!BlockKindIsExpression(kind))
            {
                return Reject(SnapRejection::StackBlockInExpression);
            }
            if (!SlotTypeMatches(slot->kind, draggedDef->outputType))
            {
                return Reject(SnapRejection::SlotTypeMismatch);
            }
            return Accept();
        }
    }

    return Reject(SnapRejection::UnknownOpcode);
}

} // namespace SagaEditor::VisualScripting
