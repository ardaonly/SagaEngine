/// @file BlockAuthoringTests.cpp
/// @brief GoogleTest coverage for the Scratch-style block authoring model.

#include "SagaEditor/VisualScripting/Blocks/BlockCategory.h"
#include "SagaEditor/VisualScripting/Blocks/BlockDefinition.h"
#include "SagaEditor/VisualScripting/Blocks/BlockInstance.h"
#include "SagaEditor/VisualScripting/Blocks/BlockKind.h"
#include "SagaEditor/VisualScripting/Blocks/BlockLibrary.h"
#include "SagaEditor/VisualScripting/Blocks/BlockScript.h"
#include "SagaEditor/VisualScripting/Blocks/BlockSlot.h"
#include "SagaEditor/VisualScripting/Blocks/BlockStack.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace
{

using namespace SagaEditor::VisualScripting;

// ─── BlockKind ────────────────────────────────────────────────────────────────

TEST(BlockKindTest, IdRoundTripsAndDefaultsToStack)
{
    const BlockKind all[] = {
        BlockKind::Hat, BlockKind::Stack, BlockKind::CBlock,
        BlockKind::EBlock, BlockKind::Cap, BlockKind::Reporter,
        BlockKind::Boolean,
    };
    for (auto k : all)
    {
        EXPECT_EQ(BlockKindFromId(BlockKindId(k)), k);
    }
    EXPECT_EQ(BlockKindFromId("garbage"), BlockKind::Stack);
}

TEST(BlockKindTest, ConnectionPredicates)
{
    // Top-notch / bottom-notch booleans match Scratch's snap rules.
    EXPECT_FALSE(BlockKindHasTopNotch(BlockKind::Hat));
    EXPECT_TRUE (BlockKindHasBottomNotch(BlockKind::Hat));

    EXPECT_TRUE (BlockKindHasTopNotch(BlockKind::Stack));
    EXPECT_TRUE (BlockKindHasBottomNotch(BlockKind::Stack));

    EXPECT_TRUE (BlockKindHasTopNotch(BlockKind::Cap));
    EXPECT_FALSE(BlockKindHasBottomNotch(BlockKind::Cap));

    EXPECT_FALSE(BlockKindHasTopNotch(BlockKind::Reporter));
    EXPECT_FALSE(BlockKindHasBottomNotch(BlockKind::Reporter));
    EXPECT_TRUE (BlockKindIsExpression(BlockKind::Reporter));
    EXPECT_TRUE (BlockKindIsExpression(BlockKind::Boolean));
    EXPECT_FALSE(BlockKindIsExpression(BlockKind::Stack));
}

TEST(BlockKindTest, MouthCounts)
{
    EXPECT_EQ(BlockKindMouthCount(BlockKind::CBlock), 1);
    EXPECT_EQ(BlockKindMouthCount(BlockKind::EBlock), 2);
    EXPECT_EQ(BlockKindMouthCount(BlockKind::Stack),  0);
    EXPECT_EQ(BlockKindMouthCount(BlockKind::Hat),    0);
}

// ─── BlockCategory ────────────────────────────────────────────────────────────

TEST(BlockCategoryTest, BuiltinsHaveDistinctIdsAndOrderedSortOrder)
{
    auto cats = MakeBuiltinCategories();
    EXPECT_EQ(cats.size(), 9u);

    for (std::size_t i = 0; i < cats.size(); ++i)
    {
        EXPECT_FALSE(cats[i].id.empty());
        EXPECT_FALSE(cats[i].displayName.empty());
        for (std::size_t j = 0; j < i; ++j)
        {
            EXPECT_NE(cats[i].id, cats[j].id);
            EXPECT_NE(cats[i].displayName, cats[j].displayName);
        }
    }
    // SortOrder is monotonically increasing in the canonical list.
    for (std::size_t i = 1; i < cats.size(); ++i)
    {
        EXPECT_LT(cats[i - 1].sortOrder, cats[i].sortOrder);
    }
    // Events comes after Sound (matches Scratch's palette order).
    auto findId = [&](const std::string& id) -> int
    {
        for (std::size_t i = 0; i < cats.size(); ++i)
            if (cats[i].id == id) return static_cast<int>(i);
        return -1;
    };
    EXPECT_GT(findId("saga.cat.events"), findId("saga.cat.sound"));
    EXPECT_GT(findId("saga.cat.myblocks"), findId("saga.cat.variables"));
}

// ─── BlockSlot ────────────────────────────────────────────────────────────────

TEST(BlockSlotTest, KindIdRoundTrip)
{
    const BlockSlotKind all[] = {
        BlockSlotKind::Number, BlockSlotKind::Text, BlockSlotKind::Boolean,
        BlockSlotKind::Dropdown, BlockSlotKind::Colour, BlockSlotKind::Variable,
        BlockSlotKind::Procedure, BlockSlotKind::Statement,
    };
    for (auto k : all)
    {
        EXPECT_EQ(BlockSlotKindFromId(BlockSlotKindId(k)), k);
    }
    EXPECT_EQ(BlockSlotKindFromId("nope"), BlockSlotKind::Number);
}

TEST(BlockSlotTest, AcceptsExpressionPredicate)
{
    EXPECT_TRUE (BlockSlotAcceptsExpression(BlockSlotKind::Number));
    EXPECT_TRUE (BlockSlotAcceptsExpression(BlockSlotKind::Boolean));
    EXPECT_TRUE (BlockSlotAcceptsExpression(BlockSlotKind::Variable));
    EXPECT_FALSE(BlockSlotAcceptsExpression(BlockSlotKind::Dropdown));
    EXPECT_FALSE(BlockSlotAcceptsExpression(BlockSlotKind::Colour));
    EXPECT_FALSE(BlockSlotAcceptsExpression(BlockSlotKind::Statement));
}

TEST(BlockSlotTest, FillBuildersTagDiscriminator)
{
    EXPECT_EQ(BlockSlotFill::MakeEmpty().kind,    BlockSlotFill::Kind::Empty);
    EXPECT_EQ(BlockSlotFill::MakeNumber(1).kind,  BlockSlotFill::Kind::Number);
    EXPECT_EQ(BlockSlotFill::MakeText("hi").kind, BlockSlotFill::Kind::Text);
    EXPECT_EQ(BlockSlotFill::MakeBool(true).kind, BlockSlotFill::Kind::Boolean);
    EXPECT_EQ(BlockSlotFill::MakeColor(0xFFu).kind,            BlockSlotFill::Kind::Color);
    EXPECT_EQ(BlockSlotFill::MakeDropdown("x").kind,           BlockSlotFill::Kind::Dropdown);
    EXPECT_EQ(BlockSlotFill::MakeVariable("score").kind,       BlockSlotFill::Kind::VariableRef);
    EXPECT_EQ(BlockSlotFill::MakeProcedure("greet").kind,      BlockSlotFill::Kind::ProcedureRef);
    EXPECT_EQ(BlockSlotFill::MakeExpression(42).kind,          BlockSlotFill::Kind::Expression);

    EXPECT_EQ(BlockSlotFill::MakeNumber(3.14).numberValue, 3.14);
    EXPECT_EQ(BlockSlotFill::MakeText("a"   ).textValue,   "a");
    EXPECT_EQ(BlockSlotFill::MakeBool(true  ).boolValue,   true);
    EXPECT_EQ(BlockSlotFill::MakeColor(0xAAu).colorValue,  0xAAu);
    EXPECT_EQ(BlockSlotFill::MakeDropdown("o").refId,       "o");
    EXPECT_EQ(BlockSlotFill::MakeExpression(5).expressionId, 5u);
}

TEST(BlockSlotTest, FillEqualityRespectsKind)
{
    auto a = BlockSlotFill::MakeNumber(5.0);
    auto b = BlockSlotFill::MakeNumber(5.0);
    auto c = BlockSlotFill::MakeNumber(6.0);
    auto d = BlockSlotFill::MakeText("5"); // same scalar, different kind
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

// ─── BlockLibrary ─────────────────────────────────────────────────────────────

class BlockLibraryTest : public ::testing::Test
{
protected:
    void SetUp() override { m_lib.RegisterAllBuiltins(); }
    BlockLibrary m_lib;
};

TEST_F(BlockLibraryTest, BuiltinRegistrationLoadsCategoriesAndDefinitions)
{
    EXPECT_EQ(m_lib.CategoryCount(), 9u);
    EXPECT_GE(m_lib.DefinitionCount(), 9u);
    EXPECT_TRUE(m_lib.HasCategory("saga.cat.motion"));
    EXPECT_TRUE(m_lib.HasCategory("saga.cat.myblocks"));
    EXPECT_TRUE(m_lib.HasDefinition("saga.block.motion.move_steps"));
    EXPECT_TRUE(m_lib.HasDefinition("saga.block.events.when_flag_clicked"));
    EXPECT_TRUE(m_lib.HasDefinition("saga.block.control.repeat"));
    EXPECT_TRUE(m_lib.HasDefinition("saga.block.control.if"));
}

TEST_F(BlockLibraryTest, MoveStepsHasNumericSlot)
{
    const auto* def = m_lib.FindDefinition("saga.block.motion.move_steps");
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->kind, BlockKind::Stack);
    EXPECT_EQ(def->categoryId, "saga.cat.motion");
    ASSERT_EQ(def->slots.size(), 1u);
    EXPECT_EQ(def->slots[0].kind, BlockSlotKind::Number);
    EXPECT_EQ(def->slots[0].defaultNumber, 10.0);
}

TEST_F(BlockLibraryTest, RepeatIsCBlockWithOneMouth)
{
    const auto* def = m_lib.FindDefinition("saga.block.control.repeat");
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->kind, BlockKind::CBlock);
    EXPECT_EQ(def->mouthLabels.size(), 1u);
    EXPECT_EQ(BlockKindMouthCount(def->kind), 1);
}

