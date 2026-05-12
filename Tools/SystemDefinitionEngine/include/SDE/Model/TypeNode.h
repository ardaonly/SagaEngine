/// @file TypeNode.h
/// @brief Flat type node system for SDE's type vocabulary.
///
/// Types are registered in a TypeRegistry and referenced by TypeNodeId handles.
/// Equality between types reduces to comparing TypeNodeId values (O(1)).
/// No recursive ownership — the registry owns all nodes.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SDE
{

// ─── TypeNodeId ───────────────────────────────────────────────────────────────

/// Opaque handle into a TypeRegistry. Comparable and cheaply copyable.
using TypeNodeId = uint32_t;

/// Sentinel: no type assigned.
constexpr TypeNodeId kInvalidTypeNodeId = 0;

// ─── TypeKind ─────────────────────────────────────────────────────────────────

/// The kind of a type node. Determines which fields of TypeNode are meaningful.
enum class TypeKind
{
    Number,  ///< IEEE-754 double.
    Integer, ///< Signed 64-bit integer. Distinct from Number for schema enforcement.
    Text,
    Boolean,
    Color,
    Enum,    ///< Closed set of named values; TypeNode::enumId names the EnumDefinition.
    Ref,     ///< Typed reference to another model; TypeNode::refModelId names the target.
    List,    ///< Homogeneous ordered sequence; TypeNode::elementType is the element type.
    Map,     ///< String-keyed map; TypeNode::keyType and valueType are the key/value types.
    Struct,  ///< Inline anonymous struct; sub-fields stored separately in TypeRegistry.
};

// ─── TypeNode ─────────────────────────────────────────────────────────────────

/// One node in the type graph. Owned by a TypeRegistry; accessed via TypeNodeId.
struct TypeNode
{
    TypeNodeId  id          = kInvalidTypeNodeId;
    TypeKind    kind        = TypeKind::Number;
    std::string enumId;                            ///< Valid when kind == Enum.
    std::string refModelId;                        ///< Valid when kind == Ref.
    TypeNodeId  elementType = kInvalidTypeNodeId;  ///< Valid when kind == List.
    TypeNodeId  keyType     = kInvalidTypeNodeId;  ///< Valid when kind == Map.
    TypeNodeId  valueType   = kInvalidTypeNodeId;  ///< Valid when kind == Map.
};

// ─── StructFieldSpec ──────────────────────────────────────────────────────────

/// One named field within a Struct type node.
struct StructFieldSpec
{
    std::string name;
    TypeNodeId  type = kInvalidTypeNodeId;
};

// ─── TypeRegistry ─────────────────────────────────────────────────────────────

/// Owns all TypeNodes. Built once during schema loading, then frozen.
///
/// All pipeline components share a const reference. Type equality is TypeNodeId equality.
class TypeRegistry
{
public:
    /// Register a primitive type (Number, Integer, Text, Boolean, Color).
    [[nodiscard]] TypeNodeId Primitive(TypeKind kind);

    /// Register a reference type pointing at the model named by modelId.
    [[nodiscard]] TypeNodeId Ref(std::string modelId);

    /// Register an enum type backed by the named EnumDefinition.
    [[nodiscard]] TypeNodeId Enum(std::string enumId);

    /// Register a homogeneous list type.
    [[nodiscard]] TypeNodeId List(TypeNodeId elementType);

    /// Register a string-keyed map type.
    [[nodiscard]] TypeNodeId Map(TypeNodeId keyType, TypeNodeId valueType);

    /// Register an inline struct type with the given named field specs.
    [[nodiscard]] TypeNodeId Struct(std::vector<StructFieldSpec> fields);

    /// Look up a TypeNode by id. Asserts on invalid id.
    [[nodiscard]] const TypeNode& Get(TypeNodeId id) const;

    /// Return the named sub-field specs for a Struct node; empty for non-Struct nodes.
    [[nodiscard]] const std::vector<StructFieldSpec>& StructFields(TypeNodeId id) const;

    /// Prevent further registrations. Must be called before the pipeline runs.
    void Freeze();

    [[nodiscard]] bool IsFrozen() const noexcept;

    /// Return a human-readable name, e.g. "Ref<Item>", "List<Number>", "Integer".
    [[nodiscard]] std::string TypeName(TypeNodeId id) const;

private:
    std::vector<TypeNode>                              mNodes;
    std::map<TypeNodeId, std::vector<StructFieldSpec>> mStructFields;
    bool                                               mFrozen = false;
    TypeNodeId                                         mNextId = 1;

    [[nodiscard]] TypeNodeId Allocate(TypeNode node);
};

} // namespace SDE
