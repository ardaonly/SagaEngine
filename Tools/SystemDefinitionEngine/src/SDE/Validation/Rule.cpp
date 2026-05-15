/// @file Rule.cpp
/// @brief Concrete per-field and cross-field rule implementations.

#include "SDE/Validation/Rule.h"
#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/FieldDefinition.h"

#include <regex>
#include <sstream>

namespace SDE
{

// ─── RangeRule ────────────────────────────────────────────────────────────────

RangeRule::RangeRule(double min, double max)
    : mMin(min)
    , mMax(max)
{
}

bool RangeRule::Apply(const RawValue&        value,
                      const FieldDefinition& field,
                      const SourceLocation&  loc,
                      std::vector<Diagnostic>& out) const
{
    if (!value.IsNumber())
    {
        out.push_back(Diagnostic::MakeError(loc, "SDE_RANGE",
            "Field '" + field.id + "': expected a numeric value for range check."));
        out.back().category = DiagnosticCategory::Rule;
        return false;
    }

    double v = value.AsDouble();
    if (v < mMin || v > mMax)
    {
        std::ostringstream msg;
        msg << "Field '" << field.id << "': value " << v
            << " is outside the allowed range [" << mMin << ", " << mMax << "].";
        out.push_back(Diagnostic::MakeError(loc, "SDE_RANGE", msg.str()));
        out.back().category = DiagnosticCategory::Rule;
        return false;
    }
    return true;
}

// ─── RegexRule ────────────────────────────────────────────────────────────────

RegexRule::RegexRule(std::string pattern)
    : mPattern(std::move(pattern))
    , mCompiled(mPattern)
{
}

bool RegexRule::Apply(const RawValue&        value,
                      const FieldDefinition& field,
                      const SourceLocation&  loc,
                      std::vector<Diagnostic>& out) const
{
    if (!value.IsText())
    {
        out.push_back(Diagnostic::MakeError(loc, "SDE_REGEX",
            "Field '" + field.id + "': expected a text value for regex check."));
        out.back().category = DiagnosticCategory::Rule;
        return false;
    }

    const auto& text = std::get<RawText>(value.data);
    if (!std::regex_match(text, mCompiled))
    {
        out.push_back(Diagnostic::MakeError(loc, "SDE_REGEX",
            "Field '" + field.id + "': value '" + text +
            "' does not match pattern '" + mPattern + "'."));
        out.back().category = DiagnosticCategory::Rule;
        return false;
    }
    return true;
}

// ─── EnumMembershipRule ───────────────────────────────────────────────────────

EnumMembershipRule::EnumMembershipRule(std::string enumId)
    : mEnumId(std::move(enumId))
{
}

bool EnumMembershipRule::Apply(const RawValue&        value,
                                const FieldDefinition& field,
                                const SourceLocation&  loc,
                                std::vector<Diagnostic>& out) const
{
    // The Validator is responsible for looking up the EnumDefinition and calling
    // ContainsMember(). This rule stores the enumId so it can be serialized.
    // The actual membership check is delegated to Validator::CheckTypeMatch().
    (void)value; (void)field; (void)loc; (void)out;
    return true;
}

// ─── RefIntegrityRule ─────────────────────────────────────────────────────────

bool RefIntegrityRule::Apply(const RawValue&        value,
                             const FieldDefinition& field,
                             const SourceLocation&  loc,
                             std::vector<Diagnostic>& out) const
{
    if (!value.IsText())
    {
        out.push_back(Diagnostic::MakeError(loc, "SDE_REF_INTEGRITY",
            "Field '" + field.id + "': expected a text reference id."));
        out.back().category = DiagnosticCategory::Rule;
        return false;
    }

    const auto& text = std::get<RawText>(value.data);
    if (text.empty() || text.find('/') != std::string::npos)
    {
        out.push_back(Diagnostic::MakeError(loc, "SDE_REF_INTEGRITY",
            "Field '" + field.id + "': reference id must be a non-empty local instance id."));
        out.back().category = DiagnosticCategory::Rule;
        out.back().metadata["fieldId"] = field.id;
        return false;
    }

    return true;
}

// ─── CrossFieldRule ───────────────────────────────────────────────────────────

CrossFieldRule::CrossFieldRule(std::string ruleId, std::shared_ptr<Predicate> predicate)
    : mRuleId(std::move(ruleId))
    , mPredicate(std::move(predicate))
{
}

bool CrossFieldRule::Apply(const ModelInstance&     instance,
                            const SourceLocation&    loc,
                            std::vector<Diagnostic>& out) const
{
    if (!mPredicate->Evaluate(instance))
    {
        out.push_back(Diagnostic::MakeError(loc, mRuleId,
            "Cross-field rule '" + mRuleId + "' failed for instance '" +
            instance.instanceId + "'."));
        out.back().category = DiagnosticCategory::Rule;
        return false;
    }
    return true;
}

const std::string& CrossFieldRule::RuleId() const noexcept
{
    return mRuleId;
}

const Predicate& CrossFieldRule::GetPredicate() const noexcept
{
    return *mPredicate;
}

} // namespace SDE