TEST_F(BlockLibraryTest, WhenFlagClickedIsHatWithEventId)
{
    const auto* def = m_lib.FindDefinition("saga.block.events.when_flag_clicked");
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->kind, BlockKind::Hat);
    EXPECT_EQ(def->hatEventId, "saga.event.flag");
}

TEST_F(BlockLibraryTest, GetDefinitionsForCategoryFiltersAndSorts)
{
    const auto motion = m_lib.GetDefinitionsForCategory("saga.cat.motion");
    EXPECT_GE(motion.size(), 2u);
    for (const auto& d : motion)
    {
        EXPECT_EQ(d.categoryId, "saga.cat.motion");
    }
    // Sorted ascending by displayName.
    for (std::size_t i = 1; i < motion.size(); ++i)
    {
        EXPECT_LE(motion[i - 1].displayName, motion[i].displayName);
    }
}

TEST_F(BlockLibraryTest, RegisterDefinitionRejectsUnknownCategory)
{
    BlockDefinition d;
    d.opcode     = "saga.block.test.bad";
    d.categoryId = "saga.cat.does_not_exist";
    EXPECT_FALSE(m_lib.RegisterDefinition(d));
}

TEST_F(BlockLibraryTest, RegisterDefinitionAllowsEmptyCategoryAndOpcode)
{
    BlockDefinition d;
    d.opcode = "";
    EXPECT_FALSE(m_lib.RegisterDefinition(d)); // empty opcode rejected
    BlockDefinition e;
    e.opcode = "saga.block.test.bare";
    e.categoryId = ""; // empty category is allowed (uncategorised tools)
    EXPECT_TRUE(m_lib.RegisterDefinition(e));
}

