/// @file ModelCompiler.h
/// @brief Drives the full compilation pipeline.
///
/// Pipeline order (inside Compile()):
///   1. InferDefaults     — fill optional fields from FieldDefinition::defaultVal
///   2. Validator         — schema + rule pass
///   3. ReferenceResolver — build index, resolve Ref fields, detect cycles
///   4. BuildGraph        — produce the immutable CompiledModelGraph

#pragma once

#include "SDE/Compilation/CompiledModelGraph.h"
#include "SDE/Compilation/ReferenceResolver.h"
#include "SDE/Validation/ValidationResult.h"

#include <optional>
#include <vector>

namespace SDE
{

class RuleRegistry;
class TypeRegistry;
struct ModelInstance;
struct ModelDefinition;

// ─── CompileOptions ───────────────────────────────────────────────────────────

/// Options controlling pipeline behaviour.
struct CompileOptions
{
    bool abortOnFirstError = false; ///< Stop after the first Fatal or Error diagnostic.
    bool inferDefaults     = true;  ///< Fill optional fields with declared FieldDefinition defaults.
};

// ─── CompileResult ────────────────────────────────────────────────────────────

/// Outcome of a full compilation pass.
struct CompileResult
{
    CompileState                      state = CompileState::Clean;
    std::optional<CompiledModelGraph> graph;      ///< nullopt when state >= UnresolvedRefs.
    ValidationResult                  validation;
};

// ─── ModelCompiler ────────────────────────────────────────────────────────────

/// Orchestrates the full validation and compilation pipeline.
class ModelCompiler
{
public:
    ModelCompiler(const RuleRegistry& ruleRegistry,
                  const TypeRegistry& typeRegistry,
                  CompileOptions      options = {});

    /// Compile a batch of raw instances against their definitions.
    ///
    /// Takes instances by value — the pipeline may mutate them during inference.
    [[nodiscard]] CompileResult Compile(
        std::vector<ModelInstance>          instances,
        const std::vector<ModelDefinition>& definitions) const;

private:
    const RuleRegistry& mRuleRegistry;
    const TypeRegistry& mTypeRegistry;
    CompileOptions      mOptions;

    void InferDefaults(std::vector<ModelInstance>&         instances,
                       const std::vector<ModelDefinition>& definitions) const;

    [[nodiscard]] CompiledModelGraph BuildGraph(
        const std::vector<ModelInstance>&    instances,
        const ReferenceResolver::ResolveResult& resolved) const;
};

} // namespace SDE
