/// @file BuildModelTests.cpp
/// @brief Coverage for Forge manifest lowering into BuildModel.

#include "Forge/BuildModel.h"
#include "Forge/Manifest.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{

bool Expect(bool condition, const char* label)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "[BuildModelTests] failed: " << label << "\n";
    return false;
}

} // namespace

int main()
{
    bool ok = true;

    const std::filesystem::path originalPath = std::filesystem::current_path();
    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "forge-build-model-tests";
    std::filesystem::remove_all(tempPath);
    std::filesystem::create_directories(tempPath);
    std::filesystem::current_path(tempPath);

    {
        std::ofstream version("VERSION", std::ios::trunc);
        version << "9.8.7\n";
    }

    Forge::Manifest manifest;
    manifest.Set("project", "name", "fixture");
    manifest.Set("build", "build", "build/RelWithDebInfo-${version}");

    const Forge::BuildModel model = Forge::BuildModel::FromManifest(manifest);
    ok &= Expect(model.projectVersion == "9.8.7",
                 "root VERSION is used when manifest version is absent");
    ok &= Expect(model.build.buildDir == "build/RelWithDebInfo-9.8.7",
                 "build dir expands project version");

    manifest.Set("project", "version", "1.2.3");
    const Forge::BuildModel explicitVersionModel =
        Forge::BuildModel::FromManifest(manifest);
    ok &= Expect(explicitVersionModel.projectVersion == "1.2.3",
                 "manifest version overrides VERSION file");
    ok &= Expect(explicitVersionModel.build.buildDir ==
                     "build/RelWithDebInfo-1.2.3",
                 "build dir expands manifest version");

    std::filesystem::current_path(originalPath);
    std::filesystem::remove_all(tempPath);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
