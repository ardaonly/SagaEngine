/// @file RangeRuleTests.cpp
/// @brief Tests for RangeRule: in-range, below-min, above-max.

#include "SDE/Validation/Rule.h"
#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/FieldDefinition.h"

#include <gtest/gtest.h>

using namespace SDE;

namespace
{

FieldDefinition MakeField(const std::string& id)
{
    FieldDefinition fd;
    fd.id = id;
    return fd;
}

RawValue NumberValue(double v)
{
    RawValue rv;
    rv.data = static_cast<RawNumber>(v);
    return rv;
}

RawValue IntegerValue(int64_t v)
{
    RawValue rv;
    rv.data = static_cast<RawInteger>(v);
    return rv;
}

} // namespace

TEST(RangeRuleTest, RuleIdIsCorrect)
{
    RangeRule rule(0.0, 100.0);
    EXPECT_EQ(rule.RuleId(), "SDE_RANGE");
}

TEST(RangeRuleTest, PassForValueInRange)
{
    RangeRule rule(0.0, 100.0);
    FieldDefinition fd  = MakeField("damage");
    RawValue        val = NumberValue(50.0);
    std::vector<Diagnostic> diags;
    EXPECT_TRUE(rule.Apply(val, fd, {}, diags));
    EXPECT_TRUE(diags.empty());
}

TEST(RangeRuleTest, PassForBoundaryValues)
{
    RangeRule rule(0.0, 100.0);
    FieldDefinition fd = MakeField("x");
    std::vector<Diagnostic> diags;
    EXPECT_TRUE(rule.Apply(NumberValue(0.0),   fd, {}, diags));
    EXPECT_TRUE(rule.Apply(NumberValue(100.0), fd, {}, diags));
    EXPECT_TRUE(diags.empty());
}

TEST(RangeRuleTest, FailForValueBelowMin)
{
    RangeRule rule(0.0, 100.0);
    FieldDefinition fd  = MakeField("x");
    RawValue        val = NumberValue(-1.0);
    std::vector<Diagnostic> diags;
    EXPECT_FALSE(rule.Apply(val, fd, {}, diags));
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_EQ(diags[0].code, "SDE_RANGE");
    EXPECT_EQ(diags[0].severity, Severity::Error);
}

TEST(RangeRuleTest, FailForValueAboveMax)
{
    RangeRule rule(0.0, 100.0);
    FieldDefinition fd  = MakeField("x");
    RawValue        val = NumberValue(101.0);
    std::vector<Diagnostic> diags;
    EXPECT_FALSE(rule.Apply(val, fd, {}, diags));
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_EQ(diags[0].code, "SDE_RANGE");
}

TEST(RangeRuleTest, WorksWithIntegerRawValue)
{
    RangeRule rule(0.0, 10.0);
    FieldDefinition fd  = MakeField("x");
    std::vector<Diagnostic> diags;
    EXPECT_TRUE(rule.Apply(IntegerValue(5), fd, {}, diags));
    EXPECT_TRUE(diags.empty());
}

TEST(RangeRuleTest, FailForNonNumericValue)
{
    RangeRule rule(0.0, 100.0);
    FieldDefinition fd = MakeField("x");
    RawValue text;
    text.data = RawText{"hello"};
    std::vector<Diagnostic> diags;
    EXPECT_FALSE(rule.Apply(text, fd, {}, diags));
    EXPECT_EQ(diags.size(), 1u);
}
