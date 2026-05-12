/// @file ModelCompilerTests.cpp
/// @brief Tests for ModelCompiler: clean compile, dangling ref, graph content.

#include "SDE/Compilation/ModelCompiler.h"
#include "SDE/Compilation/CompiledModelGraph.h"
#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/FieldDefinition.h"
#include "SDE/Model/ModelDefinition.h"
#include "SDE/Model/TypeNode.h"
#include "SDE/Validation/Validator.h"

#include <gtest/gtest.h>

using namespace SDE;

namespace
{

RuleRegistry MakeEmptyRegistry()
{
    RuleRegistry reg;
    reg.Freeze();
    return reg;
}

ModelInstance MakeTextInstance(const std::string& modelId,
                                const std::string& instanceId,
                                const std::string& fieldId,
                                const std::string& text)
{
    ModelInstance inst;
    inst.modelId    = modelId;
    inst.instanceId = instanceId;
    RawValue rv;
    rv.data = RawText{text};
    inst.fields[fieldId] = rv;
    return inst;
}

} // namespace

// ─── Empty batch ──────────────────────────────────────────────────────────────

TEST(ModelCompilerTest, EmptyInstances_ReturnsCleanWithGraph)
{
    TypeRegistry  types;
    types.Freeze();
    RuleRegistry  rules = MakeEmptyRegistry();

    ModelCompiler compiler(rules, types);
    auto result = compiler.Compile({}, {});

    EXPECT_EQ(result.state, CompileState::Clean);
    ASSERT_TRUE(result.graph.has_value());
    EXPECT_EQ(result.graph->TotalCount(), 0u);
}

// ─── Valid scalar instance ────────────────────────────────────────────────────

TEST(ModelCompilerTest, ValidTextInstance_ReturnsCleanWithPopulatedGraph)
{
    TypeRegistry types;
    TypeNodeId   textType = types.Primitive(TypeKind::Text);
    types.Freeze();

    FieldDefinition nameFd;
    nameFd.id       = "name";
    nameFd.type     = textType;
    nameFd.presence = FieldPresence::Required;

    ModelDefinition def;
    def.id = "Item";
    def.fields.push_back(nameFd);

    auto inst = MakeTextInstance("Item", "sword_01", "name", "Sword");

    RuleRegistry rules = MakeEmptyRegistry();
    ModelCompiler compiler(rules, types);
    auto result = compiler.Compile({inst}, {def});

    EXPECT_EQ(result.state, CompileState::Clean);
    ASSERT_TRUE(result.graph.has_value());
    EXPECT_EQ(result.graph->TotalCount(), 1u);

    const CompiledInstance* ci = result.graph->Find("Item", "sword_01");
    ASSERT_NE(ci, nullptr);
    EXPECT_EQ(ci->modelId,    "Item");
    EXPECT_EQ(ci->instanceId, "sword_01");

    const CompiledValue* cv = ci->GetField("name");
    ASSERT_NE(cv, nullptr);
    ASSERT_TRUE(std::holds_alternative<CompiledText>(cv->data));
    EXPECT_EQ(std::get<CompiledText>(cv->data), "Sword");
}

TEST(ModelCompilerTest, ValidIntegerInstance_GraphContainsCompiledInteger)
{
    TypeRegistry types;
    TypeNodeId   intType = types.Primitive(TypeKind::Integer);
    types.Freeze();

    FieldDefinition levelFd;
    levelFd.id       = "level";
    levelFd.type     = intType;
    levelFd.presence = FieldPresence::Required;

    ModelDefinition def;
    def.id = "Item";
    def.fields.push_back(levelFd);

    ModelInstance inst;
    inst.modelId    = "Item";
    inst.instanceId = "sword_01";
    RawValue rv;
    rv.data = static_cast<RawInteger>(10);
    inst.fields["level"] = rv;

    RuleRegistry rules = MakeEmptyRegistry();
    ModelCompiler compiler(rules, types);
    auto result = compiler.Compile({inst}, {def});

    EXPECT_EQ(result.state, CompileState::Clean);
    ASSERT_TRUE(result.graph.has_value());

    const CompiledInstance* ci = result.graph->Find("Item", "sword_01");
    ASSERT_NE(ci, nullptr);
    const CompiledValue* cv = ci->GetField("level");
    ASSERT_NE(cv, nullptr);
    ASSERT_TRUE(std::holds_alternative<CompiledInteger>(cv->data));
    EXPECT_EQ(std::get<CompiledInteger>(cv->data), 10);
}

