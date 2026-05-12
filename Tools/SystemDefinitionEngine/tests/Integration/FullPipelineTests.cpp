/// @file FullPipelineTests.cpp
/// @brief End-to-end tests: JSON load → ModelCompiler → CompiledModelGraph.

#include "SDE/Compilation/ModelCompiler.h"
#include "SDE/Compilation/CompiledModelGraph.h"
#include "SDE/IO/JsonModelLoader.h"
#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/ModelDefinition.h"
#include "SDE/Model/TypeNode.h"
#include "SDE/Validation/Validator.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace SDE;
namespace fs = std::filesystem;

namespace
{

std::string WriteTempFile(const std::string& content, const std::string& name)
{
    fs::path p = fs::temp_directory_path() / name;
    std::ofstream out(p);
    out << content;
    return p.string();
}

RuleRegistry MakeEmptyRegistry()
{
    RuleRegistry reg;
    reg.Freeze();
    return reg;
}

} // namespace

// ─── Happy path ───────────────────────────────────────────────────────────────

TEST(FullPipelineTest, ValidJsonData_ProducesCleanGraphWithInstances)
{
    const std::string schemaJson = R"({
        "id": "Item",
        "displayName": "Game Item",
        "schemaVersion": 1,
        "fields": [
            { "id": "name",  "type": "Text",    "presence": "required" },
            { "id": "level", "type": "Integer",  "presence": "required" }
        ]
    })";

    const std::string dataJson = R"({
        "modelId": "Item",
        "data": [
            { "id": "sword_01", "name": "Sword", "level": 5  },
            { "id": "axe_01",   "name": "Axe",   "level": 8  }
        ]
    })";

    std::string schemaPath = WriteTempFile(schemaJson, "fp_item.schema.json");
    std::string dataPath   = WriteTempFile(dataJson,   "fp_items.json");

    // ─── Load schema ──────────────────────────────────────────────────────────
    TypeRegistry         types;
    std::vector<Diagnostic> schemaDiags;
    auto defOpt = JsonModelLoader::LoadDefinition(schemaPath, types, schemaDiags);
    types.Freeze();

    ASSERT_TRUE(defOpt.has_value());
    EXPECT_TRUE(schemaDiags.empty());

    // ─── Load data ────────────────────────────────────────────────────────────
    JsonModelLoader loader;
    std::vector<Diagnostic> loadDiags;
    auto instances = loader.Load(dataPath, loadDiags);

    ASSERT_EQ(instances.size(), 2u);
    EXPECT_TRUE(loadDiags.empty());

    // ─── Compile ──────────────────────────────────────────────────────────────
    RuleRegistry  rules = MakeEmptyRegistry();
    ModelCompiler compiler(rules, types);
    auto result = compiler.Compile(instances, {*defOpt});

    EXPECT_EQ(result.state, CompileState::Clean);
    ASSERT_TRUE(result.graph.has_value());
    EXPECT_EQ(result.graph->TotalCount(), 2u);

    const CompiledInstance* sword = result.graph->Find("Item", "sword_01");
    ASSERT_NE(sword, nullptr);
    const CompiledValue* level = sword->GetField("level");
    ASSERT_NE(level, nullptr);
    ASSERT_TRUE(std::holds_alternative<CompiledInteger>(level->data));
    EXPECT_EQ(std::get<CompiledInteger>(level->data), 5);
}

// ─── Reference resolution across two models ───────────────────────────────────

TEST(FullPipelineTest, CrossModelRef_ResolvesToCompiledInstanceRef)
{
    const std::string itemSchema = R"({
        "id": "Item",
        "displayName": "Game Item",
        "schemaVersion": 1,
        "fields": [
            { "id": "name", "type": "Text", "presence": "required" }
        ]
    })";

    const std::string skillSchema = R"({
        "id": "Skill",
        "displayName": "Skill",
        "schemaVersion": 1,
        "fields": [
            { "id": "name",  "type": "Text",       "presence": "required" },
            { "id": "weapon","type": "Ref<Item>",   "presence": "required" }
        ]
    })";

    const std::string itemData = R"({
        "modelId": "Item",
        "data": [ { "id": "sword_01", "name": "Sword" } ]
    })";

    const std::string skillData = R"({
        "modelId": "Skill",
        "data": [ { "id": "slash", "name": "Slash", "weapon": "sword_01" } ]
    })";

    TypeRegistry types;
    std::vector<Diagnostic> diags;

    auto itemDefOpt  = JsonModelLoader::LoadDefinition(
        WriteTempFile(itemSchema,  "fp_item2.schema.json"), types, diags);
    auto skillDefOpt = JsonModelLoader::LoadDefinition(
        WriteTempFile(skillSchema, "fp_skill2.schema.json"), types, diags);
    types.Freeze();

    ASSERT_TRUE(itemDefOpt.has_value());
    ASSERT_TRUE(skillDefOpt.has_value());

    JsonModelLoader loader;
    auto itemInstances  = loader.Load(WriteTempFile(itemData,  "fp_items2.json"),  diags);
    auto skillInstances = loader.Load(WriteTempFile(skillData, "fp_skills2.json"), diags);
    EXPECT_TRUE(diags.empty());

    std::vector<ModelInstance> all;
    all.insert(all.end(), itemInstances.begin(),  itemInstances.end());
    all.insert(all.end(), skillInstances.begin(), skillInstances.end());

    RuleRegistry  rules = MakeEmptyRegistry();
    ModelCompiler compiler(rules, types);
    auto result = compiler.Compile(all, {*itemDefOpt, *skillDefOpt});

    EXPECT_EQ(result.state, CompileState::Clean);
    ASSERT_TRUE(result.graph.has_value());

    const CompiledInstance* skill = result.graph->Find("Skill", "slash");
    ASSERT_NE(skill, nullptr);
    const CompiledValue* weapon = skill->GetField("weapon");
    ASSERT_NE(weapon, nullptr);
    ASSERT_TRUE(std::holds_alternative<CompiledInstanceRef>(weapon->data));
    const auto& ref = std::get<CompiledInstanceRef>(weapon->data);
    EXPECT_EQ(ref.modelId,    "Item");
    EXPECT_EQ(ref.instanceId, "sword_01");
}

