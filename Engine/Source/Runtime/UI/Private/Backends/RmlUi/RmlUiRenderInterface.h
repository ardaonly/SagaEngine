// SPDX-License-Identifier: MPL-2.0
/// @file RmlUiRenderInterface.h
/// @brief Converts private RmlUi geometry into vendor-neutral Saga overlay submissions.

#pragma once

#include "SagaEngine/Render/Backend/RenderBackendFactory.h"
#include "SagaEngine/UI/UiTypes.h"

#include <RmlUi/Core/RenderInterface.h>

#include <unordered_map>

namespace SagaEngine::UI::Backends
{
class RmlUiRenderInterface final : public Rml::RenderInterface
{
public:
    void Configure(Render::Backend::IRenderOverlaySubmission* submission,
                   UiViewport viewport) noexcept;
    [[nodiscard]] bool Initialize();
    void Shutdown() noexcept;
    void SetViewport(UiViewport viewport) noexcept;
    void BeginFrame();
    [[nodiscard]] bool EndFrame(UiRenderStats& stats);

    void RenderGeometry(Rml::Vertex* vertices, int numVertices, int* indices,
        int numIndices, Rml::TextureHandle texture,
        const Rml::Vector2f& translation) override;
    [[nodiscard]] Rml::CompiledGeometryHandle CompileGeometry(Rml::Vertex* vertices,
        int numVertices, int* indices, int numIndices, Rml::TextureHandle texture) override;
    void RenderCompiledGeometry(Rml::CompiledGeometryHandle geometry,
        const Rml::Vector2f& translation) override;
    void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry) override;
    [[nodiscard]] bool LoadTexture(Rml::TextureHandle& textureHandle,
        Rml::Vector2i& textureDimensions, const Rml::String& source) override;
    [[nodiscard]] bool GenerateTexture(Rml::TextureHandle& textureHandle,
        const Rml::byte* source, const Rml::Vector2i& sourceDimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(int x, int y, int width, int height) override;

private:
    struct Geometry
    {
        std::vector<Rml::Vertex> vertices;
        std::vector<int> indices;
        Rml::TextureHandle texture = 0;
    };
    void Append(const Rml::Vertex* vertices, int numVertices, const int* indices,
        int numIndices, Rml::TextureHandle texture, const Rml::Vector2f& translation);
    [[nodiscard]] Render::Backend::RenderOverlayTextureHandle ResolveTexture(
        Rml::TextureHandle texture) const noexcept;

    Render::Backend::IRenderOverlaySubmission* submission_ = nullptr;
    Render::Backend::RenderOverlayFrame frame_;
    std::unordered_map<Rml::CompiledGeometryHandle, Geometry> geometry_;
    std::unordered_map<Rml::TextureHandle, Render::Backend::RenderOverlayTextureHandle> textures_;
    UiViewport viewport_;
    Rml::CompiledGeometryHandle nextGeometryHandle_ = 1;
    Rml::TextureHandle nextTextureHandle_ = 1;
    int scissorX_ = 0;
    int scissorY_ = 0;
    int scissorWidth_ = 0;
    int scissorHeight_ = 0;
    bool scissorEnabled_ = false;
    bool initialized_ = false;
};
} // namespace SagaEngine::UI::Backends
