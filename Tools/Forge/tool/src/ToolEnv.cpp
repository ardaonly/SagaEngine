/// @file ToolEnv.cpp
/// @brief ToolEnv implementation — loads .forge overrides and manages the active instance.

#include "Forge/ToolEnv.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace Forge
{

namespace
{

std::string Trim(std::string s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

ToolEnv gActive; // process-wide singleton, all-defaults until SetActive() is called

} // namespace

// ─── ToolEnv ──────────────────────────────────────────────────────────────────

bool ToolEnv::IsDefault() const noexcept
{
    return cmake == "cmake" && conan == "conan" && ctest == "ctest" && clangFmt == "clang-format";
}

ToolEnv ToolEnv::LoadFromFile(const std::string& path, std::string* outError)
{
    std::ifstream in(path);
    if (!in)
    {
        if (outError) *outError = "cannot open: " + path;
        return ToolEnv{};
    }

    ToolEnv env;
    std::string line;
    while (std::getline(in, line))
    {
        const auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        const std::string key = Trim(line.substr(0, eq));
        const std::string val = Trim(line.substr(eq + 1));
        if (val.empty()) continue;

        if      (key == "CMAKE")        env.cmake    = val;
        else if (key == "CONAN")        env.conan    = val;
        else if (key == "CTEST")        env.ctest    = val;
        else if (key == "CLANG_FORMAT") env.clangFmt = val;
    }
    return env;
}

void ToolEnv::SetActive(ToolEnv env) noexcept { gActive = std::move(env); }
const ToolEnv& ToolEnv::Active() noexcept     { return gActive; }

} // namespace Forge
