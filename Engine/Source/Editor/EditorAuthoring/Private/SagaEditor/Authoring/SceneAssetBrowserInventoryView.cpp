#include "SagaEditor/Authoring/SceneAssetBrowserInventoryView.h"

#include <algorithm>

namespace SagaEditor::Authoring
{
namespace
{

namespace fs = std::filesystem;

[[nodiscard]] SceneAssetInventoryItemView Item(const std::string& id,
                                               const std::string& kind,
                                               const fs::path& path)
{
    const bool exists = fs::exists(path);
    return SceneAssetInventoryItemView{
        id,
        kind,
        path.lexically_normal(),
        exists,
        exists ? "Ready" : "Missing",
    };
}

[[nodiscard]] bool IsAssetManifest(const fs::path& path)
{
    const std::string name = path.filename().string();
    return name == "asset_manifest.json" ||
        name == "assets.json" ||
        name == "manifest.json";
}

[[nodiscard]] bool IsSceneArtifact(const fs::path& path)
{
    const std::string ext = path.extension().string();
    return ext == ".scene" || ext == ".sagascene" || ext == ".scene.json";
}

[[nodiscard]] bool IsEntityArtifact(const fs::path& path)
{
    const std::string name = path.filename().string();
    const std::string ext = path.extension().string();
    return name == "entities.json" || ext == ".entity" ||
        ext == ".sagaentity";
}

} // namespace

SceneAssetBrowserInventoryView LoadSceneAssetBrowserInventoryView(
    const ProjectReadinessView& project)
{
    SceneAssetBrowserInventoryView view;
    view.projectRoot = project.projectRoot;
    if (!project.ok)
    {
        view.diagnostics = project.diagnostics;
        return view;
    }

    const fs::path assetRoot = (project.projectRoot / "Assets").lexically_normal();
    view.assetRoots.push_back(assetRoot);
    if (!fs::exists(assetRoot))
    {
        view.diagnostics.push_back(AuthoringDiagnostic{
            "Editor.SceneAssetBrowser.AssetRootMissing",
            "asset root is missing",
            assetRoot.string(),
        });
    }
    else
    {
        for (const auto& entry : fs::recursive_directory_iterator(assetRoot))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            const fs::path path = entry.path();
            if (IsAssetManifest(path))
            {
                view.manifests.push_back(Item(path.filename().string(),
                                              "AssetManifest",
                                              path));
            }
            else
            {
                view.assets.push_back(Item(path.filename().string(),
                                           "AssetFile",
                                           path));
            }
        }
    }

    const fs::path sceneRoot = (project.projectRoot / "Scenes").lexically_normal();
    if (fs::exists(sceneRoot))
    {
        for (const auto& entry : fs::recursive_directory_iterator(sceneRoot))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            const fs::path path = entry.path();
            if (IsSceneArtifact(path))
            {
                view.scenes.push_back(Item(path.stem().string(),
                                           "Scene",
                                           path));
            }
            else if (IsEntityArtifact(path))
            {
                view.entities.push_back(Item(path.stem().string(),
                                             "Entity",
                                             path));
            }
        }
    }

    if (view.manifests.empty())
    {
        view.diagnostics.push_back(AuthoringDiagnostic{
            "Editor.SceneAssetBrowser.AssetManifestMissing",
            "no asset manifest source-of-truth was discovered",
            assetRoot.string(),
        });
    }
    if (view.scenes.empty() && view.entities.empty())
    {
        view.sceneEntitySourceStatus = "MissingSourceOfTruth";
        view.diagnostics.push_back(AuthoringDiagnostic{
            "Editor.SceneAssetBrowser.SceneEntitySourceMissing",
            "no scene or entity source-of-truth was discovered",
            project.projectRoot.string(),
        });
    }
    else
    {
        view.sceneEntitySourceStatus = "Ready";
    }

    std::sort(view.assets.begin(), view.assets.end(),
              [](const SceneAssetInventoryItemView& lhs,
                 const SceneAssetInventoryItemView& rhs)
              {
                  return lhs.path < rhs.path;
              });
    std::sort(view.manifests.begin(), view.manifests.end(),
              [](const SceneAssetInventoryItemView& lhs,
                 const SceneAssetInventoryItemView& rhs)
              {
                  return lhs.path < rhs.path;
              });
    view.ok = view.diagnostics.empty();
    return view;
}

} // namespace SagaEditor::Authoring
