#pragma once

#include "SagaEditor/Authoring/ProjectBrowserWorkflowView.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

struct SceneAssetInventoryItemView
{
    std::string id;
    std::string kind;
    std::filesystem::path path;
    bool exists = false;
    std::string status;
};

struct SceneAssetBrowserInventoryView
{
    bool ok = false;
    std::filesystem::path projectRoot;
    std::vector<std::filesystem::path> assetRoots;
    std::vector<SceneAssetInventoryItemView> scenes;
    std::vector<SceneAssetInventoryItemView> entities;
    std::vector<SceneAssetInventoryItemView> assets;
    std::vector<SceneAssetInventoryItemView> manifests;
    std::string sceneEntitySourceStatus;
    std::vector<AuthoringDiagnostic> diagnostics;
};

[[nodiscard]] SceneAssetBrowserInventoryView LoadSceneAssetBrowserInventoryView(
    const ProjectReadinessView& project);

} // namespace SagaEditor::Authoring
