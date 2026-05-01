/// @file JsonEmitter.cpp
/// @brief Hand-rolled JSON serializer for ExtractionResult.
///        No third-party JSON library is required — the output schema is simple
///        and the emitter is the only consumer of this format.

#include "JsonEmitter.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace prism {

// ─── Escaping ─────────────────────────────────────────────────────────────────

static std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (const char c : s)
    {
        switch (c)
        {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out += buf;
            }
            else
            {
                out += c;
            }
        }
    }
    return out;
}

static std::string Q(const std::string& s)
{
    return "\"" + JsonEscape(s) + "\"";
}

// ─── Primitive Builders ───────────────────────────────────────────────────────

static std::string Indent(int depth)
{
    return std::string(static_cast<size_t>(depth) * 2, ' ');
}

static std::string StringArray(const std::vector<std::string>& v, int depth)
{
    if (v.empty()) return "[]";

    std::string out = "[\n";
    const std::string ind = Indent(depth + 1);
    for (size_t i = 0; i < v.size(); ++i)
    {
        out += ind + Q(v[i]);
        if (i + 1 < v.size()) out += ',';
        out += '\n';
    }
    out += Indent(depth) + ']';
    return out;
}

static std::string IsoTimestamp()
{
    const auto now  = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ─── Record Serializers ───────────────────────────────────────────────────────

static std::string EmitLocation(const Location& loc, int depth)
{
    const std::string i1 = Indent(depth + 1);
    std::string out = "{\n";
    out += i1 + Q("file")   + ": " + Q(loc.file)                     + ",\n";
    out += i1 + Q("line")   + ": " + std::to_string(loc.line)        + ",\n";
    out += i1 + Q("column") + ": " + std::to_string(loc.column)      + '\n';
    out += Indent(depth) + '}';
    return out;
}

static std::string EmitExtent(const Extent& e, int depth)
{
    const std::string i1 = Indent(depth + 1);
    std::string out = "{\n";
    out += i1 + Q("start_line")   + ": " + std::to_string(e.start_line)   + ",\n";
    out += i1 + Q("start_column") + ": " + std::to_string(e.start_column) + ",\n";
    out += i1 + Q("end_line")     + ": " + std::to_string(e.end_line)     + ",\n";
    out += i1 + Q("end_column")   + ": " + std::to_string(e.end_column)   + '\n';
    out += Indent(depth) + '}';
    return out;
}

static std::string EmitSymbol(const SymbolRecord& s, int depth)
{
    const std::string i1 = Indent(depth + 1);
    std::string out = "{\n";
    out += i1 + Q("id")             + ": " + Q(s.id)                            + ",\n";
    out += i1 + Q("usr")            + ": " + Q(s.usr)                           + ",\n";
    out += i1 + Q("name")           + ": " + Q(s.name)                          + ",\n";
    out += i1 + Q("qualified_name") + ": " + Q(s.qualified_name)                + ",\n";
    out += i1 + Q("display_name")   + ": " + Q(s.display_name)                  + ",\n";
    out += i1 + Q("kind")           + ": " + Q(s.kind)                          + ",\n";
    out += i1 + Q("access")         + ": " + Q(s.access)                        + ",\n";
    out += i1 + Q("is_definition")  + ": " + (s.is_definition ? "true" : "false") + ",\n";
    out += i1 + Q("brief")          + ": " + Q(s.brief)                         + ",\n";
    out += i1 + Q("raw_comment")    + ": " + Q(s.raw_comment)                   + ",\n";
    out += i1 + Q("signature")      + ": " + Q(s.signature)                     + ",\n";
    out += i1 + Q("result_type")    + ": " + Q(s.result_type)                   + ",\n";
    out += i1 + Q("type_spelling")  + ": " + Q(s.type_spelling)                 + ",\n";
    out += i1 + Q("location")       + ": " + EmitLocation(s.location, depth + 1) + ",\n";
    out += i1 + Q("extent")         + ": " + EmitExtent(s.extent,     depth + 1) + ",\n";
    out += i1 + Q("bases")          + ": " + StringArray(s.bases,     depth + 1) + ",\n";
    out += i1 + Q("deps")           + ": " + StringArray(s.deps,      depth + 1) + ",\n";
    out += i1 + Q("children")       + ": " + StringArray(s.children,  depth + 1) + '\n';
    out += Indent(depth) + '}';
    return out;
}

static std::string EmitFile(const FileRecord& f, int depth)
{
    const std::string i1 = Indent(depth + 1);
    std::string out = "{\n";
    out += i1 + Q("path")       + ": " + Q(f.path)                           + ",\n";
    out += i1 + Q("brief")      + ": " + Q(f.brief)                          + ",\n";
    out += i1 + Q("includes")   + ": " + StringArray(f.includes,   depth + 1) + ",\n";
    out += i1 + Q("symbol_ids") + ": " + StringArray(f.symbol_ids, depth + 1) + '\n';
    out += Indent(depth) + '}';
    return out;
}

// ─── Public API ───────────────────────────────────────────────────────────────

void EmitJson(const ExtractionResult&      result,
              const std::string&           repo_root,
              const std::filesystem::path& out_path)
{
    // Ensure the parent directory exists.
    std::error_code ec;
    std::filesystem::create_directories(out_path.parent_path(), ec);
    if (ec)
        throw std::runtime_error("Cannot create output directory: " + ec.message());

    std::ofstream out(out_path, std::ios::out | std::ios::trunc);
    if (!out)
        throw std::runtime_error("Cannot open output file: " + out_path.string());

    const std::string i1 = Indent(1);
    const std::string i2 = Indent(2);

    out << "{\n";
    out << i1 << Q("schema_version") << ": " << Q("1.0")             << ",\n";
    out << i1 << Q("generated_by")   << ": " << Q("prism-extract 1.0.0") << ",\n";
    out << i1 << Q("generated_at")   << ": " << Q(IsoTimestamp())    << ",\n";
    out << i1 << Q("repo_root")      << ": " << Q(repo_root)         << ",\n";

    // ── files ────────────────────────────────────────────────────────────────
    out << i1 << Q("files") << ": [\n";
    for (size_t i = 0; i < result.files.size(); ++i)
    {
        out << i2 << EmitFile(result.files[i], 2);
        if (i + 1 < result.files.size()) out << ',';
        out << '\n';
    }
    out << i1 << "],\n";

    // ── symbols ──────────────────────────────────────────────────────────────
    out << i1 << Q("symbols") << ": [\n";
    for (size_t i = 0; i < result.symbols.size(); ++i)
    {
        out << i2 << EmitSymbol(result.symbols[i], 2);
        if (i + 1 < result.symbols.size()) out << ',';
        out << '\n';
    }
    out << i1 << "]\n";

    out << "}\n";

    if (!out)
        throw std::runtime_error("Write failed: " + out_path.string());
}

} // namespace prism
