// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Viewport/EditorRenderSceneSnapshot.h"
#include "SagaEditor/Viewport/ViewportCamera.h"
#include "SagaEngine/Platform/IWindow.h"
#include "SagaEngine/Render/Backend/IRenderBackend.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{
enum class QtRenderHostState : std::uint8_t
{
    Created,
    Attached,
    Hidden,
    DeviceLost,
    Detached,
    Destroyed,
};

struct QtRenderSurface
{
    Saga::NativeWindowHandle native{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    float devicePixelRatio = 1.0f;
};

struct QtRenderHostStatus
{
    QtRenderHostState state = QtRenderHostState::Created;
    bool backendReady = false;
    std::uint64_t framesRendered = 0;
    std::size_t drawCount = 0;
    std::vector<std::string> diagnostics;
};

struct QtRenderFrameEvidence
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba8;

    [[nodiscard]] bool HasMeaningfulPixels() const noexcept;
};

/// The sole Qt/native-surface bridge. It accepts a toolkit-owned surface while
/// the Saga render backend retains device, context, swapchain and GPU lifetime.
class QtRenderHost
{
public:
    using BackendFactory = std::function<
        std::unique_ptr<SagaEngine::Render::Backend::IRenderBackend>()>;

    explicit QtRenderHost(BackendFactory factory = {});
    ~QtRenderHost();
    [[nodiscard]] bool Attach(const QtRenderSurface& surface);
    void Resize(std::uint32_t width, std::uint32_t height, float devicePixelRatio);
    void Show();
    void Hide();
    [[nodiscard]] bool LoadScene(const EditorRenderSceneSnapshot& snapshot,
                                 const std::filesystem::path& cookedContentRoot);
    [[nodiscard]] bool RequestFrame(const ViewportCamera& camera);
    /// Renders and captures before presentation advances the swapchain. This
    /// explicit evidence path avoids readback cost during normal UI frames.
    [[nodiscard]] std::optional<QtRenderFrameEvidence> CaptureFrame(
        const ViewportCamera& camera);
    [[nodiscard]] std::optional<std::string> Pick(float pixelX, float pixelY,
                                                   const ViewportCamera& camera) const;
    void SetHighlightedEntity(std::string sceneEntityId);
    void DeviceLost();
    [[nodiscard]] bool Recover(const QtRenderSurface& surface);
    void Detach();
    void Destroy();
    [[nodiscard]] const QtRenderHostStatus& Status() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace SagaEditor
