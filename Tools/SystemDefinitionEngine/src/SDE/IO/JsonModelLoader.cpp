/// @file JsonModelLoader.cpp
/// @brief JSON-backed model instance and definition loading via nlohmann/json.
///
/// nlohmann/json is isolated entirely to this translation unit.

#include "SDE/IO/JsonModelLoader.h"
#include "SDE/Model/TypeNode.h"
#include "SDE/Model/EnumDefinition.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace SDE
{
namespace
{

// ─── JsonToRaw ────────────────────────────────────────────────────────────────

/// Recursively convert a nlohmann::json value to a RawValue.
/// Line numbers are unavailable from the nlohmann DOM; locations carry the file path only.
RawValue JsonToRaw(const nlohmann::json& j, const std::string& file)
{
    RawValue rv;
    rv.location.file = file;

    if (j.is_null())
    {
        rv.data = RawNull{};
    }
    else if (j.is_boolean())
    {
        rv.data = static_cast<RawBool>(j.get<bool>());
    }
    else if (j.is_number_integer())
    {
        rv.data = static_cast<RawInteger>(j.get<int64_t>());
    }
    else if (j.is_number_float())
    {
        rv.data = static_cast<RawNumber>(j.get<double>());
    }
    else if (j.is_string())
    {
        rv.data = j.get<std::string>();
    }
    else if (j.is_array())
    {
        RawArray arr;
        for (const auto& elem : j)
            arr.elements.push_back(JsonToRaw(elem, file));
        rv.data = std::move(arr);
    }
    else if (j.is_object())
    {
        RawObject obj;
        for (const auto& [key, val] : j.items())
            obj.fields[key] = JsonToRaw(val, file);
        rv.data = std::move(obj);
    }

    return rv;
}

// ─── ParseTypeString ──────────────────────────────────────────────────────────

/// Parse a type string ("Ref<Item>", "List<Number>", "Integer", ...) into a TypeNodeId.
/// Registers new nodes in the provided TypeRegistry as needed.
TypeNodeId ParseTypeString(const std::string& s, TypeRegistry& types,
                            std::vector<Diagnostic>& out, const SourceLocation& loc)
{
    if (s == "Number")  return types.Primitive(TypeKind::Number);
    if (s == "Integer") return types.Primitive(TypeKind::Integer);
    if (s == "Text")    return types.Primitive(TypeKind::Text);
    if (s == "Boolean") return types.Primitive(TypeKind::Boolean);
    if (s == "Color")   return types.Primitive(TypeKind::Color);

    // "Ref<ModelId>"
    if (s.substr(0, 4) == "Ref<" && s.back() == '>')
        return types.Ref(s.substr(4, s.size() - 5));

    // "Enum<EnumId>"
    if (s.substr(0, 5) == "Enum<" && s.back() == '>')
        return types.Enum(s.substr(5, s.size() - 6));

    // "List<T>"
    if (s.substr(0, 5) == "List<" && s.back() == '>')
    {
        std::string inner = s.substr(5, s.size() - 6);
        TypeNodeId elem   = ParseTypeString(inner, types, out, loc);
        if (elem == kInvalidTypeNodeId)
            return kInvalidTypeNodeId;
        return types.List(elem);
    }

    // "Map<K,V>" — find the comma not inside angle brackets.
    if (s.substr(0, 4) == "Map<" && s.back() == '>')
    {
        std::string inner  = s.substr(4, s.size() - 5);
        int         depth  = 0;
        std::size_t comma  = std::string::npos;
        for (std::size_t i = 0; i < inner.size(); ++i)
        {
            if (inner[i] == '<') ++depth;
            else if (inner[i] == '>') --depth;
            else if (inner[i] == ',' && depth == 0) { comma = i; break; }
        }
        if (comma == std::string::npos)
        {
            out.push_back(Diagnostic::MakeError(loc, "SDE_BAD_TYPE",
                "Malformed Map type string: '" + s + "'."));
            return kInvalidTypeNodeId;
        }
        TypeNodeId k = ParseTypeString(inner.substr(0, comma), types, out, loc);
        TypeNodeId v = ParseTypeString(inner.substr(comma + 1), types, out, loc);
        if (k == kInvalidTypeNodeId || v == kInvalidTypeNodeId)
            return kInvalidTypeNodeId;
        return types.Map(k, v);
    }

    out.push_back(Diagnostic::MakeError(loc, "SDE_UNKNOWN_TYPE",
        "Unknown type string: '" + s + "'."));
    return kInvalidTypeNodeId;
}

// ─── ParseFieldDefinition ────────────────────────────────────────────────────

FieldDefinition ParseFieldDefinition(const nlohmann::json& obj,
                                      TypeRegistry&          types,
                                      const std::string&     file,
                                      std::vector<Diagnostic>& out)
{
    FieldDefinition fd;
    SourceLocation  loc{file, 0, 0};

    if (obj.contains("id") && obj["id"].is_string())
        fd.id = obj["id"].get<std::string>();
    if (obj.contains("displayName") && obj["displayName"].is_string())
        fd.displayName = obj["displayName"].get<std::string>();
    if (obj.contains("default") && obj["default"].is_string())
        fd.defaultVal = obj["default"].get<std::string>();

    if (obj.contains("presence"))
    {
        std::string p = obj["presence"].get<std::string>();
        fd.presence   = (p == "optional") ? FieldPresence::Optional : FieldPresence::Required;
    }

    if (obj.contains("type") && obj["type"].is_string())
    {
        std::string ts = obj["type"].get<std::string>();
        fd.type = ParseTypeString(ts, types, out, loc);
    }
    else
    {
        out.push_back(Diagnostic::MakeError(loc, "SDE_MISSING_TYPE",
            "Field '" + fd.id + "' has no type declaration."));
    }

    return fd;
}

} // namespace

// ─── JsonModelLoader::Load ────────────────────────────────────────────────────

std::vector<ModelInstance> JsonModelLoader::Load(
    const std::string&       path,
    std::vector<Diagnostic>& outDiagnostics) const
{
    SourceLocation loc{path, 0, 0};

    std::ifstream file(path);
    if (!file.is_open())
    {
        outDiagnostics.push_back(Diagnostic::MakeFatal(loc, "SDE_IO_ERROR",
            "Cannot open file: '" + path + "'."));
        return {};
    }

    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(file, nullptr, /*exceptions=*/true,
                                     /*ignore_comments=*/true);
    }
    catch (const nlohmann::json::parse_error& e)
    {
        outDiagnostics.push_back(Diagnostic::MakeFatal(loc, "SDE_PARSE_ERROR",
            std::string("JSON parse error in '") + path + "': " + e.what()));
        return {};
    }

    if (!root.is_object() || !root.contains("modelId") || !root.contains("data"))
    {
        outDiagnostics.push_back(Diagnostic::MakeFatal(loc, "SDE_BAD_FORMAT",
            "File '" + path + "' must be { \"modelId\": \"...\", \"data\": [...] }."));
        return {};
    }

    std::string           modelId  = root["modelId"].get<std::string>();
    const nlohmann::json& dataArr  = root["data"];

    if (!dataArr.is_array())
    {
        outDiagnostics.push_back(Diagnostic::MakeFatal(loc, "SDE_BAD_FORMAT",
            "'data' field in '" + path + "' must be an array."));
        return {};
    }

    std::vector<ModelInstance> instances;
    instances.reserve(dataArr.size());

    for (const auto& obj : dataArr)
    {
        if (!obj.is_object())
        {
            outDiagnostics.push_back(Diagnostic::MakeError(loc, "SDE_BAD_FORMAT",
                "Each entry in 'data' must be a JSON object."));
            continue;
        }

        ModelInstance inst;
        inst.modelId    = modelId;
        inst.sourceFile = path;
        inst.origin     = loc;

        if (obj.contains("id") && obj["id"].is_string())
            inst.instanceId = obj["id"].get<std::string>();

        for (const auto& [key, val] : obj.items())
            inst.fields[key] = JsonToRaw(val, path);

        instances.push_back(std::move(inst));
    }

    return instances;
}

