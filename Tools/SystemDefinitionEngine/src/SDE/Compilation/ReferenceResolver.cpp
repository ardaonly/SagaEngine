/// @file ReferenceResolver.cpp
/// @brief Reference resolution and cycle detection.

#include "SDE/Compilation/ReferenceResolver.h"
#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/ModelDefinition.h"
#include "SDE/Model/TypeNode.h"

#include <unordered_set>

namespace SDE
{
namespace
{

/// Stable string key for a (modelId, instanceId, fieldId) triple — used in resolvedHandles.
std::string HandleKey(const std::string& modelId,
                      const std::string& instanceId,
                      const std::string& fieldId)
{
    return modelId + "/" + instanceId + "/" + fieldId;
}

/// DFS coloring for cycle detection.
enum class Color { White, Gray, Black };

void DfsCycle(const std::string&                                   modelId,
              const std::string&                                   instanceId,
              const std::map<std::pair<std::string,std::string>,
                             const ModelInstance*>&                 index,
              const std::vector<ModelDefinition>&                  definitions,
              const TypeRegistry&                                  types,
              std::map<std::pair<std::string,std::string>, Color>& colors,
              std::vector<Diagnostic>&                             out)
{
    auto key = std::make_pair(modelId, instanceId);
    auto& c  = colors[key];
    if (c == Color::Black) return;
    if (c == Color::Gray)
    {
        out.push_back(Diagnostic::MakeError(
            {}, "SDE_CYCLE",
            "Reference cycle detected involving '" + modelId + "/" + instanceId + "'."));
        out.back().category = DiagnosticCategory::Reference;
        return;
    }
    c = Color::Gray;

    auto instIt = index.find(key);
    if (instIt == index.end()) { c = Color::Black; return; }

    const ModelInstance& inst = *instIt->second;

    // Find the definition for this model to know which fields are Refs.
    const ModelDefinition* def = nullptr;
    for (const auto& d : definitions)
        if (d.id == modelId) { def = &d; break; }

    if (def)
    {
        for (const auto& fieldDef : def->fields)
        {
            if (fieldDef.type == kInvalidTypeNodeId) continue;
            const TypeNode& tn = types.Get(fieldDef.type);
            if (tn.kind != TypeKind::Ref)  continue;

            auto fIt = inst.fields.find(fieldDef.id);
            if (fIt == inst.fields.end()) continue;

            const RawValue& rv = fIt->second;
            if (!rv.IsText()) continue;

            const std::string& targetId = std::get<RawText>(rv.data);
            DfsCycle(tn.refModelId, targetId, index, definitions, types, colors, out);
        }
    }

    c = Color::Black;
}

} // namespace

// ─── BuildIndex ───────────────────────────────────────────────────────────────

void ReferenceResolver::BuildIndex(const std::vector<ModelInstance>& instances)
{
    mIndex.clear();
    for (const auto& inst : instances)
    {
        InstanceKey key{inst.modelId, inst.instanceId};
        // Duplicate ids within the same model are a schema error caught by the validator;
        // here we silently take the first occurrence.
        mIndex.emplace(key, &inst);
    }
}

// ─── Resolve ──────────────────────────────────────────────────────────────────

ReferenceResolver::ResolveResult ReferenceResolver::Resolve(
    const std::vector<ModelInstance>&   instances,
    const std::vector<ModelDefinition>& definitions,
    const TypeRegistry&                 types)
{
    ResolveResult result;

    // Build definition lookup.
    std::map<std::string, const ModelDefinition*> defMap;
    for (const auto& def : definitions)
        defMap[def.id] = &def;

    for (const auto& inst : instances)
    {
        auto defIt = defMap.find(inst.modelId);
        if (defIt == defMap.end()) continue;

        const ModelDefinition& def = *defIt->second;

        for (const auto& fieldDef : def.fields)
        {
            if (fieldDef.type == kInvalidTypeNodeId) continue;
            const TypeNode& tn = types.Get(fieldDef.type);
            if (tn.kind != TypeKind::Ref) continue;

            auto fIt = inst.fields.find(fieldDef.id);
            if (fIt == inst.fields.end()) continue;

            const RawValue& rv = fIt->second;
            if (!rv.IsText()) continue;

            const std::string& targetId  = std::get<RawText>(rv.data);
            InstanceKey        targetKey{tn.refModelId, targetId};

            if (mIndex.find(targetKey) == mIndex.end())
            {
                result.diagnostics.push_back(Diagnostic::MakeError(
                    rv.location, "SDE_DANGLING_REF",
                    "Field '" + fieldDef.id + "' in '" + inst.modelId + "/" +
                    inst.instanceId + "' references non-existent '" +
                    tn.refModelId + "/" + targetId + "'."));
                result.diagnostics.back().category = DiagnosticCategory::Reference;
                result.hasDanglingRefs = true;
                continue;
            }

            // Wrong model type check: the index key already encodes the expected model id,
            // so if the entry exists under (refModelId, targetId) it is the right type.

            std::string hkey = HandleKey(inst.modelId, inst.instanceId, fieldDef.id);
            result.resolvedHandles[hkey] = CompiledInstanceRef{tn.refModelId, targetId};
        }
    }

    // Cycle detection.
    DetectCycles(instances, definitions, types, result.diagnostics);
    for (const auto& d : result.diagnostics)
        if (d.code == "SDE_CYCLE") { result.hasCycles = true; break; }

    return result;
}

// ─── DetectCycles ─────────────────────────────────────────────────────────────

void ReferenceResolver::DetectCycles(const std::vector<ModelInstance>&   instances,
                                      const std::vector<ModelDefinition>& definitions,
                                      const TypeRegistry&                 types,
                                      std::vector<Diagnostic>&            out) const
{
    std::map<InstanceKey, Color> colors;
    for (const auto& inst : instances)
        DfsCycle(inst.modelId, inst.instanceId, mIndex, definitions, types, colors, out);
}

} // namespace SDE
