/// @file ModelCompiler.cpp
/// @brief Full compilation pipeline: default inference, validation, resolution, graph build.

#include "SDE/Compilation/ModelCompiler.h"
#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/ModelDefinition.h"
#include "SDE/Model/TypeNode.h"
#include "SDE/Validation/Validator.h"

namespace SDE
{

// ─── Constructor ──────────────────────────────────────────────────────────────

ModelCompiler::ModelCompiler(const RuleRegistry& ruleRegistry,
                              const TypeRegistry& typeRegistry,
                              CompileOptions      options)
    : mRuleRegistry(ruleRegistry)
    , mTypeRegistry(typeRegistry)
    , mOptions(options)
{
}

// ─── Compile ──────────────────────────────────────────────────────────────────

CompileResult ModelCompiler::Compile(std::vector<ModelInstance>          instances,
                                      const std::vector<ModelDefinition>& definitions) const
{
    CompileResult result;

    // ─── Step 1: Infer defaults ───────────────────────────────────────────────

    if (mOptions.inferDefaults)
        InferDefaults(instances, definitions);

    // ─── Step 2: Validate (schema + per-field + cross-field rules) ────────────

    Validator validator(mRuleRegistry, mTypeRegistry);
    result.validation = validator.Validate(instances, definitions);
    result.state      = result.validation.state;

    if (mOptions.abortOnFirstError && !IsUsable(result.state))
    {
        result.state = Merge(result.state, CompileState::Aborted);
        return result;
    }

    // ─── Step 3: Reference resolution ────────────────────────────────────────

    ReferenceResolver resolver;
    resolver.BuildIndex(instances);
    auto resolved = resolver.Resolve(instances, definitions, mTypeRegistry);

    for (auto& d : resolved.diagnostics)
        result.validation.diagnostics.push_back(std::move(d));

    if (resolved.hasDanglingRefs)
        result.state = Merge(result.state, CompileState::UnresolvedRefs);
    if (resolved.hasCycles)
        result.state = Merge(result.state, CompileState::ValidationFailed);

    // Stop here when references are broken — the graph would be incomplete.
    if (result.state >= CompileState::UnresolvedRefs)
        return result;

    // ─── Step 4: Build compiled graph ─────────────────────────────────────────

    result.graph = BuildGraph(instances, resolved);
    return result;
}

// ─── InferDefaults ────────────────────────────────────────────────────────────

void ModelCompiler::InferDefaults(std::vector<ModelInstance>&         instances,
                                   const std::vector<ModelDefinition>& definitions) const
{
    // Build definition lookup.
    std::map<std::string, const ModelDefinition*> defMap;
    for (const auto& def : definitions)
        defMap[def.id] = &def;

    for (auto& inst : instances)
    {
        auto it = defMap.find(inst.modelId);
        if (it == defMap.end()) continue;

        for (const auto& fieldDef : it->second->fields)
        {
            if (!fieldDef.defaultVal) continue;
            if (inst.fields.count(fieldDef.id)) continue;

            // Insert the default as a RawText; the validator will type-check it.
            RawValue rv;
            rv.data     = RawText{*fieldDef.defaultVal};
            rv.location = {inst.sourceFile, 0, 0};
            inst.fields[fieldDef.id] = std::move(rv);
        }
    }
}

// ─── BuildGraph ───────────────────────────────────────────────────────────────

CompiledModelGraph ModelCompiler::BuildGraph(
    const std::vector<ModelInstance>&       instances,
    const ReferenceResolver::ResolveResult& resolved) const
{
    CompiledModelGraph graph;

    for (const auto& inst : instances)
    {
        CompiledInstance ci;
        ci.modelId    = inst.modelId;
        ci.instanceId = inst.instanceId;
        ci.origin     = inst.origin;

        for (const auto& [fieldId, rawValue] : inst.fields)
        {
            CompiledValue cv;

            // Check whether this field was resolved to a Ref handle.
            std::string hkey = inst.modelId + "/" + inst.instanceId + "/" + fieldId;
            auto        hIt  = resolved.resolvedHandles.find(hkey);

            if (hIt != resolved.resolvedHandles.end())
            {
                cv.data = hIt->second;
            }
            else
            {
                // Convert raw value to compiled value (scalars only; collections go via slab).
                std::visit([&](const auto& v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, RawNull>)
                        cv.data = CompiledNull{};
                    else if constexpr (std::is_same_v<T, RawInteger>)
                        cv.data = static_cast<CompiledInteger>(v);
                    else if constexpr (std::is_same_v<T, RawNumber>)
                        cv.data = static_cast<CompiledNumber>(v);
                    else if constexpr (std::is_same_v<T, RawBool>)
                        cv.data = static_cast<CompiledBool>(v);
                    else if constexpr (std::is_same_v<T, RawText>)
                        cv.data = static_cast<CompiledText>(v);
                    else if constexpr (std::is_same_v<T, RawArray>)
                    {
                        // Arrays: store elements contiguously in the slab.
                        SlabRange range = graph.mSlab.Reserve(
                            static_cast<uint32_t>(v.elements.size()));
                        for (uint32_t i = 0; i < v.elements.size(); ++i)
                        {
                            CompiledValue elem;
                            elem.data = CompiledNull{};
                            graph.mSlab.mSlots[range.offset + i] = std::move(elem);
                        }
                        cv.data = CompiledArrayRef{range};
                    }
                    else if constexpr (std::is_same_v<T, RawObject>)
                    {
                        // Objects: alternating key/value pairs in the slab.
                        uint32_t  pairCount = static_cast<uint32_t>(v.fields.size());
                        SlabRange range     = graph.mSlab.Reserve(pairCount * 2);
                        uint32_t  i         = range.offset;
                        for (const auto& [k, val] : v.fields)
                        {
                            CompiledValue key;
                            key.data = CompiledText{k};
                            graph.mSlab.mSlots[i++] = std::move(key);
                            CompiledValue entry;
                            entry.data = CompiledNull{};
                            graph.mSlab.mSlots[i++] = std::move(entry);
                        }
                        cv.data = CompiledObjectRef{range};
                    }
                }, rawValue.data);
            }

            ci.fields[fieldId] = std::move(cv);
        }

        graph.AddInstance(std::move(ci));
    }

    return graph;
}

} // namespace SDE
