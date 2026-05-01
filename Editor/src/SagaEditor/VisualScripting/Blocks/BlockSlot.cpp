/// @file BlockSlot.cpp
/// @brief Implementation of slot identity, definition, and fill types.

#include "SagaEditor/VisualScripting/Blocks/BlockSlot.h"

#include <utility>

namespace SagaEditor::VisualScripting
{

// ─── Slot Kind Identity ───────────────────────────────────────────────────────

const char* BlockSlotKindId(BlockSlotKind kind) noexcept
{
    switch (kind)
    {
        case BlockSlotKind::Number:    return "saga.slotkind.number";
        case BlockSlotKind::Text:      return "saga.slotkind.text";
        case BlockSlotKind::Boolean:   return "saga.slotkind.boolean";
        case BlockSlotKind::Dropdown:  return "saga.slotkind.dropdown";
        case BlockSlotKind::Colour:    return "saga.slotkind.colour";
        case BlockSlotKind::Variable:  return "saga.slotkind.variable";
        case BlockSlotKind::Procedure: return "saga.slotkind.procedure";
        case BlockSlotKind::Statement: return "saga.slotkind.statement";
    }
    return "saga.slotkind.unknown";
}

BlockSlotKind BlockSlotKindFromId(const std::string& id) noexcept
{
    if (id == "saga.slotkind.number")    return BlockSlotKind::Number;
    if (id == "saga.slotkind.text")      return BlockSlotKind::Text;
    if (id == "saga.slotkind.boolean")   return BlockSlotKind::Boolean;
    if (id == "saga.slotkind.dropdown")  return BlockSlotKind::Dropdown;
    if (id == "saga.slotkind.colour")    return BlockSlotKind::Colour;
    if (id == "saga.slotkind.variable")  return BlockSlotKind::Variable;
    if (id == "saga.slotkind.procedure") return BlockSlotKind::Procedure;
    if (id == "saga.slotkind.statement") return BlockSlotKind::Statement;
    return BlockSlotKind::Number;
}

// ─── Slot Definition Equality ─────────────────────────────────────────────────

bool BlockSlotDefinition::operator==(const BlockSlotDefinition& o) const noexcept
{
    return id            == o.id
        && label         == o.label
        && kind          == o.kind
        && defaultNumber == o.defaultNumber
        && defaultText   == o.defaultText
        && defaultBool   == o.defaultBool
        && defaultColor  == o.defaultColor
        && options       == o.options;
}

// ─── Slot Fill Equality ───────────────────────────────────────────────────────

bool BlockSlotFill::operator==(const BlockSlotFill& o) const noexcept
{
    if (kind != o.kind) return false;
    switch (kind)
    {
        case Kind::Empty:        return true;
        case Kind::Number:       return numberValue == o.numberValue;
        case Kind::Text:         return textValue   == o.textValue;
        case Kind::Boolean:      return boolValue   == o.boolValue;
        case Kind::Color:        return colorValue  == o.colorValue;
        case Kind::Dropdown:
        case Kind::VariableRef:
        case Kind::ProcedureRef: return refId       == o.refId;
        case Kind::Expression:   return expressionId == o.expressionId;
    }
    return false;
}

// ─── Slot Fill Builders ───────────────────────────────────────────────────────

BlockSlotFill BlockSlotFill::MakeEmpty() noexcept
{
    return {};
}

BlockSlotFill BlockSlotFill::MakeNumber(double v) noexcept
{
    BlockSlotFill f;
    f.kind        = Kind::Number;
    f.numberValue = v;
    return f;
}

BlockSlotFill BlockSlotFill::MakeText(std::string v)
{
    BlockSlotFill f;
    f.kind      = Kind::Text;
    f.textValue = std::move(v);
    return f;
}

BlockSlotFill BlockSlotFill::MakeBool(bool v) noexcept
{
    BlockSlotFill f;
    f.kind      = Kind::Boolean;
    f.boolValue = v;
    return f;
}

BlockSlotFill BlockSlotFill::MakeColor(std::uint32_t rgba) noexcept
{
    BlockSlotFill f;
    f.kind       = Kind::Color;
    f.colorValue = rgba;
    return f;
}

BlockSlotFill BlockSlotFill::MakeDropdown(std::string optionId)
{
    BlockSlotFill f;
    f.kind  = Kind::Dropdown;
    f.refId = std::move(optionId);
    return f;
}

BlockSlotFill BlockSlotFill::MakeVariable(std::string variableId)
{
    BlockSlotFill f;
    f.kind  = Kind::VariableRef;
    f.refId = std::move(variableId);
    return f;
}

BlockSlotFill BlockSlotFill::MakeProcedure(std::string procedureId)
{
    BlockSlotFill f;
    f.kind  = Kind::ProcedureRef;
    f.refId = std::move(procedureId);
    return f;
}

BlockSlotFill BlockSlotFill::MakeExpression(std::uint64_t instanceId) noexcept
{
    BlockSlotFill f;
    f.kind         = Kind::Expression;
    f.expressionId = instanceId;
    return f;
}

} // namespace SagaEditor::VisualScripting
