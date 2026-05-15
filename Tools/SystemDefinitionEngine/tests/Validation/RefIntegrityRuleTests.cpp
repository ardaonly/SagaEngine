/// @file RefIntegrityRuleTests.cpp
/// @brief Tests for reference syntax validation before resolver lookup.

#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/FieldDefinition.h"
#include "SDE/Validation/Rule.h"

#include <gtest/gtest.h>

using namespace SDE;

TEST(RefIntegrityRuleTest, RejectsEmptyReferenceId)
{
    FieldDefinition field;
    field.id = "weapon";

    RawValue value;
    value.data = RawText{""};

    std::vector<Diagnostic> diagnostics;
    RefIntegrityRule rule;

    EXPECT_FALSE(rule.Apply(value, field, {"skill.json", 3, 21}, diagnostics));
    ASSERT_EQ(diagnostics.size(), 1u);
    EXPECT_EQ(diagnostics[0].code, "SDE_REF_INTEGRITY");
    EXPECT_EQ(diagnostics[0].category, DiagnosticCategory::Rule);
}

TEST(RefIntegrityRuleTest, AcceptsLocalReferenceId)
{
    FieldDefinition field;
    field.id = "weapon";

    RawValue value;
    value.data = RawText{"sword_01"};

    std::vector<Diagnostic> diagnostics;
    RefIntegrityRule rule;

    EXPECT_TRUE(rule.Apply(value, field, {}, diagnostics));
    EXPECT_TRUE(diagnostics.empty());
}
