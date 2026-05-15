/// @file Rule.h
/// @brief Per-field and cross-field validation rule hierarchy.

#pragma once

#include "SDE/Validation/Diagnostic.h"
#include "SDE/Validation/Predicate.h"

#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace SDE
{

struct RawValue;       // forward decl — defined in ModelLoader.h.
struct ModelInstance;  // forward decl — defined in ModelLoader.h.
struct FieldDefinition;

// ─── Rule ─────────────────────────────────────────────────────────────────────

/// Abstract base for per-field validation constraints.
///
/// Applied to a single RawValue in the context of its FieldDefinition.
/// Cross-field constraints use CrossFieldRule instead.
class Rule
{
public:
    virtual ~Rule() = default;

    /// Apply the rule. Emit diagnostics and return false on failure.
    virtual bool Apply(const RawValue&        value,
                       const FieldDefinition& field,
                       const SourceLocation&  loc,
                       std::vector<Diagnostic>& outDiagnostics) const = 0;

    /// Stable rule behavior identifier, e.g. "range". This is not a diagnostic code.
    [[nodiscard]] virtual std::string RuleId() const = 0;
};

// ─── RangeRule ────────────────────────────────────────────────────────────────

/// Enforce min <= value <= max on Number and Integer fields.
class RangeRule final : public Rule
{
public:
    RangeRule(double min, double max);

    bool Apply(const RawValue&        value,
               const FieldDefinition& field,
               const SourceLocation&  loc,
               std::vector<Diagnostic>& out) const override;

    [[nodiscard]] std::string RuleId() const override { return "range"; }

private:
    double mMin;
    double mMax;
};

// ─── RegexRule ────────────────────────────────────────────────────────────────

/// Enforce that a Text field matches a regular expression.
class RegexRule final : public Rule
{
public:
    explicit RegexRule(std::string pattern);

    bool Apply(const RawValue&        value,
               const FieldDefinition& field,
               const SourceLocation&  loc,
               std::vector<Diagnostic>& out) const override;

    [[nodiscard]] std::string RuleId() const override { return "regex"; }

private:
    std::string mPattern;
    std::regex  mCompiled;
};

// ─── EnumMembershipRule ───────────────────────────────────────────────────────

/// Enforce that a Text or Enum field value is a member of the declared enum.
class EnumMembershipRule final : public Rule
{
public:
    explicit EnumMembershipRule(std::string enumId);

    bool Apply(const RawValue&        value,
               const FieldDefinition& field,
               const SourceLocation&  loc,
               std::vector<Diagnostic>& out) const override;

    [[nodiscard]] std::string RuleId() const override { return "enumMember"; }

    [[nodiscard]] const std::string& EnumId() const noexcept { return mEnumId; }

private:
    std::string mEnumId;
};

// ─── RefIntegrityRule ─────────────────────────────────────────────────────────

/// Enforce that a Ref field carries a non-empty local instance id before resolver lookup.
class RefIntegrityRule final : public Rule
{
public:
    bool Apply(const RawValue&        value,
               const FieldDefinition& field,
               const SourceLocation&  loc,
               std::vector<Diagnostic>& out) const override;

    [[nodiscard]] std::string RuleId() const override { return "refIntegrity"; }
};

// ─── CrossFieldRule ───────────────────────────────────────────────────────────

/// Cross-field constraint backed by a serializable Predicate tree.
///
/// Applied to an entire ModelInstance rather than a single field value.
/// Distinct from Rule — ModelDefinition::crossFieldRules holds these separately.
class CrossFieldRule
{
public:
    CrossFieldRule(std::string ruleId, std::shared_ptr<Predicate> predicate);

    /// Apply to the whole instance. Emit a diagnostic when the predicate fails.
    bool Apply(const ModelInstance&     instance,
               const SourceLocation&    loc,
               std::vector<Diagnostic>& outDiagnostics) const;

    [[nodiscard]] const std::string& RuleId()     const noexcept;
    [[nodiscard]] const Predicate&   GetPredicate() const noexcept;

private:
    std::string              mRuleId;
    std::shared_ptr<Predicate> mPredicate;
};

} // namespace SDE
