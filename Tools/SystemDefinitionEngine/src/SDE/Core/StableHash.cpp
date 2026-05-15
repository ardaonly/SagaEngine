/// @file StableHash.cpp
/// @brief Platform-independent hashing helpers for compiler artifacts.

#include "SDE/Core/StableHash.h"
#include "SDE/Compilation/CompiledModelGraph.h"
#include "SDE/Compiler/DependencyManifest.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <type_traits>
#include <variant>

namespace SDE
{
namespace
{

void HashAppend(uint64_t& hash, const std::string& bytes)
{
    constexpr uint64_t kPrime = 1099511628211ull;
    for (unsigned char c : bytes)
    {
        hash ^= static_cast<uint64_t>(c);
        hash *= kPrime;
    }
}

std::string Hex64(uint64_t value)
{
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

std::string NormalizeSlashes(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

void AppendCompiledValue(std::ostringstream& out,
                         const CompiledValue& value,
                         const CompiledModelGraph& graph);

void AppendSlabArray(std::ostringstream& out,
                     const SlabRange& range,
                     const CompiledModelGraph& graph)
{
    out << '[';
    for (uint32_t i = 0; i < range.count; ++i)
    {
        if (i > 0)
            out << ',';
        AppendCompiledValue(out, graph.Slab().At(range.offset + i), graph);
    }
    out << ']';
}

void AppendSlabObject(std::ostringstream& out,
                      const SlabRange& range,
                      const CompiledModelGraph& graph)
{
    out << '{';
    bool first = true;
    for (uint32_t i = range.offset; i + 1 < range.offset + range.count; i += 2)
    {
        const auto* key = std::get_if<CompiledText>(&graph.Slab().At(i).data);
        if (key == nullptr)
            continue;
        if (!first)
            out << ',';
        first = false;
        out << 's' << key->size() << ':' << *key << '=';
        AppendCompiledValue(out, graph.Slab().At(i + 1), graph);
    }
    out << '}';
}

void AppendCompiledValue(std::ostringstream& out,
                         const CompiledValue& value,
                         const CompiledModelGraph& graph)
{
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, CompiledNull>)
            out << "null";
        else if constexpr (std::is_same_v<T, CompiledInteger>)
            out << "i" << v;
        else if constexpr (std::is_same_v<T, CompiledNumber>)
            out << "f" << std::setprecision(17) << v;
        else if constexpr (std::is_same_v<T, CompiledBool>)
            out << "b" << (v ? "1" : "0");
        else if constexpr (std::is_same_v<T, CompiledText>)
            out << "s" << v.size() << ':' << v;
        else if constexpr (std::is_same_v<T, CompiledInstanceRef>)
            out << "r" << v.modelId.size() << ':' << v.modelId << '/'
                << v.instanceId.size() << ':' << v.instanceId;
        else if constexpr (std::is_same_v<T, CompiledArrayRef>)
            AppendSlabArray(out, v.range, graph);
        else if constexpr (std::is_same_v<T, CompiledObjectRef>)
            AppendSlabObject(out, v.range, graph);
    }, value.data);
}

} // namespace

std::string NormalizePathForArtifactHash(const std::filesystem::path& root,
                                         const std::filesystem::path& path)
{
    std::error_code ec;
    const auto absRoot = std::filesystem::weakly_canonical(root, ec);
    const auto absPath = std::filesystem::weakly_canonical(path, ec);
    auto rel = std::filesystem::relative(absPath, absRoot, ec);
    if (ec)
        rel = path.lexically_normal();
    return NormalizeSlashes(rel.generic_string());
}

std::string StableHashBytes(const std::string& bytes)
{
    uint64_t hash = 14695981039346656037ull;
    HashAppend(hash, bytes);
    return Hex64(hash);
}

std::string StableFileFingerprint(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return {};
    std::ostringstream bytes;
    bytes << file.rdbuf();
    std::string normalized = bytes.str();
    normalized.erase(std::remove(normalized.begin(), normalized.end(), '\r'), normalized.end());
    return StableHashBytes(normalized);
}

std::string StableHashCompiledGraph(const CompiledModelGraph& graph)
{
    std::ostringstream out;
    for (const auto& modelId : graph.ModelIds())
    {
        out << "model:" << modelId.size() << ':' << modelId << '\n';
        for (const CompiledInstance* inst : graph.AllOf(modelId))
        {
            out << "instance:" << inst->instanceId.size() << ':' << inst->instanceId << '\n';
            for (const auto& [fieldId, value] : inst->fields)
            {
                out << "field:" << fieldId.size() << ':' << fieldId << '=';
                AppendCompiledValue(out, value, graph);
                out << '\n';
            }
        }
    }
    return StableHashBytes(out.str());
}

std::string StableHashDependencyManifest(const DependencyManifest& manifest)
{
    std::ostringstream out;
    auto appendRecord = [&](const char* group, const DependencyRecord& record) {
        out << group << ':' << record.logicalId << ':' << record.normalizedPath
            << ':' << record.fingerprint << '\n';
    };
    for (const auto& record : manifest.directSchemas)
        appendRecord("schema", record);
    for (const auto& record : manifest.transitiveSchemas)
        appendRecord("transitive", record);
    for (const auto& record : manifest.dataFiles)
        appendRecord("data", record);
    for (const auto& edge : manifest.edges)
        out << "edge:" << edge.fromLogicalId << "->" << edge.toLogicalId << '\n';
    return StableHashBytes(out.str());
}

} // namespace SDE
