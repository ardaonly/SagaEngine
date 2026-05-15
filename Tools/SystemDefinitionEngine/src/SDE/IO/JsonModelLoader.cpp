/// @file JsonModelLoader.cpp
/// @brief JSON-backed model instance and definition loading via nlohmann/json.
///
/// nlohmann/json is isolated entirely to this translation unit.

#include "SDE/IO/JsonModelLoader.h"
#include "SDE/Model/EnumDefinition.h"
#include "SDE/Model/EnumRegistry.h"
#include "SDE/Model/TypeNode.h"
#include "SDE/Validation/Rule.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>

namespace SDE
{
namespace
{

// ─── JsonToRaw ────────────────────────────────────────────────────────────────

/// Recursively convert a nlohmann::json value to a RawValue.
/// Line numbers are unavailable from the nlohmann DOM; locations carry the file path only.
RawValue JsonToRaw(const nlohmann::json& j, SourceLocation loc)
{
    RawValue rv;
    rv.location = std::move(loc);

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
            arr.elements.push_back(JsonToRaw(elem, rv.location));
        rv.data = std::move(arr);
    }
    else if (j.is_object())
    {
        RawObject obj;
        for (const auto& [key, val] : j.items())
            obj.fields[key] = JsonToRaw(val, rv.location);
        rv.data = std::move(obj);
    }

    return rv;
}

SourceLocation ByteOffsetToLocation(const std::string& text,
                                    const std::string& file,
                                    std::size_t byteOffset)
{
    SourceLocation loc{file, 1, 1};
    for (std::size_t i = 0; i < text.size() && i < byteOffset; ++i)
    {
        if (text[i] == '\n')
        {
            ++loc.line;
            loc.column = 1;
        }
        else
        {
            ++loc.column;
        }
    }
    return loc;
}

SourceLocation FindTokenLocation(const std::string& text,
                                 const std::string& file,
                                 const std::string& token,
                                 std::size_t start = 0)
{
    const std::string needle = "\"" + token + "\"";
    const auto pos = text.find(needle, start);
    if (pos == std::string::npos)
        return {file, 0, 0};
    return ByteOffsetToLocation(text, file, pos);
}

Diagnostic MakeCategorizedFatal(SourceLocation loc,
                                DiagnosticCategory category,
                                std::string code,
                                std::string message)
{
    Diagnostic d = Diagnostic::MakeFatal(std::move(loc), std::move(code), std::move(message));
    d.category = category;
    return d;
}

Diagnostic MakeCategorizedError(SourceLocation loc,
                                DiagnosticCategory category,
                                std::string code,
                                std::string message)
{
    Diagnostic d = Diagnostic::MakeError(std::move(loc), std::move(code), std::move(message));
    d.category = category;
    return d;
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
    if (s.size() >= 5 && s.substr(0, 4) == "Ref<" && s.back() == '>')
        return types.Ref(s.substr(4, s.size() - 5));

    // "Enum<EnumId>"
    if (s.size() >= 7 && s.substr(0, 5) == "Enum<" && s.back() == '>')
        return types.Enum(s.substr(5, s.size() - 6));

    // "List<T>"
    if (s.size() >= 7 && s.substr(0, 5) == "List<" && s.back() == '>')
    {
        std::string inner = s.substr(5, s.size() - 6);
        TypeNodeId elem   = ParseTypeString(inner, types, out, loc);
        if (elem == kInvalidTypeNodeId)
            return kInvalidTypeNodeId;
        return types.List(elem);
    }

    // "Map<K,V>" — find the comma not inside angle brackets.
    if (s.size() >= 6 && s.substr(0, 4) == "Map<" && s.back() == '>')
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
            out.push_back(MakeCategorizedError(loc, DiagnosticCategory::Schema, "SDE_BAD_TYPE",
                "Malformed Map type string: '" + s + "'."));
            return kInvalidTypeNodeId;
        }
        TypeNodeId k = ParseTypeString(inner.substr(0, comma), types, out, loc);
        TypeNodeId v = ParseTypeString(inner.substr(comma + 1), types, out, loc);
        if (k == kInvalidTypeNodeId || v == kInvalidTypeNodeId)
            return kInvalidTypeNodeId;
        return types.Map(k, v);
    }

    out.push_back(MakeCategorizedError(loc, DiagnosticCategory::Schema, "SDE_UNKNOWN_TYPE",
        "Unknown type string: '" + s + "'."));
    return kInvalidTypeNodeId;
}