// ─── AllOf and ModelIds ────────────────────────────────────────────────────────

TEST(ModelCompilerTest, AllOf_ReturnsOnlyInstancesOfRequestedModel)
{
    TypeRegistry types;
    TypeNodeId   textType = types.Primitive(TypeKind::Text);
    types.Freeze();

    FieldDefinition nameFd;
    nameFd.id       = "name";
    nameFd.type     = textType;
    nameFd.presence = FieldPresence::Required;

    ModelDefinition itemDef;
    itemDef.id = "Item";
    itemDef.fields.push_back(nameFd);

    ModelDefinition skillDef;
    skillDef.id = "Skill";
    skillDef.fields.push_back(nameFd);

    std::vector<ModelInstance> instances = {
        MakeTextInstance("Item",  "sword_01", "name", "Sword"),
        MakeTextInstance("Item",  "shield_01", "name", "Shield"),
        MakeTextInstance("Skill", "slash", "name", "Slash"),
    };

    RuleRegistry  rules = MakeEmptyRegistry();
    ModelCompiler compiler(rules, types);
    auto result = compiler.Compile(instances, {itemDef, skillDef});

    ASSERT_TRUE(result.graph.has_value());
    EXPECT_EQ(result.graph->TotalCount(), 3u);

    auto items = result.graph->AllOf("Item");
    EXPECT_EQ(items.size(), 2u);

    auto skills = result.graph->AllOf("Skill");
    EXPECT_EQ(skills.size(), 1u);
}

// ─── Dangling reference ───────────────────────────────────────────────────────

TEST(ModelCompilerTest, DanglingRef_ReturnsUnresolvedRefsAndNulloptGraph)
{
    TypeRegistry types;
    TypeNodeId   refType = types.Ref("Item");
    types.Freeze();

    FieldDefinition ownerFd;
    ownerFd.id       = "owner";
    ownerFd.type     = refType;
    ownerFd.presence = FieldPresence::Required;

    ModelDefinition skillDef;
    skillDef.id = "Skill";
    skillDef.fields.push_back(ownerFd);

    // owner references an Item that does not exist.
    ModelInstance skill;
    skill.modelId    = "Skill";
    skill.instanceId = "slash";
    RawValue ownerVal;
    ownerVal.data = RawText{"nonexistent"};
    skill.fields["owner"] = ownerVal;

    RuleRegistry  rules = MakeEmptyRegistry();
    ModelCompiler compiler(rules, types);
    auto result = compiler.Compile({skill}, {skillDef});

    EXPECT_EQ(result.state, CompileState::UnresolvedRefs);
    EXPECT_FALSE(result.graph.has_value());

    bool hasDanglingDiag = false;
    for (const auto& d : result.validation.diagnostics)
        if (d.code == "SDE_DANGLING_REF") { hasDanglingDiag = true; break; }
    EXPECT_TRUE(hasDanglingDiag);
}

// ─── Default inference ────────────────────────────────────────────────────────

TEST(ModelCompilerTest, DefaultInference_FillsMissingOptionalField)
{
    TypeRegistry types;
    TypeNodeId   textType = types.Primitive(TypeKind::Text);
    types.Freeze();

    FieldDefinition rarityFd;
    rarityFd.id         = "rarity";
    rarityFd.type       = textType;
    rarityFd.presence   = FieldPresence::Optional;
    rarityFd.defaultVal = "Common";

    FieldDefinition nameFd;
    nameFd.id       = "name";
    nameFd.type     = textType;
    nameFd.presence = FieldPresence::Required;

    ModelDefinition def;
    def.id = "Item";
    def.fields.push_back(nameFd);
    def.fields.push_back(rarityFd);

    // Instance does NOT include rarity — should be inferred from the default.
    auto inst = MakeTextInstance("Item", "sword_01", "name", "Sword");

    RuleRegistry  rules = MakeEmptyRegistry();
    ModelCompiler compiler(rules, types);
    auto result = compiler.Compile({inst}, {def});

    ASSERT_TRUE(result.graph.has_value());
    const CompiledInstance* ci = result.graph->Find("Item", "sword_01");
    ASSERT_NE(ci, nullptr);

    const CompiledValue* cv = ci->GetField("rarity");
    ASSERT_NE(cv, nullptr);
    ASSERT_TRUE(std::holds_alternative<CompiledText>(cv->data));
    EXPECT_EQ(std::get<CompiledText>(cv->data), "Common");
}
