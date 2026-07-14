/// @file BlockIR.cpp
/// @brief Implementation of the canonical IR types + value/statement builders.

#include "SagaEditor/VisualScripting/Blocks/BlockIR.h"

#include <utility>

namespace SagaEditor::VisualScripting
{

// ─── IRType Identity ──────────────────────────────────────────────────────────

const char* IRTypeId(IRType type) noexcept
{
    switch (type)
    {
        case IRType::Void:    return "saga.ir.void";
        case IRType::Number:  return "saga.ir.number";
        case IRType::Text:    return "saga.ir.text";
        case IRType::Boolean: return "saga.ir.boolean";
        case IRType::Color:   return "saga.ir.color";
        case IRType::Any:     return "saga.ir.any";
    }
    return "saga.ir.unknown";
}

IRType IRTypeFromId(const std::string& id) noexcept
{
    if (id == "saga.ir.void")    return IRType::Void;
    if (id == "saga.ir.number")  return IRType::Number;
    if (id == "saga.ir.text")    return IRType::Text;
    if (id == "saga.ir.boolean") return IRType::Boolean;
    if (id == "saga.ir.color")   return IRType::Color;
    if (id == "saga.ir.any")     return IRType::Any;
    return IRType::Any;
}

// ─── IRValue Builders ─────────────────────────────────────────────────────────

IRValuePtr IRValue::MakeEmpty()
{
    auto v   = std::make_shared<IRValue>();
    v->tag   = Tag::Empty;
    v->type  = IRType::Void;
    return v;
}

IRValuePtr IRValue::MakeNumber(double n)
{
    auto v          = std::make_shared<IRValue>();
    v->tag          = Tag::Literal;
    v->type         = IRType::Number;
    v->literalNumber = n;
    return v;
}

IRValuePtr IRValue::MakeText(std::string t)
{
    auto v        = std::make_shared<IRValue>();
    v->tag        = Tag::Literal;
    v->type       = IRType::Text;
    v->literalText = std::move(t);
    return v;
}

IRValuePtr IRValue::MakeBool(bool b)
{
    auto v        = std::make_shared<IRValue>();
    v->tag        = Tag::Literal;
    v->type       = IRType::Boolean;
    v->literalBool = b;
    return v;
}

IRValuePtr IRValue::MakeColor(std::uint32_t rgba)
{
    auto v         = std::make_shared<IRValue>();
    v->tag         = Tag::Literal;
    v->type        = IRType::Color;
    v->literalColor = rgba;
    return v;
}

IRValuePtr IRValue::MakeVariableRef(std::string variableId, IRType varType)
{
    auto v        = std::make_shared<IRValue>();
    v->tag        = Tag::VariableRef;
    v->type       = varType;
    v->variableId = std::move(variableId);
    return v;
}

IRValuePtr IRValue::MakeCall(std::string opcode,
                              IRType returnType,
                              std::vector<IRValuePtr> args)
{
    auto v       = std::make_shared<IRValue>();
    v->tag       = Tag::Call;
    v->type      = returnType;
    v->opcode    = std::move(opcode);
    v->arguments = std::move(args);
    return v;
}

bool IRValue::Equals(const IRValue& o) const noexcept
{
    if (tag != o.tag || type != o.type) return false;
    switch (tag)
    {
        case Tag::Empty:       return true;
        case Tag::Literal:
            return literalNumber == o.literalNumber
                && literalText   == o.literalText
                && literalBool   == o.literalBool
                && literalColor  == o.literalColor;
        case Tag::VariableRef: return variableId == o.variableId;
        case Tag::Call:
            if (opcode != o.opcode) return false;
            if (arguments.size() != o.arguments.size()) return false;
            for (std::size_t i = 0; i < arguments.size(); ++i)
            {
                const auto& a = arguments[i];
                const auto& b = o.arguments[i];
                if (!a && !b) continue;
                if (!a || !b) return false;
                if (!a->Equals(*b)) return false;
            }
            return true;
    }
    return false;
}

// ─── IRStatement Builders ─────────────────────────────────────────────────────

IRStatementPtr IRStatement::MakeCall(std::string opcode,
                                      std::vector<IRValuePtr> args)
{
    auto s     = std::make_shared<IRStatement>();
    s->tag     = Tag::Call;
    s->opcode  = std::move(opcode);
    s->arguments = std::move(args);
    return s;
}

IRStatementPtr IRStatement::MakeIf(IRValuePtr condition,
                                    std::vector<IRStatementPtr> body)
{
    auto s        = std::make_shared<IRStatement>();
    s->tag        = Tag::If;
    s->condition  = std::move(condition);
    s->thenBody   = std::move(body);
    return s;
}

IRStatementPtr IRStatement::MakeIfElse(IRValuePtr condition,
                                        std::vector<IRStatementPtr> thenBody,
                                        std::vector<IRStatementPtr> elseBody)
{
    auto s        = std::make_shared<IRStatement>();
    s->tag        = Tag::IfElse;
    s->condition  = std::move(condition);
    s->thenBody   = std::move(thenBody);
    s->elseBody   = std::move(elseBody);
    return s;
}

IRStatementPtr IRStatement::MakeRepeat(IRValuePtr count,
                                        std::vector<IRStatementPtr> body)
{
    auto s        = std::make_shared<IRStatement>();
    s->tag        = Tag::Repeat;
    s->repeatCount = std::move(count);
    s->thenBody   = std::move(body);
    return s;
}

IRStatementPtr IRStatement::MakeForever(std::vector<IRStatementPtr> body)
{
    auto s     = std::make_shared<IRStatement>();
    s->tag     = Tag::Forever;
    s->thenBody = std::move(body);
    return s;
}

IRStatementPtr IRStatement::MakeStop()
{
    auto s = std::make_shared<IRStatement>();
    s->tag = Tag::Stop;
    return s;
}

IRStatementPtr IRStatement::MakeProcedureCall(std::string id,
                                               std::vector<IRValuePtr> args)
{
    auto s         = std::make_shared<IRStatement>();
    s->tag         = Tag::ProcedureCall;
    s->procedureId = std::move(id);
    s->arguments   = std::move(args);
    return s;
}

// ─── IRProgram ────────────────────────────────────────────────────────────────

const IRProcedure* IRProgram::FindProcedure(const std::string& id) const noexcept
{
    for (const auto& p : procedures)
    {
        if (p.id == id) return &p;
    }
    return nullptr;
}

std::vector<const IRProcedure*> IRProgram::EntryPoints() const
{
    std::vector<const IRProcedure*> out;
    for (const auto& p : procedures)
    {
        if (p.isEntryPoint) out.push_back(&p);
    }
    return out;
}

} // namespace SagaEditor::VisualScripting