// ─── Dangling reference ───────────────────────────────────────────────────────

TEST(FullPipelineTest, DanglingRef_ProducesUnresolvedRefsStateAndNulloptGraph)
{
    const std::string skillSchema = R"({
        "id": "Skill",
        "displayName": "Skill",
        "schemaVersion": 1,
        "fields": [
            { "id": "weapon", "type": "Ref<Item>", "presence": "required" }
        ]
    })";

    const std::string skillData = R"({
        "modelId": "Skill",
        "data": [ { "id": "slash", "weapon": "nonexistent" } ]
    })";

    TypeRegistry types;
    std::vector<Diagnostic> diags;
    auto skillDefOpt = JsonModelLoader::LoadDefinition(
        WriteTempFile(skillSchema, "fp_skill_dangling.schema.json"), types, diags);
    types.Freeze();

    ASSERT_TRUE(skillDefOpt.has_value());

    JsonModelLoader loader;
    auto instances = loader.Load(WriteTempFile(skillData, "fp_skills_dangling.json"), diags);
    ASSERT_EQ(instances.size(), 1u);

    RuleRegistry  rules = MakeEmptyRegistry();
    ModelCompiler compiler(rules, types);
    auto result = compiler.Compile(instances, {*skillDefOpt});

    EXPECT_EQ(result.state, CompileState::UnresolvedRefs);
    EXPECT_FALSE(result.graph.has_value());

    bool hasDanglingDiag = false;
    for (const auto& d : result.validation.diagnostics)
        if (d.code == "SDE_DANGLING_REF") { hasDanglingDiag = true; break; }
    EXPECT_TRUE(hasDanglingDiag);
}

// ─── Same id in two model namespaces ──────────────────────────────────────────

TEST(FullPipelineTest, SameInstanceIdInTwoModels_BothCompiledWithoutCollision)
{
    const std::string itemSchema = R"({
        "id": "Item",
        "displayName": "Item",
        "schemaVersion": 1,
        "fields": [ { "id": "name", "type": "Text", "presence": "required" } ]
    })";

    const std::string skillSchema = R"({
        "id": "Skill",
        "displayName": "Skill",
        "schemaVersion": 1,
        "fields": [ { "id": "name", "type": "Text", "presence": "required" } ]
    })";

    // Both use id "01".
    const std::string itemData  = R"({ "modelId": "Item",  "data": [ { "id": "01", "name": "Sword" } ] })";
    const std::string skillData = R"({ "modelId": "Skill", "data": [ { "id": "01", "name": "Slash" } ] })";

    TypeRegistry types;
    std::vector<Diagnostic> diags;
    auto itemDefOpt  = JsonModelLoader::LoadDefinition(
        WriteTempFile(itemSchema,  "fp_item_ns.schema.json"), types, diags);
    auto skillDefOpt = JsonModelLoader::LoadDefinition(
        WriteTempFile(skillSchema, "fp_skill_ns.schema.json"), types, diags);
    types.Freeze();

    ASSERT_TRUE(itemDefOpt.has_value());
    ASSERT_TRUE(skillDefOpt.has_value());

    JsonModelLoader loader;
    auto itemInstances  = loader.Load(WriteTempFile(itemData,  "fp_items_ns.json"),  diags);
    auto skillInstances = loader.Load(WriteTempFile(skillData, "fp_skills_ns.json"), diags);

    std::vector<ModelInstance> all;
    all.insert(all.end(), itemInstances.begin(),  itemInstances.end());
    all.insert(all.end(), skillInstances.begin(), skillInstances.end());

    RuleRegistry  rules = MakeEmptyRegistry();
    ModelCompiler compiler(rules, types);
    auto result = compiler.Compile(all, {*itemDefOpt, *skillDefOpt});

    EXPECT_EQ(result.state, CompileState::Clean);
    ASSERT_TRUE(result.graph.has_value());
    EXPECT_EQ(result.graph->TotalCount(), 2u);

    // Both instances are accessible without collision.
    EXPECT_NE(result.graph->Find("Item",  "01"), nullptr);
    EXPECT_NE(result.graph->Find("Skill", "01"), nullptr);
}
