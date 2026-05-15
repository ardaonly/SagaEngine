/// @file EnumValidationTests.cpp
/// @brief Tests for EnumRegistry-backed enum validation.

#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/EnumRegistry.h"
#include "SDE/Model/FieldDefinition.h"
#include "SDE/Model/ModelDefinition.h"
#include "SDE/Model/TypeNode.h"
#include "SDE/Validation/Validator.h"

#include <gtest/gtest.h>

using namespace SDE;

namespace
{

RuleRegistry MakeRules()
{
    RuleRegistry rules;
    rules.Freeze();
    return rules;
}

} // namespace

TEST(EnumValidationTest, UnknownEnumMemberProducesStructuredDiagnostic)
{
    TypeRegistry types;
    TypeNodeId rarityType = types.Enum("Rarity");
    types.Freeze();

    EnumDefinition rarity;
    rarity.id = "Rarity";
    rarity.members.push_back({"Common", {}});
    rarity.members.push_back({"Rare", {}});

    EnumRegistry enums;
    enums.Register(rarity);
    enums.Freeze();

    FieldDefinition rarityField;
    rarityField.id = "rarity";
    rarityField.type = rarityType;

    ModelDefinition def;
    def.id = "Item";
    def.fields.push_back(rarityField);

    ModelInstance inst;
    inst.modelId = "Item";
    inst.instanceId = "sword_01";
    RawValue rarityValue;
    rarityValue.location = {"item.json", 4, 16};
    rarityValue.data = RawText{"Legendary"};
    inst.fields["rarity"] = rarityValue;

    RuleRegistry rules = MakeRules();
    Validator validator(rules, types, &enums);
    auto result = validator.Validate({inst}, {def});

    ASSERT_TRUE(result.HasErrors());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "SDE_ENUM_MEMBER");
    EXPECT_EQ(result.diagnostics[0].category, DiagnosticCategory::Rule);
    EXPECT_EQ(result.diagnostics[0].metadata.at("enumId"), "Rarity");
}
