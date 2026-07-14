/// @file BlockToIRLowerer.cpp
/// @brief BlockScript → IRProgram lowering pass.

#include "SagaEditor/VisualScripting/Blocks/BlockToIRLowerer.h"

#include "SagaEditor/VisualScripting/Blocks/BlockLibrary.h"
#include "SagaEditor/VisualScripting/Blocks/BlockScript.h"

#include <algorithm>
#include <utility>

namespace SagaEditor::VisualScripting
{

bool LowerResult::Ok() const noexcept
{
    return std::none_of(diagnostics.begin(), diagnostics.end(),
        [](const LowerDiagnostic& d) noexcept
        { return d.severity == LowerDiagnostic::Severity::Error; });
}

namespace
{

// ─── Lowering Context ─────────────────────────────────────────────────────────

/// Threaded through every recursive call so warnings / errors can be
/// pushed without stretching every helper signature.
struct LowerCtx
{
    const BlockScript&            script;
    const BlockLibrary&           library;
    std::vector<LowerDiagnostic>& diagnostics;

    void Warn(std::string msg, std::string opcode = {})
    {
        diagnostics.push_back({ LowerDiagnostic::Severity::Warning,
                                std::move(msg), std::move(opcode) });
    }
    void Error(std::string msg, std::string opcode = {})
    {
        diagnostics.push_back({ LowerDiagnostic::Severity::Error,
                                std::move(msg), std::move(opcode) });
    }
};

[[nodiscard]] IRType MapOutputType(BlockOutputType ot) noexcept
{
    switch (ot)
    {
        case BlockOutputType::None:    return IRType::Void;
        case BlockOutputType::Number:  return IRType::Number;
        case BlockOutputType::Text:    return IRType::Text;
        case BlockOutputType::Boolean: return IRType::Boolean;
        case BlockOutputType::Any:     return IRType::Any;
    }
    return IRType::Any;
}

[[nodiscard]] IRType MapSlotType(BlockSlotKind k) noexcept
{
    switch (k)
    {
        case BlockSlotKind::Number:    return IRType::Number;
        case BlockSlotKind::Text:      return IRType::Text;
        case BlockSlotKind::Boolean:   return IRType::Boolean;
        case BlockSlotKind::Colour:    return IRType::Color;
        case BlockSlotKind::Variable:  return IRType::Any;
        case BlockSlotKind::Procedure: return IRType::Void;
        case BlockSlotKind::Dropdown:
        case BlockSlotKind::Statement: return IRType::Text;
    }
    return IRType::Any;
}

// Forward decls for recursion.
IRValuePtr                    LowerSlotFill(LowerCtx& ctx,
                                            const BlockSlotFill& fill,
                                            const BlockSlotDefinition& slotDef);
std::vector<IRStatementPtr>   LowerStack    (LowerCtx& ctx,
                                              BlockStackId stackId);

// ─── Slot Lowering ────────────────────────────────────────────────────────────

IRValuePtr LowerExpressionInstance(LowerCtx& ctx, std::uint64_t exprInstanceId)
{
    BlockInstanceId id; id.value = exprInstanceId;
    const BlockInstance* inst = ctx.script.FindInstance(id);
    if (inst == nullptr)
    {
        ctx.Error("Slot expression references missing instance");
        return IRValue::MakeEmpty();
    }
    const BlockDefinition* def = ctx.library.FindDefinition(inst->opcode);
    if (def == nullptr)
    {
        ctx.Error("Slot expression has unknown opcode: " + inst->opcode,
                  inst->opcode);
        return IRValue::MakeEmpty();
    }
    if (!BlockKindIsExpression(def->kind))
    {
        ctx.Error("Slot expression block is not a Reporter / Boolean: "
                  + inst->opcode, inst->opcode);
        return IRValue::MakeEmpty();
    }

    // Collect arguments by walking the expression's slot definitions
    // in declaration order so the IR call's argument order matches the
    // visual reading order of the block.
    std::vector<IRValuePtr> args;
    args.reserve(def->slots.size());
    for (const auto& slotDef : def->slots)
    {
        const auto it = inst->slotFills.find(slotDef.id);
        if (it == inst->slotFills.end())
        {
            args.push_back(IRValue::MakeEmpty());
            continue;
        }
        args.push_back(LowerSlotFill(ctx, it->second, slotDef));
    }

    return IRValue::MakeCall(inst->opcode, MapOutputType(def->outputType),
                             std::move(args));
}

IRValuePtr LowerSlotFill(LowerCtx&                  ctx,
                          const BlockSlotFill&       fill,
                          const BlockSlotDefinition& slotDef)
{
    using K = BlockSlotFill::Kind;
    switch (fill.kind)
    {
        case K::Empty:
        {
            // Fill the slot with the definition's default literal so
            // the runtime never sees an "empty" expression.
            switch (slotDef.kind)
            {
                case BlockSlotKind::Number:
                    return IRValue::MakeNumber(slotDef.defaultNumber);
                case BlockSlotKind::Text:
                    return IRValue::MakeText(slotDef.defaultText);
                case BlockSlotKind::Boolean:
                    return IRValue::MakeBool(slotDef.defaultBool);
                case BlockSlotKind::Colour:
                    return IRValue::MakeColor(slotDef.defaultColor);
                default:
                    return IRValue::MakeEmpty();
            }
        }
        case K::Number:    return IRValue::MakeNumber(fill.numberValue);
        case K::Text:      return IRValue::MakeText  (fill.textValue);
        case K::Boolean:   return IRValue::MakeBool  (fill.boolValue);
        case K::Color:     return IRValue::MakeColor (fill.colorValue);
        case K::Dropdown:
            return IRValue::MakeText(fill.refId);   // canonicalised as text
        case K::VariableRef:
            return IRValue::MakeVariableRef(fill.refId, MapSlotType(slotDef.kind));
        case K::ProcedureRef:
            return IRValue::MakeText(fill.refId);   // procedure name as text
        case K::Expression:
            return LowerExpressionInstance(ctx, fill.expressionId);
    }
    return IRValue::MakeEmpty();
}

// ─── Stack Lowering ───────────────────────────────────────────────────────────

IRStatementPtr LowerControlBlock(LowerCtx&             ctx,
                                  const BlockInstance&  inst,
                                  const BlockDefinition& def)
{
    // Lift control-flow opcodes the editor ships into branch /
    // loop IR statements; everything else falls through to a
    // generic call.
    if (def.opcode == "saga.block.control.repeat")
    {
        IRValuePtr count = IRValue::MakeNumber(10.0);
        if (auto it = inst.slotFills.find("TIMES");
            it != inst.slotFills.end() && !def.slots.empty())
        {
            count = LowerSlotFill(ctx, it->second, def.slots.front());
        }
        std::vector<IRStatementPtr> body;
        if (!inst.mouthStacks.empty())
        {
            body = LowerStack(ctx, inst.mouthStacks.front());
        }
        return IRStatement::MakeRepeat(std::move(count), std::move(body));
    }
    if (def.opcode == "saga.block.control.if")
    {
        IRValuePtr cond = IRValue::MakeBool(false);
        if (auto it = inst.slotFills.find("CONDITION");
            it != inst.slotFills.end() && !def.slots.empty())
        {
            cond = LowerSlotFill(ctx, it->second, def.slots.front());
        }
        std::vector<IRStatementPtr> body;
        if (!inst.mouthStacks.empty())
        {
            body = LowerStack(ctx, inst.mouthStacks.front());
        }
        return IRStatement::MakeIf(std::move(cond), std::move(body));
    }
    if (def.opcode == "saga.block.control.if_else")
    {
        IRValuePtr cond = IRValue::MakeBool(false);
        if (auto it = inst.slotFills.find("CONDITION");
            it != inst.slotFills.end() && !def.slots.empty())
        {
            cond = LowerSlotFill(ctx, it->second, def.slots.front());
        }
        std::vector<IRStatementPtr> thenBody;
        std::vector<IRStatementPtr> elseBody;
        if (inst.mouthStacks.size() >= 1)
            thenBody = LowerStack(ctx, inst.mouthStacks[0]);
        if (inst.mouthStacks.size() >= 2)
            elseBody = LowerStack(ctx, inst.mouthStacks[1]);
        return IRStatement::MakeIfElse(std::move(cond),
                                       std::move(thenBody),
                                       std::move(elseBody));
    }
    if (def.opcode == "saga.block.control.forever")
    {
        std::vector<IRStatementPtr> body;
        if (!inst.mouthStacks.empty())
        {
            body = LowerStack(ctx, inst.mouthStacks.front());
        }
        return IRStatement::MakeForever(std::move(body));
    }
    return nullptr;
}

IRStatementPtr LowerInstance(LowerCtx& ctx, const BlockInstance& inst)
{
    const BlockDefinition* def = ctx.library.FindDefinition(inst.opcode);
    if (def == nullptr)
    {
        ctx.Error("Unknown opcode: " + inst.opcode, inst.opcode);
        return IRStatement::MakeCall(inst.opcode, {});
    }

    if (def->kind == BlockKind::Cap)
    {
        return IRStatement::MakeStop();
    }

    if (def->kind == BlockKind::CBlock || def->kind == BlockKind::EBlock)
    {
        if (auto stmt = LowerControlBlock(ctx, inst, *def))
        {
            return stmt;
        }
        // Unknown C/E opcode falls through to a generic call to keep
        // the IR well-formed; the runtime compiler will reject it.
    }

    // Generic stack call: walk the slot definitions in declaration
    // order and lower each fill into an argument value.
    std::vector<IRValuePtr> args;
    args.reserve(def->slots.size());
    for (const auto& slotDef : def->slots)
    {
        const auto it = inst.slotFills.find(slotDef.id);
        if (it == inst.slotFills.end())
        {
            args.push_back(LowerSlotFill(ctx, BlockSlotFill::MakeEmpty(), slotDef));
        }
        else
        {
            args.push_back(LowerSlotFill(ctx, it->second, slotDef));
        }
    }
    return IRStatement::MakeCall(inst.opcode, std::move(args));
}

std::vector<IRStatementPtr> LowerStack(LowerCtx& ctx, BlockStackId stackId)
{
    std::vector<IRStatementPtr> out;
    const BlockStack* stack = ctx.script.FindStack(stackId);
    if (stack == nullptr) return out;
    out.reserve(stack->instances.size());
    for (const auto& instId : stack->instances)
    {
        const BlockInstance* inst = ctx.script.FindInstance(instId);
        if (inst == nullptr)
        {
            ctx.Error("Stack references missing instance");
            continue;
        }
        out.push_back(LowerInstance(ctx, *inst));
    }
    return out;
}

// ─── Procedure Lowering ───────────────────────────────────────────────────────

IRProcedure LowerHatStack(LowerCtx& ctx, BlockStackId stackId, std::size_t index)
{
    IRProcedure proc;
    const BlockStack* stack = ctx.script.FindStack(stackId);
    if (stack == nullptr || stack->IsEmpty())
    {
        ctx.Warn("Empty stack at root");
        proc.id           = "saga.proc.empty." + std::to_string(index);
        proc.displayName  = "Empty";
        proc.isEntryPoint = false;
        return proc;
    }

    const BlockInstanceId headId = stack->instances.front();
    const BlockInstance* head    = ctx.script.FindInstance(headId);
    if (head == nullptr)
    {
        ctx.Error("Stack head references missing instance");
        proc.id = "saga.proc.broken." + std::to_string(index);
        return proc;
    }
    const BlockDefinition* headDef = ctx.library.FindDefinition(head->opcode);
    if (headDef == nullptr)
    {
        ctx.Error("Unknown opcode at stack head: " + head->opcode, head->opcode);
        proc.id = "saga.proc.unknown." + std::to_string(index);
        return proc;
    }

    if (headDef->kind == BlockKind::Hat)
    {
        proc.isEntryPoint = true;
        proc.eventId      = headDef->hatEventId;
        proc.id           = "saga.entry." + headDef->opcode + "." + std::to_string(index);
        proc.displayName  = headDef->displayName;

        // Body is everything below the hat.
        for (std::size_t i = 1; i < stack->instances.size(); ++i)
        {
            const BlockInstance* inst = ctx.script.FindInstance(stack->instances[i]);
            if (inst == nullptr) continue;
            proc.body.push_back(LowerInstance(ctx, *inst));
        }
        return proc;
    }

    // Non-hat root — treat the whole stack as a free-floating shelf.
    proc.isEntryPoint = false;
    proc.id           = "saga.shelf." + std::to_string(index);
    proc.displayName  = "Shelf";
    for (const auto& instId : stack->instances)
    {
        const BlockInstance* inst = ctx.script.FindInstance(instId);
        if (inst == nullptr) continue;
        proc.body.push_back(LowerInstance(ctx, *inst));
    }
    return proc;
}

} // namespace

// ─── LowerBlockScript ─────────────────────────────────────────────────────────

LowerResult LowerBlockScript(const BlockScript&  script,
                              const BlockLibrary& library)
{
    LowerResult result;
    LowerCtx    ctx { script, library, result.diagnostics };

    const auto roots = script.GetRootStacks();
    result.program.procedures.reserve(roots.size());
    for (std::size_t i = 0; i < roots.size(); ++i)
    {
        result.program.procedures.push_back(LowerHatStack(ctx, roots[i], i));
    }
    return result;
}

} // namespace SagaEditor::VisualScripting
