/// @file EnvProbe.cpp
/// @brief Tool version detection and strict-mode enforcement.

#include "Forge/EnvProbe.h"
#include "Forge/ProcessRunner.h"

#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace Forge
{

namespace
{

std::string FirstLine(const std::string& s)
{
    const auto nl = s.find('\n');
    return nl != std::string::npos ? s.substr(0, nl) : s;
}

/// Extract the first "X.Y" or "X.Y.Z" token from a version line.
std::string ExtractVersion(const std::string& line)
{
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(line[i]))) continue;
        std::size_t j = i;
        while (j < line.size() && (std::isdigit(static_cast<unsigned char>(line[j])) || line[j] == '.'))
            ++j;
        const std::string v = line.substr(i, j - i);
        if (v.find('.') != std::string::npos) return v; // must contain at least one dot
    }
    return {};
}

/// Return true if actual starts with pinned (e.g. pin="3.28" matches "3.28.1").
bool VersionSatisfies(const std::string& actual, const std::string& pinned)
{
    if (pinned.empty()) return true;
    if (actual.size() < pinned.size()) return false;
    if (actual.substr(0, pinned.size()) != pinned) return false;
    // The character immediately after the pin must be either '.', end-of-string,
    // or a non-digit — so "3.2" doesn't accidentally match "3.28".
    if (actual.size() > pinned.size())
    {
        const char next = actual[pinned.size()];
        if (std::isdigit(static_cast<unsigned char>(next))) return false;
    }
    return true;
}

} // namespace

// ─── EnvProbe ─────────────────────────────────────────────────────────────────

EnvReport EnvProbe::Detect(const ToolEnv& env)
{
    EnvReport r;

    auto probe = [](const std::string& exe) -> ToolInfo
    {
        ToolInfo info;
        info.name = exe;
        const std::string out = ProcessRunner::Capture(exe, {"--version"});
        info.found   = !out.empty();
        info.version = ExtractVersion(FirstLine(out));
        return info;
    };

    r.cmake = probe(env.cmake);
    r.conan = probe(env.conan);

    for (const char* cc : {"c++", "g++", "clang++"})
    {
        const std::string out = ProcessRunner::Capture(cc, {"--version"});
        if (!out.empty())
        {
            r.cxx.name    = cc;
            r.cxx.found   = true;
            r.cxx.version = ExtractVersion(FirstLine(out));
            break;
        }
    }

    return r;
}

void EnvProbe::PrintTable(const EnvReport& report, const BuildModel& model, std::ostream& os)
{
    auto row = [&](const ToolInfo& info, const std::string& pin)
    {
        const std::string status = !info.found
            ? "not found"
            : (!pin.empty() && !VersionSatisfies(info.version, pin))
                ? info.version + "  [mismatch: pin=" + pin + "]"
                : info.version + (pin.empty() ? "" : "  [ok]");
        os << "  " << std::left << std::setw(14) << info.name
           << status << "\n";
    };

    os << "forge env\n";
    row(report.cmake, model.toolchain.cmake);
    row(report.conan, model.toolchain.conan);
    if (report.cxx.found) row(report.cxx, model.toolchain.compiler);
}

void EnvProbe::PrintJson(const EnvReport& report, const BuildModel& model, std::ostream& os)
{
    auto escape = [](const std::string& s) { return s; }; // versions are safe
    auto entry  = [&](const ToolInfo& info, const std::string& pin, bool last)
    {
        const std::string ok = pin.empty() ? "null"
            : (VersionSatisfies(info.version, pin) ? "true" : "false");
        os << "    \"" << escape(info.name) << "\": {"
           << "\"found\": " << (info.found ? "true" : "false")
           << ", \"version\": \"" << escape(info.version) << "\""
           << ", \"pin\": \"" << escape(pin) << "\""
           << ", \"satisfied\": " << ok
           << "}" << (last ? "" : ",") << "\n";
    };

    os << "{\n";
    entry(report.cmake, model.toolchain.cmake, false);
    entry(report.conan, model.toolchain.conan, !report.cxx.found);
    if (report.cxx.found)
        entry(report.cxx, model.toolchain.compiler, true);
    os << "}\n";
}

bool EnvProbe::CheckStrict(const EnvReport& report, const BuildModel& model)
{
    bool ok = true;

    auto check = [&](const ToolInfo& info, const std::string& pin)
    {
        if (pin.empty()) return;
        if (!info.found)
        {
            std::cerr << "[forge] --strict: '" << info.name << "' not found on PATH\n";
            ok = false;
            return;
        }
        if (!VersionSatisfies(info.version, pin))
        {
            std::cerr << "[forge] --strict: " << info.name
                      << " version mismatch — required " << pin
                      << ", found " << info.version << "\n";
            ok = false;
        }
    };

    check(report.cmake, model.toolchain.cmake);
    check(report.conan, model.toolchain.conan);
    if (report.cxx.found)
        check(report.cxx, model.toolchain.compiler);

    return ok;
}

} // namespace Forge