// ─── JsonModelLoader::LoadDefinition ─────────────────────────────────────────

std::optional<ModelDefinition> JsonModelLoader::LoadDefinition(
    const std::string&       path,
    TypeRegistry&            types,
    std::vector<Diagnostic>& outDiagnostics)
{
    SourceLocation loc{path, 0, 0};

    std::ifstream file(path);
    if (!file.is_open())
    {
        outDiagnostics.push_back(Diagnostic::MakeFatal(loc, "SDE_IO_ERROR",
            "Cannot open schema file: '" + path + "'."));
        return std::nullopt;
    }

    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(file, nullptr, true, true);
    }
    catch (const nlohmann::json::parse_error& e)
    {
        outDiagnostics.push_back(Diagnostic::MakeFatal(loc, "SDE_PARSE_ERROR",
            std::string("JSON parse error in '") + path + "': " + e.what()));
        return std::nullopt;
    }

    ModelDefinition def;

    if (root.contains("id") && root["id"].is_string())
        def.id = root["id"].get<std::string>();
    if (root.contains("displayName") && root["displayName"].is_string())
        def.displayName = root["displayName"].get<std::string>();
    if (root.contains("schemaVersion") && root["schemaVersion"].is_number_integer())
        def.schemaVersion = root["schemaVersion"].get<int>();

    if (root.contains("fields") && root["fields"].is_array())
    {
        for (const auto& fobj : root["fields"])
            def.fields.push_back(ParseFieldDefinition(fobj, types, path, outDiagnostics));
    }

    return def;
}

} // namespace SDE
