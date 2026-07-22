// SPDX-License-Identifier: MPL-2.0
/// @file RmlUiRenderInterface.cpp

#include "RmlUiRenderInterface.h"

#include <algorithm>
#include <cstdint>

namespace SagaEngine::UI::Backends
{
namespace SagaRender = ::SagaEngine::Render::Backend;

void RmlUiRenderInterface::Configure(SagaRender::IRenderOverlaySubmission* submission,
    UiViewport viewport) noexcept
{
    submission_ = submission;
    viewport_ = viewport;
}

bool RmlUiRenderInterface::Initialize()
{
    if (initialized_) return true;
    if (submission_)
    {
        const auto result = submission_->InitializeOverlay();
        if (!result.Succeeded()) return false;
    }
    initialized_ = true;
    return true;
}

void RmlUiRenderInterface::Shutdown() noexcept
{
    if (!initialized_ && textures_.empty() && geometry_.empty()) return;
    if (submission_ && initialized_)
    {
        for (const auto& [_, texture] : textures_)
            submission_->DestroyOverlayTextureResource(texture);
        submission_->ShutdownOverlay();
    }
    textures_.clear();
    geometry_.clear();
    frame_ = {};
    initialized_ = false;
    nextGeometryHandle_ = 1;
    nextTextureHandle_ = 1;
}

void RmlUiRenderInterface::SetViewport(UiViewport viewport) noexcept
{
    viewport_ = viewport;
}

void RmlUiRenderInterface::BeginFrame()
{
    frame_ = {};
    frame_.displaySize[0] = static_cast<float>(viewport_.width);
    frame_.displaySize[1] = static_cast<float>(viewport_.height);
    frame_.framebufferScale[0] = viewport_.pixelRatio;
    frame_.framebufferScale[1] = viewport_.pixelRatio;
}

bool RmlUiRenderInterface::EndFrame(UiRenderStats& stats)
{
    stats.drawCallCount = frame_.drawLists.size();
    stats.compiledGeometryCount = geometry_.size();
    stats.textureCount = textures_.size();
    if (viewport_.width == 0 || viewport_.height == 0) return true;
    return !submission_ || submission_->SubmitOverlayFrame(frame_).Succeeded();
}

void RmlUiRenderInterface::RenderGeometry(Rml::Vertex* vertices, int numVertices,
    int* indices, int numIndices, Rml::TextureHandle texture,
    const Rml::Vector2f& translation)
{
    Append(vertices, numVertices, indices, numIndices, texture, translation);
}

Rml::CompiledGeometryHandle RmlUiRenderInterface::CompileGeometry(Rml::Vertex* vertices,
    int numVertices, int* indices, int numIndices, Rml::TextureHandle texture)
{
    if (!vertices || !indices || numVertices <= 0 || numIndices <= 0) return 0;
    const auto handle = nextGeometryHandle_++;
    geometry_.emplace(handle, Geometry{
        std::vector<Rml::Vertex>(vertices, vertices + numVertices),
        std::vector<int>(indices, indices + numIndices), texture});
    return handle;
}

void RmlUiRenderInterface::RenderCompiledGeometry(Rml::CompiledGeometryHandle geometry,
    const Rml::Vector2f& translation)
{
    const auto found = geometry_.find(geometry);
    if (found == geometry_.end()) return;
    Append(found->second.vertices.data(), static_cast<int>(found->second.vertices.size()),
        found->second.indices.data(), static_cast<int>(found->second.indices.size()),
        found->second.texture, translation);
}

void RmlUiRenderInterface::ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry)
{
    geometry_.erase(geometry);
}

bool RmlUiRenderInterface::LoadTexture(Rml::TextureHandle& textureHandle,
    Rml::Vector2i& textureDimensions, const Rml::String& source)
{
    (void)source;
    textureHandle = 0;
    textureDimensions = {0, 0};
    return false;
}

