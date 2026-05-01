/// @file Resolver.cpp
/// @brief Implementation of name → executable-path resolution. UTF-8 throughout.

#include "SagaTools/Resolver.h"
#include "SagaTools/Encoding.h"

#include <filesystem>
#include <sstream>

namespace SagaTools
{

namespace
{

// ─── Path helpers (UTF-8 aware) ──────────────────────────────────────────────

bool FileExists(const std::string& utf8Path)
{
    std::error_code ec;
    const auto p = PathFromUtf8(utf8Path);
    return std::filesystem::exists(p, ec)
        && std::filesystem::is_regular_file(p, ec);
}

bool ContainsPathSeparator(const std::string& s)
{
    return s.find('/') != std::string::npos || s.find('\\') != std::string::npos;
}

#if defined(_WIN32)
constexpr char kPathSeparator = ';';
const std::vector<std::string> kExeSuffixes = {".exe", ".bat", ".cmd", ""};
#else
constexpr char kPathSeparator = ':';
const std::vector<std::string> kExeSuffixes = {""};
#endif

/// `${VAR}` substitution against the process environment. Unset vars expand
/// to the empty string. Lookup goes through `GetEnvUtf8` so values containing
/// non-ASCII characters survive on Windows (where `getenv` returns ANSI).
std::string ExpandEnv(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); )
    {
        if (in[i] == '$' && i + 1 < in.size() && in[i + 1] == '{')
        {
            const std::size_t close = in.find('}', i + 2);
            if (close != std::string::npos)
            {
                const std::string var = in.substr(i + 2, close - (i + 2));
                out.append(GetEnvUtf8(var));
                i = close + 1;
                continue;
            }
        }
        out.push_back(in[i++]);
    }
    return out;
}

std::string SearchOnPath(const std::string& executable)
{
    const std::string env = GetEnvUtf8("PATH");
    if (env.empty()) return {};
    std::stringstream ss(env);
    std::string dir;
    while (std::getline(ss, dir, kPathSeparator))
    {
        if (dir.empty()) continue;
        for (const auto& suffix : kExeSuffixes)
        {
            const std::string candidate = dir + "/" + executable + suffix;
            if (FileExists(candidate))
                return PathToUtf8(std::filesystem::absolute(PathFromUtf8(candidate)));
        }
    }
    return {};
}

std::string AbsoluteFromRegistry(const std::string& baseDir, const std::string& path)
{
    namespace fs = std::filesystem;
    const fs::path p = PathFromUtf8(path);
    if (p.is_absolute()) return PathToUtf8(p);
    return PathToUtf8(fs::weakly_canonical(PathFromUtf8(baseDir) / p));
}

} // namespace

// ─── Resolution ───────────────────────────────────────────────────────────────

Resolution Resolver::Resolve(const Registry& registry, const std::string& name) noexcept
{
    Resolution out;

    const std::string* raw = registry.FindToolRaw(name);
    if (!raw) return out;

    out.expandedValue = ExpandEnv(*raw);
    if (out.expandedValue.empty()) return out;

    if (ContainsPathSeparator(out.expandedValue))
    {
        const std::string abs = AbsoluteFromRegistry(registry.BaseDir(), out.expandedValue);
        if (FileExists(abs))
        {
            out.available    = true;
            out.resolvedPath = abs;
            out.source       = "registry-path";
        }
        return out;
    }

    const std::string hit = SearchOnPath(out.expandedValue);
    if (!hit.empty())
    {
        out.available    = true;
        out.resolvedPath = hit;
        out.source       = "PATH";
    }
    return out;
}

} // namespace SagaTools
