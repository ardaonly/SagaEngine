/// @file ModelWriter.cpp
/// @brief JsonModelWriter: serialize a CompiledModelGraph to JSON.

#include "SDE/IO/ModelWriter.h"
#include "SDE/Compilation/CompiledModelGraph.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace SDE
{
namespace
{

// ─── CompiledValueToJson ──────────────────────────────────────────────────────

nlohmann::json CompiledValueToJson(const CompiledValue&    value,
                                    const CompiledModelGraph& graph);

nlohmann::json SlabRangeToJsonArray(const ValueSlab& slab, const SlabRange& range,
                                     const CompiledModelGraph& graph)
{
    nlohmann::json arr = nlohmann::json::array();
    for (uint32_t i = range.offset; i < range.offset + range.count; ++i)
        arr.push_back(CompiledValueToJson(slab.At(i), graph));
    return arr;
}

nlohmann::json SlabRangeToJsonObject(const ValueSlab& slab, const SlabRange& range,
                                      const CompiledModelGraph& graph)
{
    // Object pairs are stored as alternating Text key / value pairs in the slab.
    nlohmann::json obj = nlohmann::json::object();
    for (uint32_t i = range.offset; i + 1 < range.offset + range.count; i += 2)
    {
        const CompiledValue& keyNode = slab.At(i);
        if (const auto* k = std::get_if<CompiledText>(&keyNode.data))
            obj[*k] = CompiledValueToJson(slab.At(i + 1), graph);
    }
    return obj;
}

nlohmann::json CompiledValueToJson(const CompiledValue& value, const CompiledModelGraph& graph)
{
    return std::visit([&](const auto& v) -> nlohmann::json {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, CompiledNull>)
            return nullptr;
        else if constexpr (std::is_same_v<T, CompiledInteger>)
            return v;
        else if constexpr (std::is_same_v<T, CompiledNumber>)
            return v;
        else if constexpr (std::is_same_v<T, CompiledBool>)
            return v;
        else if constexpr (std::is_same_v<T, CompiledText>)
            return v;
        else if constexpr (std::is_same_v<T, CompiledInstanceRef>)
            return nlohmann::json{{"$ref", v.modelId + "/" + v.instanceId}};
        else if constexpr (std::is_same_v<T, CompiledArrayRef>)
            return SlabRangeToJsonArray(graph.Slab(), v.range, graph);
        else if constexpr (std::is_same_v<T, CompiledObjectRef>)
            return SlabRangeToJsonObject(graph.Slab(), v.range, graph);
        return nullptr;
    }, value.data);
}

} // namespace

// ─── JsonModelWriter::Write ───────────────────────────────────────────────────

bool JsonModelWriter::Write(const CompiledModelGraph& graph,
                             const std::string&        path,
                             std::string*              outError) const
{
    nlohmann::json root = nlohmann::json::object();

    for (const std::string& modelId : graph.ModelIds())
    {
        nlohmann::json instances = nlohmann::json::array();
        for (const CompiledInstance* inst : graph.AllOf(modelId))
        {
            nlohmann::json obj = nlohmann::json::object();
            for (const auto& [fieldId, val] : inst->fields)
                obj[fieldId] = CompiledValueToJson(val, graph);
            instances.push_back(std::move(obj));
        }
        root[modelId] = std::move(instances);
    }

    std::ofstream file(path);
    if (!file.is_open())
    {
        if (outError)
            *outError = "Cannot open output file: '" + path + "'.";
        return false;
    }

    file << root.dump(2);
    return file.good();
}

} // namespace SDE
