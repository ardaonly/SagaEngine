/// @file Predicate.cpp
/// @brief Predicate AST node evaluation implementations.

#include "SDE/Validation/Predicate.h"
#include "SDE/IO/ModelLoader.h"

#include <variant>

namespace SDE
{

// ─── FieldExistsPredicate ─────────────────────────────────────────────────────

FieldExistsPredicate::FieldExistsPredicate(std::string fieldId)
    : mFieldId(std::move(fieldId))
{
}

bool FieldExistsPredicate::Evaluate(const ModelInstance& instance) const
{
    auto it = instance.fields.find(mFieldId);
    if (it == instance.fields.end())
        return false;
    return !std::holds_alternative<RawNull>(it->second.data);
}

// ─── FieldEqualsPredicate ─────────────────────────────────────────────────────

FieldEqualsPredicate::FieldEqualsPredicate(std::string fieldId, std::string expectedValue)
    : mFieldId(std::move(fieldId))
    , mExpectedValue(std::move(expectedValue))
{
}

bool FieldEqualsPredicate::Evaluate(const ModelInstance& instance) const
{
    auto it = instance.fields.find(mFieldId);
    if (it == instance.fields.end())
        return false;
    const RawValue& v = it->second;
    if (const auto* text = std::get_if<RawText>(&v.data))
        return *text == mExpectedValue;
    if (const auto* b = std::get_if<RawBool>(&v.data))
        return (*b ? "true" : "false") == mExpectedValue;
    if (const auto* i = std::get_if<RawInteger>(&v.data))
        return std::to_string(*i) == mExpectedValue;
    return false;
}

// ─── FieldRangePredicate ──────────────────────────────────────────────────────

FieldRangePredicate::FieldRangePredicate(std::string fieldId, double min, double max)
    : mFieldId(std::move(fieldId))
    , mMin(min)
    , mMax(max)
{
}

bool FieldRangePredicate::Evaluate(const ModelInstance& instance) const
{
    auto it = instance.fields.find(mFieldId);
    if (it == instance.fields.end())
        return false;
    const RawValue& v = it->second;
    if (!v.IsNumber())
        return false;
    double val = v.AsDouble();
    return val >= mMin && val <= mMax;
}

// ─── AndPredicate ─────────────────────────────────────────────────────────────

AndPredicate::AndPredicate(std::shared_ptr<Predicate> lhs, std::shared_ptr<Predicate> rhs)
    : mLhs(std::move(lhs))
    , mRhs(std::move(rhs))
{
}

bool AndPredicate::Evaluate(const ModelInstance& instance) const
{
    return mLhs->Evaluate(instance) && mRhs->Evaluate(instance);
}

// ─── OrPredicate ──────────────────────────────────────────────────────────────

OrPredicate::OrPredicate(std::shared_ptr<Predicate> lhs, std::shared_ptr<Predicate> rhs)
    : mLhs(std::move(lhs))
    , mRhs(std::move(rhs))
{
}

bool OrPredicate::Evaluate(const ModelInstance& instance) const
{
    return mLhs->Evaluate(instance) || mRhs->Evaluate(instance);
}

// ─── ImplicationPredicate ─────────────────────────────────────────────────────

ImplicationPredicate::ImplicationPredicate(std::shared_ptr<Predicate> condition,
                                            std::shared_ptr<Predicate> consequent)
    : mCondition(std::move(condition))
    , mConsequent(std::move(consequent))
{
}

bool ImplicationPredicate::Evaluate(const ModelInstance& instance) const
{
    // P → Q  ≡  ¬P ∨ Q
    if (!mCondition->Evaluate(instance))
        return true;
    return mConsequent->Evaluate(instance);
}

} // namespace SDE
