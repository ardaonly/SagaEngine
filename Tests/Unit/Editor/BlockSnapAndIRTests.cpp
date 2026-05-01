/// @file BlockSnapAndIRTests.cpp
/// @brief GoogleTest coverage for snap rules, IR primitives, and the lowerer.

#include "SagaEditor/VisualScripting/Blocks/BlockIR.h"
#include "SagaEditor/VisualScripting/Blocks/BlockLibrary.h"
#include "SagaEditor/VisualScripting/Blocks/BlockScript.h"
#include "SagaEditor/VisualScripting/Blocks/BlockSnapRules.h"
#include "SagaEditor/VisualScripting/Blocks/BlockToIRLowerer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace
{

using namespace SagaEditor::VisualScripting;

// ─── Shared Fixture ───────────────────────────────────────────────────────────

class BlockSnapTest : public ::testing::Test
{
protected:
    void SetUp() override { m_lib.RegisterAllBuiltins(); }
    BlockLibrary m_lib;
    BlockScript  m_script;
};

// ─── DropTargetKind / SnapRejection Identity ──────────────────────────────────

TEST(DropTargetKindTest, IdRoundTrips)
{
    const DropTargetKind all[] = {
        DropTargetKind::EmptyCanvas, DropTargetKind::StackTop,
        DropTargetKind::StackBottom, DropTargetKind::StackBetween,
        DropTargetKind::ExpressionSlot, DropTargetKind::Mouth,
    };
    for (auto k : all)
    {
        EXPECT_EQ(DropTargetKindFromId(DropTargetKindId(k)), k);
    }
    EXPECT_EQ(DropTargetKindFromId("garbage"), DropTargetKind::EmptyCanvas);
}

TEST(SnapRejectionTest, IdsAreNonEmptyAndUnique)
{
    const SnapRejection all[] = {
        SnapRejection::Accepted, SnapRejection::UnknownOpcode,
        SnapRejection::HatMustStartStack, SnapRejection::HatNotAllowedHere,
        SnapRejection::CapMustEndStack, SnapRejection::CapCannotPrecedeStack,
        SnapRejection::ExpressionInStack, SnapRejection::StackBlockInExpression,
        SnapRejection::SlotTypeMismatch, SnapRejection::SlotDoesNotAccept,
        SnapRejection::MouthIndexOutOfRange, SnapRejection::HostHasNoSuchSlot,
        SnapRejection::InvalidStack, SnapRejection::InvalidHost,
        SnapRejection::InsertIndexOutOfRange,
    };
    std::vector<std::string> seen;
    for (auto r : all)
    {
        const std::string id = SnapRejectionId(r);
        EXPECT_FALSE(id.empty());
        for (const auto& s : seen) EXPECT_NE(id, s);
        seen.push_back(id);
    }
}

// ─── EmptyCanvas Snap ─────────────────────────────────────────────────────────

TEST_F(BlockSnapTest, EmptyCanvasAcceptsHat)
{
    auto t = BlockDropTarget::MakeEmptyCanvas();
    EXPECT_TRUE(IsAcceptedSnap("saga.block.events.when_flag_clicked",
                               t, m_script, m_lib));
}

TEST_F(BlockSnapTest, EmptyCanvasAcceptsStackBlock)
{
    auto t = BlockDropTarget::MakeEmptyCanvas();
    EXPECT_TRUE(IsAcceptedSnap("saga.block.motion.move_steps",
                               t, m_script, m_lib));
}

TEST_F(BlockSnapTest, EmptyCanvasRejectsExpression)
{
    auto t = BlockDropTarget::MakeEmptyCanvas();
    auto v = CanSnap("saga.block.operators.add", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::ExpressionInStack);
}

TEST_F(BlockSnapTest, EmptyCanvasRejectsUnknownOpcode)
{
    auto t = BlockDropTarget::MakeEmptyCanvas();
    auto v = CanSnap("saga.block.bogus", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::UnknownOpcode);
}

// ─── Stack Targets ────────────────────────────────────────────────────────────

TEST_F(BlockSnapTest, StackTopRejectsHatAndExpression)
{
    auto stack = m_script.CreateStack(
        m_script.CreateInstance("saga.block.motion.move_steps"));
    auto t = BlockDropTarget::MakeStackTop(stack);

    auto v1 = CanSnap("saga.block.events.when_flag_clicked", t, m_script, m_lib);
    EXPECT_FALSE(v1.accepted);
    EXPECT_EQ(v1.rejection, SnapRejection::HatNotAllowedHere);

    auto v2 = CanSnap("saga.block.operators.add", t, m_script, m_lib);
    EXPECT_FALSE(v2.accepted);
    EXPECT_EQ(v2.rejection, SnapRejection::ExpressionInStack);

    EXPECT_TRUE(IsAcceptedSnap("saga.block.motion.turn_right",
                               t, m_script, m_lib));
}

TEST_F(BlockSnapTest, StackBottomRejectsHat)
{
    auto stack = m_script.CreateStack(
        m_script.CreateInstance("saga.block.motion.move_steps"));
    auto t = BlockDropTarget::MakeStackBottom(stack);

    auto v = CanSnap("saga.block.events.when_flag_clicked", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::HatMustStartStack);

    EXPECT_TRUE(IsAcceptedSnap("saga.block.motion.turn_right",
                               t, m_script, m_lib));
}

TEST_F(BlockSnapTest, StackBottomRejectsAnyDropAfterCap)
{
    auto cap = m_script.CreateInstance("saga.block.control.stop_all");
    auto stack = m_script.CreateStack(cap);
    auto t = BlockDropTarget::MakeStackBottom(stack);

    auto v = CanSnap("saga.block.motion.move_steps", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::CapCannotPrecedeStack);
}

TEST_F(BlockSnapTest, StackBetweenRejectsCapMidStack)
{
    auto move = m_script.CreateInstance("saga.block.motion.move_steps");
    auto turn = m_script.CreateInstance("saga.block.motion.turn_right");
    auto stack = m_script.CreateStack(move);
    EXPECT_TRUE(m_script.AppendToStack(stack, turn));

    // index = 1 means "between move and turn" — Cap is illegal here.
    auto t = BlockDropTarget::MakeStackBetween(stack, 1);
    auto v = CanSnap("saga.block.control.stop_all", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::CapMustEndStack);
}

TEST_F(BlockSnapTest, StackBetweenIndexOutOfRange)
{
    auto stack = m_script.CreateStack(
        m_script.CreateInstance("saga.block.motion.move_steps"));
    auto t = BlockDropTarget::MakeStackBetween(stack, 99);
    auto v = CanSnap("saga.block.motion.turn_right", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::InsertIndexOutOfRange);
}

TEST_F(BlockSnapTest, InvalidStackTargetIsRejected)
{
    BlockStackId bogus; bogus.value = 9999;
    auto t = BlockDropTarget::MakeStackBottom(bogus);
    auto v = CanSnap("saga.block.motion.move_steps", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::InvalidStack);
}

// ─── Mouth Targets ────────────────────────────────────────────────────────────

TEST_F(BlockSnapTest, MouthAcceptsStackBlock)
{
    auto repeat = m_script.CreateInstance("saga.block.control.repeat");
    auto t = BlockDropTarget::MakeMouth(repeat, 0);
    EXPECT_TRUE(IsAcceptedSnap("saga.block.motion.move_steps",
                               t, m_script, m_lib));
}

TEST_F(BlockSnapTest, MouthRejectsExpression)
{
    auto repeat = m_script.CreateInstance("saga.block.control.repeat");
    auto t = BlockDropTarget::MakeMouth(repeat, 0);
    auto v = CanSnap("saga.block.operators.add", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::ExpressionInStack);
}

TEST_F(BlockSnapTest, MouthIndexOutOfRange)
{
    auto repeat = m_script.CreateInstance("saga.block.control.repeat");
    auto t = BlockDropTarget::MakeMouth(repeat, 5);
    auto v = CanSnap("saga.block.motion.move_steps", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::MouthIndexOutOfRange);
}

// ─── Expression Slot ──────────────────────────────────────────────────────────

TEST_F(BlockSnapTest, BooleanSlotAcceptsMatchingExpression)
{
    auto ifBlock = m_script.CreateInstance("saga.block.control.if");
    auto t = BlockDropTarget::MakeExpressionSlot(ifBlock, "CONDITION");
    EXPECT_TRUE(IsAcceptedSnap("saga.block.sensing.touching_edge",
                               t, m_script, m_lib));
}

TEST_F(BlockSnapTest, BooleanSlotRejectsNumberExpression)
{
    auto ifBlock = m_script.CreateInstance("saga.block.control.if");
    auto t = BlockDropTarget::MakeExpressionSlot(ifBlock, "CONDITION");
    auto v = CanSnap("saga.block.operators.add", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::SlotTypeMismatch);
}

TEST_F(BlockSnapTest, NumberSlotAcceptsAnyVariableGetter)
{
    auto repeat = m_script.CreateInstance("saga.block.control.repeat");
    auto t = BlockDropTarget::MakeExpressionSlot(repeat, "TIMES");
    // Variable-getter has output type Any — must be accepted here.
    EXPECT_TRUE(IsAcceptedSnap("saga.block.variables.get",
                               t, m_script, m_lib));
}

TEST_F(BlockSnapTest, ExpressionSlotRejectsStackBlock)
{
    auto repeat = m_script.CreateInstance("saga.block.control.repeat");
    auto t = BlockDropTarget::MakeExpressionSlot(repeat, "TIMES");
    auto v = CanSnap("saga.block.motion.move_steps", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::StackBlockInExpression);
}

TEST_F(BlockSnapTest, ExpressionSlotMissingSlotId)
{
    auto repeat = m_script.CreateInstance("saga.block.control.repeat");
    auto t = BlockDropTarget::MakeExpressionSlot(repeat, "DOES_NOT_EXIST");
    auto v = CanSnap("saga.block.operators.add", t, m_script, m_lib);
    EXPECT_FALSE(v.accepted);
    EXPECT_EQ(v.rejection, SnapRejection::HostHasNoSuchSlot);
}

// ─── IR Builders ──────────────────────────────────────────────────────────────

TEST(BlockIRTest, IRTypeIdRoundTrips)
{
    const IRType all[] = {
        IRType::Void, IRType::Number, IRType::Text,
        IRType::Boolean, IRType::Color, IRType::Any,
    };
    for (auto t : all) EXPECT_EQ(IRTypeFromId(IRTypeId(t)), t);
    EXPECT_EQ(IRTypeFromId("garbage"), IRType::Any);
}

TEST(BlockIRTest, ValueBuildersTagAndType)
{
    EXPECT_EQ(IRValue::MakeEmpty()->tag,             IRValue::Tag::Empty);
    EXPECT_EQ(IRValue::MakeNumber(1.0)->type,        IRType::Number);
    EXPECT_EQ(IRValue::MakeText("a")->type,          IRType::Text);
    EXPECT_EQ(IRValue::MakeBool(true)->type,         IRType::Boolean);
    EXPECT_EQ(IRValue::MakeColor(0xFFu)->type,       IRType::Color);
    auto vr = IRValue::MakeVariableRef("score", IRType::Number);
    EXPECT_EQ(vr->tag,        IRValue::Tag::VariableRef);
    EXPECT_EQ(vr->variableId, "score");
    EXPECT_EQ(vr->type,       IRType::Number);

    auto call = IRValue::MakeCall("saga.block.operators.add", IRType::Number,
                                   { IRValue::MakeNumber(1), IRValue::MakeNumber(2) });
    EXPECT_EQ(call->tag,           IRValue::Tag::Call);
    EXPECT_EQ(call->arguments.size(), 2u);
    EXPECT_EQ(call->arguments[0]->literalNumber, 1.0);
}

TEST(BlockIRTest, ValueEqualityWalksArguments)
{
    auto a = IRValue::MakeCall("saga.op", IRType::Number,
                                { IRValue::MakeNumber(1), IRValue::MakeNumber(2) });
    auto b = IRValue::MakeCall("saga.op", IRType::Number,
                                { IRValue::MakeNumber(1), IRValue::MakeNumber(2) });
    auto c = IRValue::MakeCall("saga.op", IRType::Number,
                                { IRValue::MakeNumber(1), IRValue::MakeNumber(99) });
    EXPECT_TRUE (a->Equals(*b));
    EXPECT_FALSE(a->Equals(*c));
}

TEST(BlockIRTest, ProgramFindAndEntryPoints)
{
    IRProgram prog;
    IRProcedure entry; entry.id = "e1"; entry.isEntryPoint = true;
    IRProcedure helper; helper.id = "h1"; helper.isEntryPoint = false;
    prog.procedures.push_back(entry);
    prog.procedures.push_back(helper);

    EXPECT_NE(prog.FindProcedure("e1"), nullptr);
    EXPECT_EQ(prog.FindProcedure("missing"), nullptr);
    EXPECT_EQ(prog.EntryPoints().size(), 1u);
}

// ─── Lowerer ──────────────────────────────────────────────────────────────────

class BlockLowererTest : public ::testing::Test
{
protected:
    void SetUp() override { m_lib.RegisterAllBuiltins(); }
    BlockLibrary m_lib;
    BlockScript  m_script;
};

TEST_F(BlockLowererTest, EmptyScriptLowersToEmptyProgram)
{
    auto result = LowerBlockScript(m_script, m_lib);
    EXPECT_TRUE(result.Ok());
    EXPECT_EQ(result.program.procedures.size(), 0u);
}

TEST_F(BlockLowererTest, FlagMoveTurnSayLowersToOneEntryPoint)
{
    // The roadmap's first anchor stack:
    //   when flag clicked → move 10 steps → turn 15 degrees → say "hi"
    auto hat   = m_script.CreateInstance("saga.block.events.when_flag_clicked");
    auto move  = m_script.CreateInstance("saga.block.motion.move_steps");
    auto turn  = m_script.CreateInstance("saga.block.motion.turn_right");
    auto say   = m_script.CreateInstance("saga.block.looks.say");

    EXPECT_TRUE(m_script.SetSlotFill(move, "STEPS",   BlockSlotFill::MakeNumber(10.0)));
    EXPECT_TRUE(m_script.SetSlotFill(turn, "DEGREES", BlockSlotFill::MakeNumber(15.0)));
    EXPECT_TRUE(m_script.SetSlotFill(say,  "MESSAGE", BlockSlotFill::MakeText("hi")));

    auto stack = m_script.CreateStack(hat);
    EXPECT_TRUE(m_script.AppendToStack(stack, move));
    EXPECT_TRUE(m_script.AppendToStack(stack, turn));
    EXPECT_TRUE(m_script.AppendToStack(stack, say));
    m_script.AddRootStack(stack);

    auto result = LowerBlockScript(m_script, m_lib);
    EXPECT_TRUE(result.Ok());
    ASSERT_EQ(result.program.procedures.size(), 1u);

    const auto& proc = result.program.procedures[0];
    EXPECT_TRUE(proc.isEntryPoint);
    EXPECT_EQ(proc.eventId,   "saga.event.flag");
    ASSERT_EQ(proc.body.size(), 3u);

    EXPECT_EQ(proc.body[0]->tag,    IRStatement::Tag::Call);
    EXPECT_EQ(proc.body[0]->opcode, "saga.block.motion.move_steps");
    ASSERT_EQ(proc.body[0]->arguments.size(), 1u);
    EXPECT_EQ(proc.body[0]->arguments[0]->literalNumber, 10.0);

    EXPECT_EQ(proc.body[1]->opcode, "saga.block.motion.turn_right");
    EXPECT_EQ(proc.body[1]->arguments[0]->literalNumber, 15.0);

    EXPECT_EQ(proc.body[2]->opcode, "saga.block.looks.say");
    EXPECT_EQ(proc.body[2]->arguments[0]->literalText,   "hi");
}

TEST_F(BlockLowererTest, RepeatLowersToRepeatStatement)
{
    auto hat    = m_script.CreateInstance("saga.block.events.when_flag_clicked");
    auto repeat = m_script.CreateInstance("saga.block.control.repeat");
    auto move   = m_script.CreateInstance("saga.block.motion.move_steps");

    EXPECT_TRUE(m_script.SetSlotFill(repeat, "TIMES", BlockSlotFill::MakeNumber(7.0)));
    auto body = m_script.CreateStack(move);
    EXPECT_TRUE(m_script.AttachMouth(repeat, 0, body));

    auto stack = m_script.CreateStack(hat);
    EXPECT_TRUE(m_script.AppendToStack(stack, repeat));
    m_script.AddRootStack(stack);

    auto result = LowerBlockScript(m_script, m_lib);
    EXPECT_TRUE(result.Ok());
    ASSERT_EQ(result.program.procedures.size(), 1u);
    const auto& proc = result.program.procedures[0];
    ASSERT_EQ(proc.body.size(), 1u);
    EXPECT_EQ(proc.body[0]->tag, IRStatement::Tag::Repeat);
    ASSERT_NE(proc.body[0]->repeatCount, nullptr);
    EXPECT_EQ(proc.body[0]->repeatCount->literalNumber, 7.0);
    ASSERT_EQ(proc.body[0]->thenBody.size(), 1u);
    EXPECT_EQ(proc.body[0]->thenBody[0]->opcode,
              "saga.block.motion.move_steps");
}

TEST_F(BlockLowererTest, IfWithBooleanReporterLowersAsBranch)
{
    auto hat    = m_script.CreateInstance("saga.block.events.when_flag_clicked");
    auto ifBlk  = m_script.CreateInstance("saga.block.control.if");
    auto move   = m_script.CreateInstance("saga.block.motion.move_steps");
    auto reach  = m_script.CreateInstance("saga.block.sensing.touching_edge");

    EXPECT_TRUE(m_script.SetSlotFill(ifBlk, "CONDITION",
                                     BlockSlotFill::MakeExpression(reach.value)));
    auto body = m_script.CreateStack(move);
    EXPECT_TRUE(m_script.AttachMouth(ifBlk, 0, body));

    auto stack = m_script.CreateStack(hat);
    EXPECT_TRUE(m_script.AppendToStack(stack, ifBlk));
    m_script.AddRootStack(stack);

    auto result = LowerBlockScript(m_script, m_lib);
    EXPECT_TRUE(result.Ok());
    const auto& proc = result.program.procedures[0];
    ASSERT_EQ(proc.body.size(), 1u);
    EXPECT_EQ(proc.body[0]->tag, IRStatement::Tag::If);
    ASSERT_NE(proc.body[0]->condition, nullptr);
    EXPECT_EQ(proc.body[0]->condition->tag,    IRValue::Tag::Call);
    EXPECT_EQ(proc.body[0]->condition->opcode, "saga.block.sensing.touching_edge");
    EXPECT_EQ(proc.body[0]->condition->type,   IRType::Boolean);
}

TEST_F(BlockLowererTest, CapBlockLowersToStop)
{
    auto hat = m_script.CreateInstance("saga.block.events.when_flag_clicked");
    auto cap = m_script.CreateInstance("saga.block.control.stop_all");
    auto stack = m_script.CreateStack(hat);
    EXPECT_TRUE(m_script.AppendToStack(stack, cap));
    m_script.AddRootStack(stack);

    auto result = LowerBlockScript(m_script, m_lib);
    EXPECT_TRUE(result.Ok());
    const auto& proc = result.program.procedures[0];
    ASSERT_EQ(proc.body.size(), 1u);
    EXPECT_EQ(proc.body[0]->tag, IRStatement::Tag::Stop);
}

TEST_F(BlockLowererTest, UnknownOpcodeProducesError)
{
    auto bogus = m_script.CreateInstance("saga.block.does_not_exist");
    auto stack = m_script.CreateStack(bogus);
    m_script.AddRootStack(stack);

    auto result = LowerBlockScript(m_script, m_lib);
    EXPECT_FALSE(result.Ok());
    bool sawError = false;
    for (const auto& d : result.diagnostics)
    {
        if (d.severity == LowerDiagnostic::Severity::Error &&
            d.message.find("Unknown opcode") != std::string::npos)
        {
            sawError = true; break;
        }
    }
    EXPECT_TRUE(sawError);
}

TEST_F(BlockLowererTest, EmptySlotFallsBackToDefault)
{
    auto hat   = m_script.CreateInstance("saga.block.events.when_flag_clicked");
    auto move  = m_script.CreateInstance("saga.block.motion.move_steps");
    // STEPS slot deliberately not set — definition default is 10.0.

    auto stack = m_script.CreateStack(hat);
    EXPECT_TRUE(m_script.AppendToStack(stack, move));
    m_script.AddRootStack(stack);

    auto result = LowerBlockScript(m_script, m_lib);
    EXPECT_TRUE(result.Ok());
    const auto& proc = result.program.procedures[0];
    ASSERT_EQ(proc.body.size(), 1u);
    ASSERT_EQ(proc.body[0]->arguments.size(), 1u);
    EXPECT_EQ(proc.body[0]->arguments[0]->literalNumber, 10.0);
}

} // namespace
