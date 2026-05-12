/// @file FieldDefinition.h
/// @brief Schema definition for a single field on a model.

#pragma once

#include "SDE/Model/TypeNode.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SDE
{

class Rule; // forward decl — Rule.h must be included by any TU that destroys a FieldDefinition.

// ─── FieldPresence ────────────────────────────────────────────────────────────

/// Whether a field value must always be present.
enum class FieldPresence { Required, Optional };

// ─── FieldDefinition ──────────────────────────────────────────────────────────

/// Schema definition for one field on a model.
struct FieldDefinition
{
    std::string                         id;          ///< Machine-readable key, e.g. "damage_min".
    std::string                         displayName;
    TypeNodeId                          type     = kInvalidTypeNodeId; ///< Handle into TypeRegistry.
    FieldPresence                       presence = FieldPresence::Required;
    std::optional<std::string>          defaultVal;  ///< String representation of the default value.
    std::vector<std::shared_ptr<Rule>>  rules;       ///< Per-field rules. Shared so the same rule
                                                     ///< instance can appear on multiple fields.
};

} // namespace SDE
