/// @file Validator.cpp
/// @brief RuleRegistry and Validator implementations.

#include "SDE/Validation/Validator.h"
#include "SDE/Validation/Rule.h"
#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/EnumRegistry.h"
#include "SDE/Model/FieldDefinition.h"
#include "SDE/Model/ModelDefinition.h"
#include "SDE/Model/TypeNode.h"

#include <algorithm>
#include <cassert>

namespace SDE
{

// ─── RuleRegistry ─────────────────────────────────────────────────────────────

void RuleRegistry::Register(std::string id, std::shared_ptr<Rule> rule)
{
    if (mFrozen)
        return;
    mRules[std::move(id)] = std::move(rule);
}

void RuleRegistry::Freeze()
{
    mFrozen = true;
}

bool RuleRegistry::IsFrozen() const noexcept
{
    return mFrozen;
}

std::shared_ptr<Rule> RuleRegistry::Find(const std::string& id) const noexcept
{
    auto it = mRules.find(id);
    return (it != mRules.end()) ? it->second : nullptr;
}

void RuleRegistry::RegisterBuiltIns(RuleRegistry& /*registry*/)
{
    // Built-in rules (RangeRule, RegexRule, EnumMembershipRule) are instantiated
    // directly by the JSON schema loader when reading per-field rule declarations.
    // The registry extension point is reserved for project-specific custom rules.
}

// ─── Validator ────────────────────────────────────────────────────────────────

Validator::Validator(const RuleRegistry& ruleRegistry,
                     const TypeRegistry& typeRegistry,
                     const EnumRegistry* enumRegistry)
    : mTypeRegistry(typeRegistry)
    , mEnumRegistry(enumRegistry)
{
    (void)ruleRegistry;
}

ValidationResult Validator::Validate(const std::vector<ModelInstance>&   instances,
                                      const std::vector<ModelDefinition>& definitions) const
{
    ValidationResult result;

    // Build a lookup: modelId → definition pointer.
    std::unordered_map<std::string, const ModelDefinition*> defMap;
    for (const auto& def : definitions)
        defMap[def.id] = &def;

    for (const auto& instance : instances)
    {
        auto it = defMap.find(instance.modelId);
        if (it == defMap.end())
        {
            result.diagnostics.push_back(
                Diagnostic::MakeError({instance.sourceFile, 0, 0}, "SDE_UNKNOWN_MODEL",
                    "No definition found for model '" + instance.modelId + "'."));
            result.diagnostics.back().category = DiagnosticCategory::Schema;
            result.state = Merge(result.state, CompileState::ValidationFailed);
            continue;
        }
        result.Merge(ValidateOne(instance, *it->second));
    }
    std::sort(result.diagnostics.begin(), result.diagnostics.end(), DiagnosticLess);
    return result;
}

ValidationResult Validator::ValidateOne(const ModelInstance&   instance,
                                         const ModelDefinition& definition) const
{
    ValidationResult result;
    std::vector<Diagnostic> diags;

    // ─── Per-field pass ───────────────────────────────────────────────────────

    // Check for required fields missing from the instance.
    for (const auto& fieldDef : definition.fields)
    {
        auto it = instance.fields.find(fieldDef.id);
        if (it == instance.fields.end())
        {
            if (fieldDef.presence == FieldPresence::Required)
            {
                diags.push_back(Diagnostic::MakeError(
                    {instance.sourceFile, 0, 0}, "SDE_MISSING_FIELD",
                    "Instance '" + instance.instanceId + "' is missing required field '" +
                    fieldDef.id + "'."));
                diags.back().category = DiagnosticCategory::Schema;
            }
            continue;
        }

        const RawValue&     value = it->second;
        const SourceLocation loc  = value.location;

        CheckTypeMatch(value, fieldDef, loc, diags);
        ApplyFieldRules(fieldDef, value, loc, diags);
    }

    // ─── Cross-field pass ─────────────────────────────────────────────────────

    ApplyCrossFieldRules(instance, definition, diags);

    // ─── Determine state ──────────────────────────────────────────────────────

    for (const auto& d : diags)
    {
        if (d.severity == Severity::Fatal)
            result.state = Merge(result.state, CompileState::Aborted);
        else if (d.severity == Severity::Error)
            result.state = Merge(result.state, CompileState::ValidationFailed);
        else if (d.severity == Severity::Warning)
            result.state = Merge(result.state, CompileState::WithWarnings);
    }

    result.diagnostics = std::move(diags);
    std::sort(result.diagnostics.begin(), result.diagnostics.end(), DiagnosticLess);
    return result;
}

void Validator::ApplyFieldRules(const FieldDefinition& field,
                                 const RawValue&        value,
                                 const SourceLocation&  loc,
                                 std::vector<Diagnostic>& out) const
{
    for (const auto& rule : field.rules)
    {
        if (rule)
            rule->Apply(value, field, loc, out);
    }
}

void Validator::ApplyCrossFieldRules(const ModelInstance&     instance,
                                      const ModelDefinition&   definition,
                                      std::vector<Diagnostic>& out) const
{
    SourceLocation loc{instance.sourceFile, 0, 0};
    for (const auto& rule : definition.crossFieldRules)
    {
        if (rule)
            rule->Apply(instance, loc, out);
    }
}

void Validator::CheckTypeMatch(const RawValue&        value,
                                const FieldDefinition& field,
                                const SourceLocation&  loc,
                                std::vector<Diagnostic>& out) const
{
    if (field.type == kInvalidTypeNodeId)
        return;

    const TypeNode& node = mTypeRegistry.Get(field.type);

    auto kindMismatch = [&](const std::string& expected) {
        out.push_back(Diagnostic::MakeError(loc, "SDE_TYPE_MISMATCH",
            "Field '" + field.id + "': expected " + expected + " but got a different value kind."));
        out.back().category = DiagnosticCategory::Type;
    };

    switch (node.kind)
    {
        case TypeKind::Number:
            if (!value.IsNumber())
                kindMismatch("Number");
            break;
        case TypeKind::Integer:
            if (!value.IsInteger())
                kindMismatch("Integer");
            break;
        case TypeKind::Text:
            if (!value.IsText())
                kindMismatch("Text");
            break;
        case TypeKind::Enum:
            if (!value.IsText())
            {
                kindMismatch("Enum (text)");
                break;
            }
            if (mEnumRegistry == nullptr)
            {
                out.push_back(Diagnostic::MakeError(loc, "SDE_ENUM_REGISTRY_MISSING",
                    "Field '" + field.id + "': enum validation requires an EnumRegistry."));
                out.back().category = DiagnosticCategory::Schema;
                break;
            }
            if (const EnumDefinition* def = mEnumRegistry->Find(node.enumId))
            {
                const auto& text = std::get<RawText>(value.data);
                if (!def->ContainsMember(text))
                {
                    out.push_back(Diagnostic::MakeError(loc, "SDE_ENUM_MEMBER",
                        "Field '" + field.id + "': value '" + text +
                        "' is not a member of enum '" + node.enumId + "'."));
                    out.back().category = DiagnosticCategory::Rule;
                    out.back().metadata["enumId"] = node.enumId;
                    out.back().metadata["fieldId"] = field.id;
                }
            }
            else
            {
                out.push_back(Diagnostic::MakeError(loc, "SDE_UNKNOWN_ENUM",
                    "Field '" + field.id + "': enum '" + node.enumId + "' is not registered."));
                out.back().category = DiagnosticCategory::Schema;
                out.back().metadata["enumId"] = node.enumId;
            }
            break;
        case TypeKind::Boolean:
            if (!value.IsBool())
                kindMismatch("Boolean");
            break;
        case TypeKind::Ref:
            // Ref fields store the target id as a text value before resolution.
            if (!value.IsText())
                kindMismatch("Ref (text id)");
            break;
        case TypeKind::List:
            if (!value.IsArray())
                kindMismatch("List");
            break;
        case TypeKind::Map:
        case TypeKind::Struct:
            if (!value.IsObject())
                kindMismatch("Map/Struct");
            break;
        default:
            break;
    }
}

} // namespace SDE
