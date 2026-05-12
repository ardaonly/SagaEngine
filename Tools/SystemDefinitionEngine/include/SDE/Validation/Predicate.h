/// @file Predicate.h
/// @brief Serializable condition AST for cross-field rules.
///
/// Each concrete Predicate subclass has a stable PredicateKind() string and can be
/// reconstructed from JSON, making rules inspectable by the editor and round-trippable
/// for DSL compilation.

#pragma once

#include <memory>
#include <string>

namespace SDE
{

struct ModelInstance; // forward decl — defined in ModelLoader.h.

// ─── Predicate ────────────────────────────────────────────────────────────────

/// Abstract AST node for a serializable condition over a ModelInstance.
class Predicate
{
public:
    virtual ~Predicate() = default;

    /// Evaluate the predicate against a model instance. Returns true when satisfied.
    [[nodiscard]] virtual bool Evaluate(const ModelInstance& instance) const = 0;

    /// Stable string identifying this predicate kind for serialization.
    [[nodiscard]] virtual std::string PredicateKind() const = 0;
};

// ─── FieldExistsPredicate ─────────────────────────────────────────────────────

/// True when the named field is present and non-null in the instance.
class FieldExistsPredicate final : public Predicate
{
public:
    explicit FieldExistsPredicate(std::string fieldId);

    [[nodiscard]] bool        Evaluate(const ModelInstance& instance) const override;
    [[nodiscard]] std::string PredicateKind() const override { return "field_exists"; }

private:
    std::string mFieldId;
};

// ─── FieldEqualsPredicate ─────────────────────────────────────────────────────

/// True when the named field's string representation equals the expected value.
class FieldEqualsPredicate final : public Predicate
{
public:
    FieldEqualsPredicate(std::string fieldId, std::string expectedValue);

    [[nodiscard]] bool        Evaluate(const ModelInstance& instance) const override;
    [[nodiscard]] std::string PredicateKind() const override { return "field_equals"; }

private:
    std::string mFieldId;
    std::string mExpectedValue;
};

// ─── FieldRangePredicate ──────────────────────────────────────────────────────

/// True when the named numeric field satisfies min <= value <= max.
class FieldRangePredicate final : public Predicate
{
public:
    FieldRangePredicate(std::string fieldId, double min, double max);

    [[nodiscard]] bool        Evaluate(const ModelInstance& instance) const override;
    [[nodiscard]] std::string PredicateKind() const override { return "field_range"; }

private:
    std::string mFieldId;
    double      mMin;
    double      mMax;
};

// ─── AndPredicate ─────────────────────────────────────────────────────────────

/// True when both sub-predicates evaluate to true.
class AndPredicate final : public Predicate
{
public:
    AndPredicate(std::shared_ptr<Predicate> lhs, std::shared_ptr<Predicate> rhs);

    [[nodiscard]] bool        Evaluate(const ModelInstance& instance) const override;
    [[nodiscard]] std::string PredicateKind() const override { return "and"; }

private:
    std::shared_ptr<Predicate> mLhs;
    std::shared_ptr<Predicate> mRhs;
};

// ─── OrPredicate ──────────────────────────────────────────────────────────────

/// True when at least one sub-predicate evaluates to true.
class OrPredicate final : public Predicate
{
public:
    OrPredicate(std::shared_ptr<Predicate> lhs, std::shared_ptr<Predicate> rhs);

    [[nodiscard]] bool        Evaluate(const ModelInstance& instance) const override;
    [[nodiscard]] std::string PredicateKind() const override { return "or"; }

private:
    std::shared_ptr<Predicate> mLhs;
    std::shared_ptr<Predicate> mRhs;
};

// ─── ImplicationPredicate ─────────────────────────────────────────────────────

/// True when: if condition is satisfied, then consequent must also be satisfied.
///
/// Encodes rules of the form "if Recipe.timed == true then Recipe.duration > 0".
class ImplicationPredicate final : public Predicate
{
public:
    ImplicationPredicate(std::shared_ptr<Predicate> condition,
                         std::shared_ptr<Predicate> consequent);

    [[nodiscard]] bool        Evaluate(const ModelInstance& instance) const override;
    [[nodiscard]] std::string PredicateKind() const override { return "implies"; }

private:
    std::shared_ptr<Predicate> mCondition;
    std::shared_ptr<Predicate> mConsequent;
};

} // namespace SDE
