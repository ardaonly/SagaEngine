/// @file Validator.h
/// @brief Rule application engine and rule registry.

#pragma once

#include "SDE/Validation/ValidationResult.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SDE
{

class Rule;
class TypeRegistry;
struct ModelInstance;
struct ModelDefinition;

// ─── RuleRegistry ─────────────────────────────────────────────────────────────

/// Maps rule ids to shared Rule instances. Built once, then frozen before the pipeline runs.
///
/// Caller constructs and populates the registry, then passes it to Validator.
/// No singleton — the registry is a value passed by const reference.
class RuleRegistry
{
public:
    /// Register a rule by id. Fails silently after Freeze().
    void Register(std::string id, std::shared_ptr<Rule> rule);

    /// Prevent further registrations.
    void Freeze();

    [[nodiscard]] bool IsFrozen() const noexcept;

    /// Look up a rule by id. Returns nullptr when not found.
    [[nodiscard]] std::shared_ptr<Rule> Find(const std::string& id) const noexcept;

    /// Register the built-in rules (SDE_RANGE, SDE_REGEX, SDE_ENUM_MEMBER).
    static void RegisterBuiltIns(RuleRegistry& registry);

private:
    std::unordered_map<std::string, std::shared_ptr<Rule>> mRules;
    bool                                                    mFrozen = false;
};

// ─── Validator ────────────────────────────────────────────────────────────────

/// Applies per-field and cross-field rules to model instances.
class Validator
{
public:
    Validator(const RuleRegistry& ruleRegistry,
              const TypeRegistry& typeRegistry);

    /// Validate all instances against their definitions.
    [[nodiscard]] ValidationResult Validate(
        const std::vector<ModelInstance>&   instances,
        const std::vector<ModelDefinition>& definitions) const;

    /// Validate a single instance against its definition.
    [[nodiscard]] ValidationResult ValidateOne(
        const ModelInstance&   instance,
        const ModelDefinition& definition) const;

private:
    const RuleRegistry& mRuleRegistry;
    const TypeRegistry& mTypeRegistry;

    void ApplyFieldRules(const FieldDefinition& field,
                         const RawValue&        value,
                         const SourceLocation&  loc,
                         std::vector<Diagnostic>& out) const;

    void ApplyCrossFieldRules(const ModelInstance&     instance,
                               const ModelDefinition&   definition,
                               std::vector<Diagnostic>& out) const;

    /// Validate that the raw value's actual kind matches the declared TypeNodeId.
    void CheckTypeMatch(const RawValue&        value,
                        const FieldDefinition& field,
                        const SourceLocation&  loc,
                        std::vector<Diagnostic>& out) const;
};

} // namespace SDE
