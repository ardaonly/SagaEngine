/// @file ComposerArtifactSummary.cpp
/// @brief Generated editor composition artifact summary reader implementation.

#include "SagaEditor/Composer/ComposerArtifactSummary.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace SagaEditor
{
namespace
{

using Json = nlohmann::json;

[[nodiscard]] Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path);
    Json json;
    input >> json;
    return json;
}

[[nodiscard]] int ArrayCount(const Json& json, const char* key)
{
    const auto it = json.find(key);
    if (it == json.end() || !it->is_array())
    {
        return 0;
    }
    return static_cast<int>(it->size());
}

} // namespace

ComposerArtifactSummary LoadComposerArtifactSummary(
    const ComposerWorkspacePaths& paths)
{
    ComposerArtifactSummary summary;
    summary.manifestFound = std::filesystem::exists(paths.manifestPath);
    summary.artifactFound = std::filesystem::exists(paths.artifactPath);
    summary.diagnosticsFound = std::filesystem::exists(paths.diagnosticsPath);
    summary.sourceMapFound = std::filesystem::exists(paths.sourceMapPath);
    summary.dependenciesFound = std::filesystem::exists(paths.dependenciesPath);

    try
    {
        if (summary.manifestFound)
        {
            const Json manifest = ReadJson(paths.manifestPath);
            summary.compositionId =
                manifest.value("compositionId", std::string{});
            summary.artifactHash =
                manifest.value("artifactHash", std::string{});
        }
        if (summary.artifactFound)
        {
            const Json artifact = ReadJson(paths.artifactPath);
            summary.panelCount = ArrayCount(artifact, "panels");
            summary.actionCount = ArrayCount(artifact, "actions");
            summary.menuCount = ArrayCount(artifact, "menus");
            summary.toolbarCount = ArrayCount(artifact, "toolbars");
            summary.shortcutCount = ArrayCount(artifact, "shortcuts");
            summary.workspaceCount = ArrayCount(artifact, "workspaces");
            summary.modeCount = ArrayCount(artifact, "editorModes");
        }
        summary.message = summary.manifestFound || summary.artifactFound
            ? "Generated composition outputs loaded."
            : "No compiled composition artifact found. Run Build Composition.";
    }
    catch (const std::exception& exception)
    {
        summary.message = exception.what();
    }
    return summary;
}

} // namespace SagaEditor