std::shared_ptr<Rule> ParseRuleDefinition(const nlohmann::json& obj,
                                          const std::string& file,
                                          std::vector<Diagnostic>& out)
{
    SourceLocation loc{file, 0, 0};
    if (!obj.is_object() || !obj.contains("kind") || !obj["kind"].is_string())
    {
        out.push_back(MakeCategorizedError(loc, DiagnosticCategory::Schema, "SDE_MISSING_RULE_KIND",
            "Validation rule is missing a string 'kind' field."));
        return nullptr;
    }

    const std::string kind = obj["kind"].get<std::string>();
    if (kind == "range")
    {
        if (!obj.contains("min") || !obj.contains("max") ||
            !obj["min"].is_number() || !obj["max"].is_number())
        {
            out.push_back(MakeCategorizedError(loc, DiagnosticCategory::Schema, "SDE_BAD_RULE",
                "Range rule requires numeric 'min' and 'max' fields."));
            return nullptr;
        }
        return std::make_shared<RangeRule>(obj["min"].get<double>(), obj["max"].get<double>());
    }
    if (kind == "regex")
    {
        if (!obj.contains("pattern") || !obj["pattern"].is_string())
        {
            out.push_back(MakeCategorizedError(loc, DiagnosticCategory::Schema, "SDE_BAD_RULE",
                "Regex rule requires a string 'pattern' field."));
            return nullptr;
        }
        return std::make_shared<RegexRule>(obj["pattern"].get<std::string>());
    }
    if (kind == "enumMember")
    {
        if (!obj.contains("enumId") || !obj["enumId"].is_string())
        {
            out.push_back(MakeCategorizedError(loc, DiagnosticCategory::Schema, "SDE_BAD_RULE",
                "Enum membership rule requires a string 'enumId' field."));
            return nullptr;
        }
        return std::make_shared<EnumMembershipRule>(obj["enumId"].get<std::string>());
    }
    if (kind == "refIntegrity")
        return std::make_shared<RefIntegrityRule>();

    out.push_back(MakeCategorizedError(loc, DiagnosticCategory::Schema, "SDE_UNKNOWN_RULE",
        "Unknown validation rule kind: '" + kind + "'."));
    return nullptr;
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
        out.push_back(MakeCategorizedError(loc, DiagnosticCategory::Schema, "SDE_MISSING_TYPE",
            "Field '" + fd.id + "' has no type declaration."));
    }

    if (obj.contains("rules") && obj["rules"].is_array())
    {
        for (const auto& ruleObj : obj["rules"])
        {
            if (auto rule = ParseRuleDefinition(ruleObj, file, out))
                fd.rules.push_back(std::move(rule));
        }
    }

    return fd;
}

