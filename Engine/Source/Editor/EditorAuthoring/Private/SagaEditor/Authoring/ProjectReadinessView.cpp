#include "SagaEditor/Authoring/ProjectReadinessView.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace SagaEditor::Authoring
{
namespace
{

using Json = nlohmann::json;
namespace fs = std::filesystem;

[[nodiscard]] fs::path ResolveProjectManifestPath(const fs::path& path)
{
    if (!fs::is_directory(path))
    {
        return path;
    }

    std::vector<fs::path> candidates;
    for (const auto& entry : fs::directory_iterator(path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".sagaproj")
        {
            candidates.push_back(entry.path());
        }
    }
    std::sort(candidates.begin(), candidates.end());
    if (candidates.empty())
    {
        return path / ".sagaproj";
    }
    return candidates.front();
}

[[nodiscard]] std::string StringField(const Json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
    {
        return {};
    }
    return it->get<std::string>();
}

[[nodiscard]] fs::path ResolveProjectPath(const fs::path& root,
                                          const std::string& value)
{
    fs::path path(value);
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }
    return (root / path).lexically_normal();
}

void LoadPathReferences(const Json& rootJson,
                        const char* key,
                        const fs::path& projectRoot,
                        std::vector<ProjectReadinessPathReference>& out)
{
    const auto it = rootJson.find(key);
    if (it == rootJson.end() || !it->is_array())
    {
        return;
    }

    for (const Json& item : *it)
    {
        if (!item.is_object())
        {
            continue;
        }
        const std::string path = StringField(item, "path");
        if (path.empty())
        {
            continue;
        }
        out.push_back(ProjectReadinessPathReference{
            StringField(item, "id"),
            ResolveProjectPath(projectRoot, path),
        });
    }
}

void AddSubsetFailure(ProjectReadinessView& view,
                      const fs::path& manifestPath,
                      const std::string& message)
{
    view.ok = false;
    view.diagnostics.push_back(AuthoringDiagnostic{
        "Editor.Project.EditorSubsetParseFailed",
        "editor subset parse failed: " + message,
        manifestPath.string(),
    });
}

} // namespace

ProjectReadinessView LoadProjectReadinessView(
    const std::filesystem::path& manifestPath)
{
    ProjectReadinessView view;
    view.manifestPath = fs::absolute(ResolveProjectManifestPath(manifestPath));
    view.projectRoot = view.manifestPath.parent_path();

    std::ifstream input(view.manifestPath);
    if (!input.is_open())
    {
        AddSubsetFailure(view, view.manifestPath, "manifest is missing");
        return view;
    }

    Json json;
    try
    {
        input >> json;
    }
    catch (const Json::exception& error)
    {
        AddSubsetFailure(view, view.manifestPath, error.what());
        return view;
    }

    if (!json.is_object())
    {
        AddSubsetFailure(view, view.manifestPath, "manifest root is not an object");
        return view;
    }

    view.projectId = StringField(json, "projectId");
    view.displayName = StringField(json, "displayName");
    if (view.projectId.empty() || view.displayName.empty())
    {
        AddSubsetFailure(
            view,
            view.manifestPath,
            "projectId and displayName are required for editor display");
        return view;
    }

    const auto pathsIt = json.find("paths");
    if (pathsIt != json.end() && pathsIt->is_object())
    {
        const std::string diagnostics = StringField(*pathsIt, "diagnostics");
        const std::string generatedReports =
            StringField(*pathsIt, "generatedReports");
        if (!diagnostics.empty())
        {
            view.diagnosticsPath =
                ResolveProjectPath(view.projectRoot, diagnostics);
        }
        if (!generatedReports.empty())
        {
            view.generatedReportsPath =
                ResolveProjectPath(view.projectRoot, generatedReports);
        }
    }

    LoadPathReferences(json, "scriptFolders", view.projectRoot, view.scriptFolders);
    LoadPathReferences(json, "launchProfiles", view.projectRoot, view.launchProfiles);
    LoadPathReferences(json, "packageProfiles", view.projectRoot, view.packageProfiles);

    view.ok = true;
    return view;
}

} // namespace SagaEditor::Authoring
