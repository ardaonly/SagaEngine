/// @file TypeNode.cpp
/// @brief TypeRegistry registration, lookup, and type name formatting.

#include "SDE/Model/TypeNode.h"

#include <cassert>
#include <stdexcept>

namespace SDE
{

// ─── Registration ─────────────────────────────────────────────────────────────

TypeNodeId TypeRegistry::Primitive(TypeKind kind)
{
    assert(!mFrozen && "TypeRegistry is frozen");
    TypeNode node;
    node.kind = kind;
    return Allocate(std::move(node));
}

TypeNodeId TypeRegistry::Ref(std::string modelId)
{
    assert(!mFrozen && "TypeRegistry is frozen");
    TypeNode node;
    node.kind       = TypeKind::Ref;
    node.refModelId = std::move(modelId);
    return Allocate(std::move(node));
}

TypeNodeId TypeRegistry::Enum(std::string enumId)
{
    assert(!mFrozen && "TypeRegistry is frozen");
    TypeNode node;
    node.kind   = TypeKind::Enum;
    node.enumId = std::move(enumId);
    return Allocate(std::move(node));
}

TypeNodeId TypeRegistry::List(TypeNodeId elementType)
{
    assert(!mFrozen && "TypeRegistry is frozen");
    assert(elementType != kInvalidTypeNodeId);
    TypeNode node;
    node.kind        = TypeKind::List;
    node.elementType = elementType;
    return Allocate(std::move(node));
}

TypeNodeId TypeRegistry::Map(TypeNodeId keyType, TypeNodeId valueType)
{
    assert(!mFrozen && "TypeRegistry is frozen");
    assert(keyType != kInvalidTypeNodeId && valueType != kInvalidTypeNodeId);
    TypeNode node;
    node.kind      = TypeKind::Map;
    node.keyType   = keyType;
    node.valueType = valueType;
    return Allocate(std::move(node));
}

TypeNodeId TypeRegistry::Struct(std::vector<StructFieldSpec> fields)
{
    assert(!mFrozen && "TypeRegistry is frozen");
    TypeNode   node;
    node.kind      = TypeKind::Struct;
    TypeNodeId id  = Allocate(std::move(node));
    mStructFields[id] = std::move(fields);
    return id;
}

// ─── Lookup ───────────────────────────────────────────────────────────────────

const TypeNode& TypeRegistry::Get(TypeNodeId id) const
{
    assert(id != kInvalidTypeNodeId && id <= mNodes.size() && "TypeNodeId out of range");
    return mNodes[static_cast<std::size_t>(id) - 1];
}

const std::vector<StructFieldSpec>& TypeRegistry::StructFields(TypeNodeId id) const
{
    static const std::vector<StructFieldSpec> kEmpty;
    auto it = mStructFields.find(id);
    return (it != mStructFields.end()) ? it->second : kEmpty;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void TypeRegistry::Freeze()
{
    mFrozen = true;
}

bool TypeRegistry::IsFrozen() const noexcept
{
    return mFrozen;
}

// ─── TypeName ─────────────────────────────────────────────────────────────────

std::string TypeRegistry::TypeName(TypeNodeId id) const
{
    if (id == kInvalidTypeNodeId)
        return "<invalid>";

    const TypeNode& node = Get(id);
    switch (node.kind)
    {
        case TypeKind::Number:  return "Number";
        case TypeKind::Integer: return "Integer";
        case TypeKind::Text:    return "Text";
        case TypeKind::Boolean: return "Boolean";
        case TypeKind::Color:   return "Color";
        case TypeKind::Enum:    return "Enum<" + node.enumId + ">";
        case TypeKind::Ref:     return "Ref<" + node.refModelId + ">";
        case TypeKind::List:    return "List<" + TypeName(node.elementType) + ">";
        case TypeKind::Map:     return "Map<" + TypeName(node.keyType) + "," + TypeName(node.valueType) + ">";
        case TypeKind::Struct:  return "Struct";
    }
    return "<unknown>";
}

// ─── Internal ─────────────────────────────────────────────────────────────────

TypeNodeId TypeRegistry::Allocate(TypeNode node)
{
    TypeNodeId id = mNextId++;
    node.id       = id;
    mNodes.push_back(std::move(node));
    return id;
}

} // namespace SDE