bool RmlUiRenderInterface::GenerateTexture(Rml::TextureHandle& textureHandle,
    const Rml::byte* source, const Rml::Vector2i& sourceDimensions)
{
    textureHandle = 0;
    if (!submission_ || !source || sourceDimensions.x <= 0 || sourceDimensions.y <= 0) return false;
    SagaRender::RenderOverlayTextureDesc desc;
    desc.width = static_cast<std::uint32_t>(sourceDimensions.x);
    desc.height = static_cast<std::uint32_t>(sourceDimensions.y);
    desc.rowPitchBytes = desc.width * 4u;
    const auto native = submission_->CreateOverlayTextureResource(
        desc, reinterpret_cast<const std::uint8_t*>(source));
    if (!native) return false;
    textureHandle = nextTextureHandle_++;
    textures_.emplace(textureHandle, native);
    return true;
}

void RmlUiRenderInterface::ReleaseTexture(Rml::TextureHandle texture)
{
    const auto found = textures_.find(texture);
    if (found == textures_.end()) return;
    if (submission_) submission_->DestroyOverlayTextureResource(found->second);
    textures_.erase(found);
}

void RmlUiRenderInterface::EnableScissorRegion(bool enable) { scissorEnabled_ = enable; }
void RmlUiRenderInterface::SetScissorRegion(int x, int y, int width, int height)
{
    scissorX_ = x;
    scissorY_ = y;
    scissorWidth_ = width;
    scissorHeight_ = height;
}

void RmlUiRenderInterface::Append(const Rml::Vertex* vertices, int numVertices,
    const int* indices, int numIndices, Rml::TextureHandle texture,
    const Rml::Vector2f& translation)
{
    if (!vertices || !indices || numVertices <= 0 || numIndices <= 0) return;
    SagaRender::RenderOverlayDrawList list;
    list.vertices.reserve(static_cast<std::size_t>(numVertices));
    for (int index = 0; index < numVertices; ++index)
    {
        SagaRender::RenderOverlayVertex converted;
        converted.pos[0] = vertices[index].position.x + translation.x;
        converted.pos[1] = vertices[index].position.y + translation.y;
        converted.uv[0] = vertices[index].tex_coord.x;
        converted.uv[1] = vertices[index].tex_coord.y;
        const auto& color = vertices[index].colour;
        converted.colorRgba8 = static_cast<std::uint32_t>(color.red) |
            (static_cast<std::uint32_t>(color.green) << 8u) |
            (static_cast<std::uint32_t>(color.blue) << 16u) |
            (static_cast<std::uint32_t>(color.alpha) << 24u);
        list.vertices.push_back(converted);
    }
    list.indices.reserve(static_cast<std::size_t>(numIndices));
    for (int index = 0; index < numIndices; ++index)
    {
        if (indices[index] < 0 || indices[index] >= numVertices) return;
        list.indices.push_back(static_cast<std::uint32_t>(indices[index]));
    }
    SagaRender::RenderOverlayDrawCommand command;
    command.texture = ResolveTexture(texture);
    command.elementCount = static_cast<std::uint32_t>(numIndices);
    command.clipRect = scissorEnabled_
        ? SagaRender::RenderOverlayClipRect{static_cast<float>(scissorX_),
            static_cast<float>(scissorY_), static_cast<float>(scissorX_ + scissorWidth_),
            static_cast<float>(scissorY_ + scissorHeight_)}
        : SagaRender::RenderOverlayClipRect{0.0f, 0.0f,
            static_cast<float>(viewport_.width), static_cast<float>(viewport_.height)};
    list.commands.push_back(command);
    frame_.drawLists.push_back(std::move(list));
}

SagaRender::RenderOverlayTextureHandle RmlUiRenderInterface::ResolveTexture(
    Rml::TextureHandle texture) const noexcept
{
    const auto found = textures_.find(texture);
    return found == textures_.end() ? SagaRender::RenderOverlayTextureHandle{} : found->second;
}
} // namespace SagaEngine::UI::Backends