void ParseEnums(const nlohmann::json& root,
                const std::string& file,
                EnumRegistry* enumRegistry,
                std::vector<Diagnostic>& out)
{
    if (enumRegistry == nullptr || !root.contains("enums") || !root["enums"].is_array())
        return;

    for (const auto& enumObj : root["enums"])
    {
        if (!enumObj.is_object())
            continue;

        EnumDefinition def;
        if (enumObj.contains("id") && enumObj["id"].is_string())
            def.id = enumObj["id"].get<std::string>();
        if (enumObj.contains("displayName") && enumObj["displayName"].is_string())
            def.displayName = enumObj["displayName"].get<std::string>();

        if (def.id.empty())
        {
            out.push_back(MakeCategorizedError({file, 0, 0}, DiagnosticCategory::Schema,
                "SDE_BAD_ENUM", "Enum declaration is missing a string 'id' field."));
            continue;
        }

        if (enumObj.contains("members") && enumObj["members"].is_array())
        {
            for (const auto& memberObj : enumObj["members"])
            {
                if (memberObj.is_string())
                {
                    def.members.push_back({memberObj.get<std::string>(), {}});
                }
                else if (memberObj.is_object() && memberObj.contains("name") && memberObj["name"].is_string())
                {
                    EnumMember member;
                    member.name = memberObj["name"].get<std::string>();
                    if (memberObj.contains("value") && memberObj["value"].is_string())
                        member.value = memberObj["value"].get<std::string>();
                    def.members.push_back(std::move(member));
                }
            }
        }

        enumRegistry->Register(std::move(def));
    }
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
        outDiagnostics.push_back(MakeCategorizedFatal(loc, DiagnosticCategory::IO,
            "SDE_IO_ERROR", "Cannot open file: '" + path + "'."));
        return {};
    }
    const std::string text((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(text, nullptr, /*exceptions=*/true,
                                     /*ignore_comments=*/true);
    }
    catch (const nlohmann::json::parse_error& e)
    {
        SourceLocation parseLoc = ByteOffsetToLocation(
            text, path, e.byte > 0 ? static_cast<std::size_t>(e.byte - 1) : 0);
        outDiagnostics.push_back(MakeCategorizedFatal(parseLoc, DiagnosticCategory::Parse,
            "SDE_PARSE_ERROR",
            std::string("JSON parse error in '") + path + "': " + e.what()));
        return {};
    }

    if (!root.is_object() || !root.contains("modelId") || !root.contains("data"))
    {
        outDiagnostics.push_back(MakeCategorizedFatal(loc, DiagnosticCategory::Schema, "SDE_BAD_FORMAT",
            "File '" + path + "' must be { \"modelId\": \"...\", \"data\": [...] }."));
        return {};
    }

    std::string           modelId  = root["modelId"].get<std::string>();
    const nlohmann::json& dataArr  = root["data"];

    if (!dataArr.is_array())
    {
        outDiagnostics.push_back(MakeCategorizedFatal(
            FindTokenLocation(text, path, "data"), DiagnosticCategory::Schema, "SDE_BAD_FORMAT",
            "'data' field in '" + path + "' must be an array."));
        return {};
    }

    const int dataVersion = root.contains("dataVersion") && root["dataVersion"].is_number_integer()
        ? root["dataVersion"].get<int>()
        : 1;

    std::vector<ModelInstance> instances;
    instances.reserve(dataArr.size());

    for (const auto& obj : dataArr)
    {
        if (!obj.is_object())
        {
            outDiagnostics.push_back(MakeCategorizedError(loc, DiagnosticCategory::Schema, "SDE_BAD_FORMAT",
                "Each entry in 'data' must be a JSON object."));
            continue;
        }

        ModelInstance inst;
        inst.modelId    = modelId;
        inst.sourceFile = path;
        inst.dataVersion = dataVersion;
        inst.origin     = loc;

        if (obj.contains("id") && obj["id"].is_string())
            inst.instanceId = obj["id"].get<std::string>();

        for (const auto& [key, val] : obj.items())
            inst.fields[key] = JsonToRaw(val, FindTokenLocation(text, path, key));

        instances.push_back(std::move(inst));
    }

    return instances;
}

// ─── JsonModelLoader::LoadDefinition ─────────────────────────────────────────

std::optional<ModelDefinition> JsonModelLoader::LoadDefinition(
    const std::string&       path,
    TypeRegistry&            types,
    std::vector<Diagnostic>& outDiagnostics,
    EnumRegistry*            enumRegistry)
{
    SourceLocation loc{path, 0, 0};

    std::ifstream file(path);
    if (!file.is_open())
    {
        outDiagnostics.push_back(MakeCategorizedFatal(loc, DiagnosticCategory::IO,
            "SDE_IO_ERROR", "Cannot open schema file: '" + path + "'."));
        return std::nullopt;
    }
    const std::string text((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(text, nullptr, true, true);
    }
    catch (const nlohmann::json::parse_error& e)
    {
        SourceLocation parseLoc = ByteOffsetToLocation(
            text, path, e.byte > 0 ? static_cast<std::size_t>(e.byte - 1) : 0);
        outDiagnostics.push_back(MakeCategorizedFatal(parseLoc, DiagnosticCategory::Parse,
            "SDE_PARSE_ERROR",
            std::string("JSON parse error in '") + path + "': " + e.what()));
        return std::nullopt;
    }

    ModelDefinition def;
    ParseEnums(root, path, enumRegistry, outDiagnostics);

    if (root.contains("id") && root["id"].is_string())
        def.id = root["id"].get<std::string>();
    if (root.contains("displayName") && root["displayName"].is_string())
        def.displayName = root["displayName"].get<std::string>();
    if (root.contains("schemaVersion") && root["schemaVersion"].is_number_integer())
        def.schemaVersion = root["schemaVersion"].get<int>();
    if (root.contains("imports") && root["imports"].is_array())
    {
        for (const auto& import : root["imports"])
        {
            if (import.is_string())
                def.imports.push_back(import.get<std::string>());
        }
    }

    if (root.contains("fields") && root["fields"].is_array())
    {
        for (const auto& fobj : root["fields"])
            def.fields.push_back(ParseFieldDefinition(fobj, types, path, outDiagnostics));
    }

    return def;
}

} // namespace SDE
