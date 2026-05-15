/// @file ArtifactReader.cpp
/// @brief Stable artifact manifest reader implementation.

#include "SDE/Artifacts/ArtifactReader.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace SDE
{
namespace
{

Diagnostic MakeReadError(const std::filesystem::path& path, std::string message)
{
    Diagnostic d = Diagnostic::MakeError({path.string(), 0, 0}, "SDE_ARTIFACT_READ_ERROR",
                                         std::move(message));
    d.category = DiagnosticCategory::IO;
    return d;
}

SemanticVersion VersionFromMajor(uint32_t major)
{
    return {major, 0, 0};
}

} // namespace

std::optional<ArtifactManifest> ArtifactReader::ReadManifest(
    const std::filesystem::path& path,
    std::vector<Diagnostic>& diagnostics) const
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        diagnostics.push_back(MakeReadError(path, "Cannot open artifact manifest."));
        return std::nullopt;
    }

    nlohmann::json json;
    try
    {
        input >> json;
    }
    catch (const nlohmann::json::exception& e)
    {
        diagnostics.push_back(MakeReadError(path, e.what()));
        return std::nullopt;
    }

    ArtifactManifest manifest;
    manifest.artifactVersion = VersionFromMajor(json.value("artifactVersion", 0u));
    manifest.compilerVersion = CurrentCompilerVersion();
    manifest.languageVersion = json.value("languageVersion", 0u);
    manifest.domain = json.value("domain", std::string{});
    manifest.kind = json.value("kind", std::string{});
    manifest.sourceHash = json.value("sourceHash", std::string{});
    manifest.dependencyHash = json.value("dependencyHash", std::string{});
    if (json.contains("modelHashes") && json["modelHashes"].is_object())
    {
        for (const auto& [key, value] : json["modelHashes"].items())
        {
            if (value.is_string())
                manifest.modelHashes[key] = value.get<std::string>();
        }
    }
    if (json.contains("outputs") && json["outputs"].is_array())
    {
        for (const auto& item : json["outputs"])
        {
            if (!item.is_object())
                continue;
            manifest.outputs.push_back({
                item.value("kind", std::string{}),
                item.value("path", std::string{}),
                item.value("hash", std::string{}),
            });
        }
    }
    return manifest;
}

} // namespace SDE
