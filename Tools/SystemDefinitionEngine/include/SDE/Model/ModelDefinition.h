/// @file ModelDefinition.h
/// @brief Schema definition for one kind of game entity (Item, Skill, Recipe, etc.).

#pragma once

#include "SDE/Model/FieldDefinition.h"
#include "SDE/Model/Relation.h"

#include <memory>
#include <string>
#include <vector>

namespace SDE
{

class CrossFieldRule; // forward decl — defined in Rule.h.

// ─── ModelDefinition ──────────────────────────────────────────────────────────

/// Schema for one model kind. Carries field schemas, relations, and cross-field rules.
struct ModelDefinition
{
    std::string                              id;           ///< e.g. "Item"
    std::string                              displayName;
    int                                      schemaVersion = 1;
    std::vector<FieldDefinition>             fields;
    std::vector<Relation>                    relations;
    std::vector<std::shared_ptr<CrossFieldRule>> crossFieldRules;

    /// Return a pointer to the field with the given id, or nullptr when not found.
    [[nodiscard]] const FieldDefinition* FindField(const std::string& fieldId) const noexcept;

    /// Return true when a field with the given id exists.
    [[nodiscard]] bool HasField(const std::string& fieldId) const noexcept;
};

} // namespace SDE
