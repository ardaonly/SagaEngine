/// @file BuildModel.cpp
/// @brief BuildModel construction from a loaded Manifest.

#include "Forge/BuildModel.h"
#include "Forge/Manifest.h"

namespace Forge
{

BuildModel BuildModel::FromManifest(const Manifest& m)
{
    BuildModel model;

    model.projectName    = m.Get("project", "name");
    model.projectVersion = m.Get("project", "version");

    model.toolchain.cmake    = m.Get("toolchain", "cmake");
    model.toolchain.conan    = m.Get("toolchain", "conan");
    model.toolchain.compiler = m.Get("toolchain", "compiler");

    model.build.sourceDir = m.Get("build", "source", ".");
    model.build.buildDir  = m.Get("build", "build",  "build");
    model.build.preset    = m.Get("build", "preset");

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
