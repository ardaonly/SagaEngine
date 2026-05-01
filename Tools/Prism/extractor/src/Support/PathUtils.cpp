/// @file Support/PathUtils.cpp
/// @brief Implementation of path normalization utilities.

#include "PathUtils.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace prism::support {

std::string MakeRelativePosix(const std::string& abs_path,
                               const std::string& repo_root)
{
    if (abs_path.empty() || repo_root.empty())
        return abs_path;

    try
    {
        const fs::path rel = fs::relative(
            fs::path(abs_path),
            fs::path(repo_root)
        );
        return NormalizePosix(rel.generic_string());
    }
    catch (const std::exception&)
    {
        // Path is outside the tree or filesystem error — return as-is.
        return NormalizePosix(abs_path);
    }
}

std::string NormalizePosix(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

bool IsSystemPath(const std::string& path)
{
    // Common system-header prefixes across Linux, macOS, and MSVC.
    static const char* const k_prefixes[] = {
        "/usr/",
        "/Library/",
        "/Applications/Xcode",
        "C:/Program Files",
        "C:\\Program Files",
        "<built-in>",
        "<scratch space>",
    };

    for (const char* prefix : k_prefixes)
        if (path.find(prefix) == 0)
            return true;

    return false;
}

} // namespace prism::support
