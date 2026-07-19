// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEngine/Math/Transform.h"

#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{
class SceneDocument;

struct EditorRenderEntitySnapshot
{
    std::string sceneEntityId;
    SagaEngine::Math::Transform worldTransform{};
    std::string meshArtifactId;
    std::string materialArtifactId;
    float boundsRadius = 1.0f;
};

struct EditorDirectionalLightSnapshot
{
    SagaEngine::Math::Vec3 direction{0.0f, -1.0f, 0.0f};
    SagaEngine::Math::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

struct EditorRenderSceneSnapshot
{
    std::string sceneId;
    std::vector<EditorRenderEntitySnapshot> entities;
    std::optional<EditorDirectionalLightSnapshot> directionalLight;
    std::string revisionHash;
};

struct EditorRenderSceneSnapshotResult
{
    EditorRenderSceneSnapshot snapshot;
    std::vector<std::string> diagnostics;
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

[[nodiscard]] EditorRenderSceneSnapshotResult BuildEditorRenderSceneSnapshot(
    const SceneDocument& document);
} // namespace SagaEditor
