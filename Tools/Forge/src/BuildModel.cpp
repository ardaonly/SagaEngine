/// @file BuildModel.cpp
/// @brief BuildModel construction from a loaded Manifest.

#include "Forge/BuildModel.h"
#include "Forge/Manifest.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace Forge
{

namespace
{

std::string Trim(std::string value)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(),
                value.end());
    return value;
}

std::string ReadRootVersionFile()
{
    std::ifstream input("VERSION");
    if (!input)
    {
        return {};
    }

    std::string version;
    std::getline(input, version);
    return Trim(std::move(version));
}

void ReplaceAll(std::string& value,
                const std::string& token,
                const std::string& replacement)
{
    if (token.empty())
    {
        return;
    }

    std::size_t position = 0;
    while ((position = value.find(token, position)) != std::string::npos)
    {
        value.replace(position, token.size(), replacement);
        position += replacement.size();
    }
}

} // namespace

BuildModel BuildModel::FromManifest(const Manifest& m)
{
    BuildModel model;

    model.projectName    = m.Get("project", "name");
    model.projectVersion = m.Get("project", "version");
    if (model.projectVersion.empty())
    {
        model.projectVersion = ReadRootVersionFile();
    }

    model.toolchain.cmake    = m.Get("toolchain", "cmake");
    model.toolchain.conan    = m.Get("toolchain", "conan");
    model.toolchain.compiler = m.Get("toolchain", "compiler");

    model.build.sourceDir = m.Get("build", "source", ".");
    model.build.buildDir  = m.Get("build", "build",  "build");
    model.build.preset    = m.Get("build", "preset");
    ReplaceAll(model.build.sourceDir, "${version}", model.projectVersion);
    ReplaceAll(model.build.buildDir, "${version}", model.projectVersion);
    ReplaceAll(model.build.preset, "${version}", model.projectVersion);

    const std::string jobsStr = m.Get("build", "jobs");
    if (!jobsStr.empty())
    {
        try { model.build.jobs = static_cast<uint32_t>(std::stoul(jobsStr)); }
        catch (...) { model.build.jobs = 0; }
    }

    for (const auto& [name, version] : m.Deps())
        model.deps.push_back({name, version});

    return model;
}

} // namespace Forge
