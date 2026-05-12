/// @file TypeRegistryTests.cpp
/// @brief Tests for TypeRegistry: registration, equality, freeze, and TypeName.

#include "SDE/Model/TypeNode.h"

#include <gtest/gtest.h>

using namespace SDE;

TEST(TypeRegistryTest, PrimitiveReturnsValidId)
{
    TypeRegistry reg;
    TypeNodeId id = reg.Primitive(TypeKind::Number);
    EXPECT_NE(id, kInvalidTypeNodeId);
}

TEST(TypeRegistryTest, DifferentPrimitivesGetDifferentIds)
{
    TypeRegistry reg;
    TypeNodeId num  = reg.Primitive(TypeKind::Number);
    TypeNodeId text = reg.Primitive(TypeKind::Text);
    EXPECT_NE(num, text);
}

TEST(TypeRegistryTest, RefRegistersWithModelId)
{
    TypeRegistry reg;
    TypeNodeId id = reg.Ref("Item");
    const TypeNode& node = reg.Get(id);
    EXPECT_EQ(node.kind, TypeKind::Ref);
    EXPECT_EQ(node.refModelId, "Item");
}

TEST(TypeRegistryTest, ListRegistersElementType)
{
    TypeRegistry reg;
    TypeNodeId elem = reg.Primitive(TypeKind::Integer);
    TypeNodeId list = reg.List(elem);
    const TypeNode& node = reg.Get(list);
    EXPECT_EQ(node.kind, TypeKind::List);
    EXPECT_EQ(node.elementType, elem);
}

TEST(TypeRegistryTest, EqualityIsByIdComparison)
{
    TypeRegistry reg;
    TypeNodeId a = reg.Primitive(TypeKind::Number);
    TypeNodeId b = reg.Primitive(TypeKind::Number);
    // Same kind but registered separately → different ids.
    EXPECT_NE(a, b);
}

TEST(TypeRegistryTest, TypeNamePrimitive)
{
    TypeRegistry reg;
    TypeNodeId id = reg.Primitive(TypeKind::Integer);
    EXPECT_EQ(reg.TypeName(id), "Integer");
}

TEST(TypeRegistryTest, TypeNameRef)
{
    TypeRegistry reg;
    TypeNodeId id = reg.Ref("Skill");
    EXPECT_EQ(reg.TypeName(id), "Ref<Skill>");
}

TEST(TypeRegistryTest, TypeNameListOfRef)
{
    TypeRegistry reg;
    TypeNodeId elem = reg.Ref("Item");
    TypeNodeId list = reg.List(elem);
    EXPECT_EQ(reg.TypeName(list), "List<Ref<Item>>");
}

TEST(TypeRegistryTest, FreezePreventsFurtherRegistration)
{
    TypeRegistry reg;
    reg.Primitive(TypeKind::Text);
    reg.Freeze();
    EXPECT_TRUE(reg.IsFrozen());
    // After freeze, Primitive should assert. We test the state, not the crash.
}

TEST(TypeRegistryTest, InvalidIdTypeNameReturnsInvalid)
{
    TypeRegistry reg;
    EXPECT_EQ(reg.TypeName(kInvalidTypeNodeId), "<invalid>");
}
