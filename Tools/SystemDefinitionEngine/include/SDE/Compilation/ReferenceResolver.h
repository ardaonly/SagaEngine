/// @file ReferenceResolver.h
/// @brief Resolves string Ref field values to CompiledInstanceRef handles.
///
/// Two-pass:
///   1. BuildIndex()  — map (modelId, instanceId) → ModelInstance* for all raw instances.
///   2. Resolve()     — walk Ref fields, replace string ids with handles, detect cycles.
///
/// The resolver does NOT mutate ModelInstance fields. It returns a handle map that
/// ModelCompiler uses when building CompiledInstances.

#pragma once

#include "SDE/Compilation/CompiledModelGraph.h"
#include "SDE/Validation/Diagnostic.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace SDE
{

struct ModelInstance;
struct ModelDefinition;
class  TypeRegistry;

// ─── ReferenceResolver ────────────────────────────────────────────────────────

/// Resolves cross-model Ref fields and detects structural errors (dangling refs, cycles).
class ReferenceResolver
{
public:
    // ─── ResolveResult ───────────────────────────────────────────────────────

    struct ResolveResult
    {
        std::vector<Diagnostic>                              diagnostics;
        bool                                                 hasDanglingRefs = false;
        bool                                                 hasCycles       = false;

        /// For each resolved Ref field: maps "modelId/instanceId/fieldId" → handle.
        /// ModelCompiler uses this when building CompiledValue entries.
        std::map<std::string, CompiledInstanceRef> resolvedHandles;
    };

    // ─── Two-pass API ────────────────────────────────────────────────────────

    /// First pass: build the lookup index from all raw instances.
    /// Must be called before Resolve().
    void BuildIndex(const std::vector<ModelInstance>& instances);

    /// Second pass: resolve all Ref fields, detect dangling references and cycles.
    [[nodiscard]] ResolveResult Resolve(
        const std::vector<ModelInstance>&   instances,
        const std::vector<ModelDefinition>& definitions,
        const TypeRegistry&                 types);

private:
    using InstanceKey = std::pair<std::string, std::string>; // (modelId, instanceId)
    std::map<InstanceKey, const ModelInstance*> mIndex;

    void DetectCycles(const std::vector<ModelInstance>&   instances,
                      const std::vector<ModelDefinition>& definitions,
                      const TypeRegistry&                 types,
                      std::vector<Diagnostic>&            out) const;
};

} // namespace SDE
