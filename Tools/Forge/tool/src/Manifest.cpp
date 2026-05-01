/// @file Manifest.cpp
/// @brief Line-based forge.toml parser and serializer.

#include "Forge/Manifest.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace Forge
{

namespace
{

// ─── String helpers ──────────────────────────────────────────────────────────

std::string Trim(std::string s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

/// Strip surrounding `"…"` or `'…'`. Bare tokens are returned as-is.
std::string Unquote(std::string s)
{
    if (s.size() >= 2 &&
        ((s.front() == '"' && s.back() == '"') ||
         (s.front() == '\'' && s.back() == '\'')))
    {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

/// `key = value` — split on the first '='. Returns false on malformed lines.
bool SplitKeyValue(const std::string& line, std::string& outKey, std::string& outValue)
{
    const auto eq = line.find('=');
    if (eq == std::string::npos) return false;
    outKey   = Trim(line.substr(0, eq));
    outValue = Unquote(Trim(line.substr(eq + 1)));
    return !outKey.empty();
}

/// `[name]` → `name`. Returns empty when not a section header.
std::string ParseHeader(const std::string& line)
{
    if (line.size() < 2 || line.front() != '[' || line.back() != ']') return {};
    return Trim(line.substr(1, line.size() - 2));
}

/// Drop everything after an unquoted `#`. Quoted strings are preserved.
std::string StripComment(const std::string& line)
{
    bool inSingle = false;
    bool inDouble = false;
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        const char c = line[i];
        if      (c == '"'  && !inSingle) inDouble = !inDouble;
        else if (c == '\'' && !inDouble) inSingle = !inSingle;
        else if (c == '#'  && !inDouble && !inSingle) return line.substr(0, i);
    }
    return line;
}

} // namespace

// ─── Loading ────────────────────────────────────────────────────────────────

std::optional<Manifest> Manifest::LoadFromFile(const std::string& path, std::string* outError)
{
    std::ifstream in(path);
    if (!in)
    {
        if (outError) *outError = "cannot open manifest: " + path;
        return std::nullopt;
    }

    Manifest m;
    std::string section;
    std::string raw;
    while (std::getline(in, raw))
    {
        const std::string line = Trim(StripComment(raw));
        if (line.empty()) continue;

        if (const std::string header = ParseHeader(line); !header.empty())
        {
            section = header;
            continue;
        }

        std::string key, value;
        if (!SplitKeyValue(line, key, value)) continue;

        if (section == "deps")
        {
            m.AddDep(key, std::move(value));
        }
        else
        {
            m.Set(section, key, std::move(value));
        }
    }
    return m;
}

// ─── Mutation ───────────────────────────────────────────────────────────────

void Manifest::Set(const std::string& section, const std::string& key, std::string value)
{
    mScalars[section][key] = std::move(value);
}

void Manifest::AddDep(const std::string& name, std::string version)
{
    for (auto& [n, v] : mDeps)
    {
        if (n == name) { v = std::move(version); return; }
    }
    mDeps.emplace_back(name, std::move(version));
}

// ─── Lookup ─────────────────────────────────────────────────────────────────

std::string Manifest::Get(const std::string& section,
                          const std::string& key,
                          const std::string& fallback) const
{
    if (auto it = mScalars.find(section); it != mScalars.end())
    {
        if (auto kv = it->second.find(key); kv != it->second.end())
            return kv->second;
    }
    return fallback;
}

// ─── Serialisation ──────────────────────────────────────────────────────────

namespace
{

void EmitSection(std::ostream& os, const std::string& name,
                 const std::map<std::string, std::string>& kv)
{
    if (kv.empty()) return;
    os << "[" << name << "]\n";
    for (const auto& [k, v] : kv) os << k << " = \"" << v << "\"\n";
    os << "\n";
}

} // namespace

bool Manifest::SaveToFile(const std::string& path, std::string* outError) const
{
    std::ofstream out(path, std::ios::trunc);
    if (!out)
    {
        if (outError) *outError = "cannot open for write: " + path;
        return false;
    }

    out << "# forge.toml — generated / maintained by `forge`. Hand-editing is fine.\n\n";

    // Canonical section order. Anything stored under an unknown section is
    // emitted last to avoid losing user-added data.
    static constexpr const char* kKnownOrder[] = {"project", "toolchain", "build"};
    for (const char* section : kKnownOrder)
    {
        if (auto it = mScalars.find(section); it != mScalars.end())
            EmitSection(out, section, it->second);
    }
    for (const auto& [name, kv] : mScalars)
    {
        bool known = false;
        for (const char* k : kKnownOrder) if (name == k) { known = true; break; }
        if (!known) EmitSection(out, name, kv);
    }

    if (!mDeps.empty())
    {
        out << "[deps]\n";
        for (const auto& [name, version] : mDeps)
            out << name << " = \"" << version << "\"\n";
        out << "\n";
    }
    return true;
}

} // namespace Forge