// ─── BlockScript ──────────────────────────────────────────────────────────────

class BlockScriptTest : public ::testing::Test
{
protected:
    void SetUp() override { m_lib.RegisterAllBuiltins(); }
    BlockLibrary m_lib;
    BlockScript  m_script;
};

TEST_F(BlockScriptTest, NewScriptIsEmpty)
{
    EXPECT_EQ(m_script.InstanceCount(), 0u);
    EXPECT_EQ(m_script.StackCount(),    0u);
    EXPECT_EQ(m_script.GetRootStacks().size(), 0u);
}

TEST_F(BlockScriptTest, HappyPathFlagMoveTurnSay)
{
    // The first anchor example from the roadmap:
    // when flag clicked → move 10 steps → turn 15 degrees → say "hi".
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

    EXPECT_EQ(m_script.InstanceCount(), 4u);
    EXPECT_EQ(m_script.StackCount(),    1u);
    EXPECT_EQ(m_script.GetRootStacks().size(), 1u);

    // No validation issues — every block is well-formed and snap rules hold.
    auto issues = m_script.Validate(m_lib);
    EXPECT_EQ(issues.size(), 0u);
}

TEST_F(BlockScriptTest, AddRootStackIsIdempotent)
{
    auto hat   = m_script.CreateInstance("saga.block.events.when_flag_clicked");
    auto stack = m_script.CreateStack(hat);
    m_script.AddRootStack(stack);
    m_script.AddRootStack(stack);
    EXPECT_EQ(m_script.GetRootStacks().size(), 1u);
}

