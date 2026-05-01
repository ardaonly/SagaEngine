/// @file BlockDefinition.cpp
/// @brief Implementation of the block-definition palette template.

#include "SagaEditor/VisualScripting/Blocks/BlockDefinition.h"

#include <utility>

namespace SagaEditor::VisualScripting
{

// ─── Label Fragment Builders ──────────────────────────────────────────────────

BlockLabelFragment BlockLabelFragment::MakeLiteral(std::string text)
{
    BlockLabelFragment f;
    f.kind = Kind::Literal;
    f.text = std::move(text);
    return f;
}

BlockLabelFragment BlockLabelFragment::MakeSlot(std::string slotId)
{
    BlockLabelFragment f;
    f.kind = Kind::SlotRef;
    f.text = std::move(slotId);
    return f;
}

BlockLabelFragment BlockLabelFragment::MakeLineBreak() noexcept
{
    BlockLabelFragment f;
    f.kind = Kind::LineBreak;
    return f;
}

// ─── Output Type Identity ─────────────────────────────────────────────────────

const char* BlockOutputTypeId(BlockOutputType type) noexcept
{
    switch (type)
    {
        case BlockOutputType::None:    return "saga.output.none";
        case BlockOutputType::Number:  return "saga.output.number";
        case BlockOutputType::Text:    return "saga.output.text";
        case BlockOutputType::Boolean: return "saga.output.boolean";
        case BlockOutputType::Any:     return "saga.output.any";
    }
    return "saga.output.unknown";
}

BlockOutputType BlockOutputTypeFromId(const std::string& id) noexcept
{
    if (id == "saga.output.none")    return BlockOutputType::None;
    if (id == "saga.output.number")  return BlockOutputType::Number;
    if (id == "saga.output.text")    return BlockOutputType::Text;
    if (id == "saga.output.boolean") return BlockOutputType::Boolean;
    if (id == "saga.output.any")     return BlockOutputType::Any;
    return BlockOutputType::None;
}

// ─── Definition Equality ──────────────────────────────────────────────────────

bool BlockDefinition::operator==(const BlockDefinition& o) const noexcept
{
    return opcode         == o.opcode
        && displayName    == o.displayName
        && categoryId     == o.categoryId
        && description    == o.description
        && kind           == o.kind
        && outputType     == o.outputType
        && labelFragments == o.labelFragments
        && slots          == o.slots
        && mouthLabels    == o.mouthLabels
        && hatEventId     == o.hatEventId;
}

} // namespace SagaEditor::VisualScripting
