/// @file Relation.h
/// @brief Declared cross-model relationship between two model types.

#pragma once

#include <string>

namespace SDE
{

// ─── RelationCardinality ──────────────────────────────────────────────────────

/// The cardinality constraint on a cross-model relationship.
enum class RelationCardinality { OneToOne, OneToMany, ManyToMany };

// ─── Relation ─────────────────────────────────────────────────────────────────

/// Declares that a field on a source model references instances of a target model.
///
/// The resolver uses this to enforce non-empty and cardinality constraints on Ref fields.
struct Relation
{
    std::string         id;
    std::string         sourceModelId;
    std::string         sourceFieldId; ///< The field that carries the Ref(s).
    std::string         targetModelId;
    RelationCardinality cardinality = RelationCardinality::OneToOne;
    bool                nonEmpty    = false; ///< When true, at least one ref is required.
};

} // namespace SDE