TEST_F(BlockScriptTest, RemoveRootStackDropsEntry)
{
    auto hat   = m_script.CreateInstance("saga.block.events.when_flag_clicked");
    auto stack = m_script.CreateStack(hat);
    m_script.AddRootStack(stack);
    m_script.RemoveRootStack(stack);
    EXPECT_EQ(m_script.GetRootStacks().size(), 0u);
}

TEST_F(BlockScriptTest, AttachMouthGrowsParentMouthList)
{
    auto repeat = m_script.CreateInstance("saga.block.control.repeat");
    auto move   = m_script.CreateInstance("saga.block.motion.move_steps");
    auto body   = m_script.CreateStack(move);

    EXPECT_TRUE(m_script.AttachMouth(repeat, 0, body));

    const auto* parent = m_script.FindInstance(repeat);
    ASSERT_NE(parent, nullptr);
    EXPECT_EQ(parent->mouthStacks.size(), 1u);
    EXPECT_EQ(parent->mouthStacks[0],    body);
}

TEST_F(BlockScriptTest, ValidateFlagsHatNotAtTop)
{
    auto move = m_script.CreateInstance("saga.block.motion.move_steps");
    auto hat  = m_script.CreateInstance("saga.block.events.when_flag_clicked");
    auto stack = m_script.CreateStack(move);
    EXPECT_TRUE(m_script.AppendToStack(stack, hat));
    m_script.AddRootStack(stack);

    auto issues = m_script.Validate(m_lib);
    bool foundHatWarning = false;
    for (const auto& is : issues)
    {
        if (is.message.find("Hat block") != std::string::npos)
        {
            foundHatWarning = true;
            break;
        }
    }
    EXPECT_TRUE(foundHatWarning);
}

TEST_F(BlockScriptTest, ValidateFlagsCapNotAtBottom)
{
    auto cap  = m_script.CreateInstance("saga.block.control.stop_all");
    auto move = m_script.CreateInstance("saga.block.motion.move_steps");
    auto stack = m_script.CreateStack(cap);
    EXPECT_TRUE(m_script.AppendToStack(stack, move));
    m_script.AddRootStack(stack);

    auto issues = m_script.Validate(m_lib);
    bool foundCapError = false;
    for (const auto& is : issues)
    {
        if (is.message.find("Cap block") != std::string::npos)
        {
            foundCapError = true;
            break;
        }
    }
    EXPECT_TRUE(foundCapError);
}

TEST_F(BlockScriptTest, ValidateFlagsUnknownOpcode)
{
    auto bad  = m_script.CreateInstance("saga.block.does_not_exist");
    auto stack = m_script.CreateStack(bad);
    m_script.AddRootStack(stack);

    auto issues = m_script.Validate(m_lib);
    bool foundOpcodeError = false;
    for (const auto& is : issues)
    {
        if (is.message.find("unknown opcode") != std::string::npos)
        {
            foundOpcodeError = true;
            break;
        }
    }
    EXPECT_TRUE(foundOpcodeError);
}

TEST_F(BlockScriptTest, ValidateFlagsExpressionAtStackHead)
{
    auto reporter = m_script.CreateInstance("saga.block.operators.add");
    auto stack    = m_script.CreateStack(reporter);
    m_script.AddRootStack(stack);

    auto issues = m_script.Validate(m_lib);
    bool foundExprError = false;
    for (const auto& is : issues)
    {
        if (is.message.find("expression") != std::string::npos)
        {
            foundExprError = true;
            break;
        }
    }
    EXPECT_TRUE(foundExprError);
}

TEST_F(BlockScriptTest, ValidateFlagsNonExpressionInExpressionSlot)
{
    auto move    = m_script.CreateInstance("saga.block.motion.move_steps");
    auto repeat  = m_script.CreateInstance("saga.block.control.repeat");
    EXPECT_TRUE(m_script.SetSlotFill(repeat, "TIMES",
        BlockSlotFill::MakeExpression(move.value)));
    auto stack = m_script.CreateStack(repeat);
    m_script.AddRootStack(stack);

    auto issues = m_script.Validate(m_lib);
    bool foundError = false;
    for (const auto& is : issues)
    {
        if (is.message.find("non-expression") != std::string::npos)
        {
            foundError = true;
            break;
        }
    }
    EXPECT_TRUE(foundError);
}

TEST_F(BlockScriptTest, MoveScriptIsConstructible)
{
    BlockScript a;
    auto inst = a.CreateInstance("saga.block.motion.move_steps");
    EXPECT_TRUE(inst.IsValid());

    BlockScript b = std::move(a);
    EXPECT_NE(b.FindInstance(inst), nullptr);
}

} // namespace
