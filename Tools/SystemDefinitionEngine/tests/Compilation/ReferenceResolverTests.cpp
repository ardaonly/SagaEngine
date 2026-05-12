/// @file ReferenceResolverTests.cpp
/// @brief Tests for ReferenceResolver: compound key, dangling refs, namespace isolation.

#include "SDE/Compilation/ReferenceResolver.h"
#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/ModelDefinition.h"
#include "SDE/Model/TypeNode.h"

#include <gtest/gtest.h>

using namespace SDE;

namespace
{

ModelInstance MakeInstance(const std::string& modelId, const std::string& instanceId)
{
    ModelInstance inst;
    inst.modelId    = modelId;
    inst.instanceId = instanceId;
    return inst;
}

ModelInstance MakeInstanceWithRef(const std::string& modelId,
                                   const std::string& instanceId,
                                   const std::string& fieldId,
                                   const std::string& targetId)
{
    ModelInstance inst = MakeInstance(modelId, instanceId);
    RawValue rv;
    rv.data = RawText{targetId};
    inst.fields[fieldId] = rv;
    return inst;
}

} // namespace

// ─── Valid resolution ─────────────────────────────────────────────────────────

TEST(ReferenceResolverTest, ValidRef_PopulatesResolvedHandles)
{
    TypeRegistry types;
    TypeNodeId   refItemType = types.Ref("Item");
    types.Freeze();

    FieldDefinition ownerFd;
    ownerFd.id   = "owner";
    ownerFd.type = refItemType;

    ModelDefinition skillDef;
    skillDef.id = "Skill";
    skillDef.fields.push_back(ownerFd);

    ModelDefinition itemDef;
    itemDef.id = "Item";

    auto item  = MakeInstance("Item", "sword_01");
    auto skill = MakeInstanceWithRef("Skill", "slash", "owner", "sword_01");

    std::vector<ModelInstance>   instances = {item, skill};
    std::vector<ModelDefinition> defs      = {itemDef, skillDef};

    ReferenceResolver resolver;
    resolver.BuildIndex(instances);
    auto result = resolver.Resolve(instances, defs, types);

    EXPECT_FALSE(result.hasDanglingRefs);
    EXPECT_FALSE(result.hasCycles);
    EXPECT_TRUE(result.diagnostics.empty());

    auto it = result.resolvedHandles.find("Skill/slash/owner");
    ASSERT_NE(it, result.resolvedHandles.end());
    EXPECT_EQ(it->second.modelId,    "Item");
    EXPECT_EQ(it->second.instanceId, "sword_01");
}

// ─── Dangling reference ───────────────────────────────────────────────────────

TEST(ReferenceResolverTest, DanglingRef_SetsFlagAndEmitsDiagnostic)
{
    TypeRegistry types;
    TypeNodeId   refItemType = types.Ref("Item");
    types.Freeze();

    FieldDefinition ownerFd;
    ownerFd.id   = "owner";
    ownerFd.type = refItemType;

    ModelDefinition skillDef;
    skillDef.id = "Skill";
    skillDef.fields.push_back(ownerFd);

    // No Item instance provided — ref is dangling.
    auto skill = MakeInstanceWithRef("Skill", "slash", "owner", "nonexistent_id");

    std::vector<ModelInstance>   instances = {skill};
    std::vector<ModelDefinition> defs      = {skillDef};

    ReferenceResolver resolver;
    resolver.BuildIndex(instances);
    auto result = resolver.Resolve(instances, defs, types);

    EXPECT_TRUE(result.hasDanglingRefs);
    ASSERT_GE(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].code, "SDE_DANGLING_REF");
    EXPECT_EQ(result.diagnostics[0].severity, Severity::Error);
}

// ─── Cross-namespace isolation ────────────────────────────────────────────────

TEST(ReferenceResolverTest, SameInstanceIdInTwoModels_NoCollision)
{
    TypeRegistry types;
    types.Freeze();

    // Both Item and Skill use instanceId "01", no cross-references.
    ModelDefinition itemDef;
    itemDef.id = "Item";

    ModelDefinition skillDef;
    skillDef.id = "Skill";

    auto item  = MakeInstance("Item",  "01");
    auto skill = MakeInstance("Skill", "01");

    std::vector<ModelInstance>   instances = {item, skill};
    std::vector<ModelDefinition> defs      = {itemDef, skillDef};

    ReferenceResolver resolver;
    resolver.BuildIndex(instances);
    auto result = resolver.Resolve(instances, defs, types);

    EXPECT_FALSE(result.hasDanglingRefs);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(ReferenceResolverTest, RefToWrongModel_TreatedAsDangling)
{
    // Skill "s01" has Ref<Item> "owner" = "s01".
    // The index has Skill/s01 but no Item/s01 — compound key prevents collision.
    TypeRegistry types;
    TypeNodeId   refItemType = types.Ref("Item");
    types.Freeze();

    FieldDefinition ownerFd;
    ownerFd.id   = "owner";
    ownerFd.type = refItemType;

    ModelDefinition skillDef;
    skillDef.id = "Skill";
    skillDef.fields.push_back(ownerFd);

    // Only a Skill/s01 exists — there is no Item/s01.
    auto skill = MakeInstanceWithRef("Skill", "s01", "owner", "s01");

    std::vector<ModelInstance>   instances = {skill};
    std::vector<ModelDefinition> defs      = {skillDef};

    ReferenceResolver resolver;
    resolver.BuildIndex(instances);
    auto result = resolver.Resolve(instances, defs, types);

    EXPECT_TRUE(result.hasDanglingRefs);
    EXPECT_EQ(result.diagnostics[0].code, "SDE_DANGLING_REF");
}

// ─── Cycle detection ──────────────────────────────────────────────────────────

TEST(ReferenceResolverTest, DirectSelfRef_DetectedAsCycle)
{
    TypeRegistry types;
    TypeNodeId   refType = types.Ref("Node");
    types.Freeze();

    FieldDefinition nextFd;
    nextFd.id   = "next";
    nextFd.type = refType;

    ModelDefinition nodeDef;
    nodeDef.id = "Node";
    nodeDef.fields.push_back(nextFd);

    // Node/a → next → Node/a  (self-reference).
    auto nodeA = MakeInstanceWithRef("Node", "a", "next", "a");

    std::vector<ModelInstance>   instances = {nodeA};
    std::vector<ModelDefinition> defs      = {nodeDef};

    ReferenceResolver resolver;
    resolver.BuildIndex(instances);
    auto result = resolver.Resolve(instances, defs, types);

    EXPECT_TRUE(result.hasCycles);
    bool hasCycleDiag = false;
    for (const auto& d : result.diagnostics)
        if (d.code == "SDE_CYCLE") { hasCycleDiag = true; break; }
    EXPECT_TRUE(hasCycleDiag);
}

// ─── No-op on empty inputs ────────────────────────────────────────────────────

TEST(ReferenceResolverTest, EmptyInstances_ReturnsCleanResult)
{
    TypeRegistry types;
    types.Freeze();

    ReferenceResolver resolver;
    resolver.BuildIndex({});
    auto result = resolver.Resolve({}, {}, types);

    EXPECT_FALSE(result.hasDanglingRefs);
    EXPECT_FALSE(result.hasCycles);
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_TRUE(result.resolvedHandles.empty());
}
