/// @file DiligentPixelTestHelpers.h
/// @brief Test-only geometry and pixel helpers for Diligent GPU integration tests.

#pragma once

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h"
#include "SagaEngine/Render/Materials/Material.h"
#include "SagaEngine/Render/Materials/MeshAsset.h"
#include "SagaEngine/Render/Scene/Camera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

namespace SagaTests::Render
{

namespace EngineRender = ::SagaEngine::Render;
namespace Backend = ::SagaEngine::Render::Backend;
namespace Math = ::SagaEngine::Math;
namespace Scene = ::SagaEngine::Render::Scene;

struct Rgba8
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

struct PixelBounds
{
    std::uint32_t minX = 0;
    std::uint32_t minY = 0;
    std::uint32_t maxX = 0;
    std::uint32_t maxY = 0;
    bool valid = false;

    [[nodiscard]] std::uint32_t Width() const noexcept
    {
        return valid ? (maxX - minX + 1u) : 0u;
    }

    [[nodiscard]] std::uint32_t Height() const noexcept
    {
        return valid ? (maxY - minY + 1u) : 0u;
    }

    [[nodiscard]] std::uint64_t Area() const noexcept
    {
        return static_cast<std::uint64_t>(Width()) * Height();
    }
};

struct PixelCentroid
{
    float x = 0.0f;
    float y = 0.0f;
    std::uint32_t count = 0;
};

[[nodiscard]] inline Rgba8 PixelAt(const Backend::RenderFrameCapture& capture,
                                   std::uint32_t x,
                                   std::uint32_t y) noexcept
{
    const auto idx = static_cast<std::size_t>(y) * capture.rowPitch +
        static_cast<std::size_t>(x) * 4u;
    return {
        capture.pixels[idx + 0u],
        capture.pixels[idx + 1u],
        capture.pixels[idx + 2u],
        capture.pixels[idx + 3u],
    };
}

[[nodiscard]] inline bool ColorNear(Rgba8 actual, Rgba8 expected,
                                    int tolerance) noexcept
{
    auto nearChannel = [tolerance](std::uint8_t a, std::uint8_t b)
    {
        return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= tolerance;
    };
    return nearChannel(actual.r, expected.r) &&
           nearChannel(actual.g, expected.g) &&
           nearChannel(actual.b, expected.b) &&
           nearChannel(actual.a, expected.a);
}

[[nodiscard]] inline bool IsDominantRed(Rgba8 c, std::uint8_t minValue = 80) noexcept
{
    return c.r >= minValue && c.r > c.g + 20u && c.r > c.b + 20u;
}

[[nodiscard]] inline bool IsDominantGreen(Rgba8 c, std::uint8_t minValue = 80) noexcept
{
    return c.g >= minValue && c.g > c.r + 20u && c.g > c.b + 20u;
}

[[nodiscard]] inline bool IsDominantBlue(Rgba8 c, std::uint8_t minValue = 80) noexcept
{
    return c.b >= minValue && c.b > c.r + 20u && c.b > c.g + 20u;
}

[[nodiscard]] inline std::uint32_t CountPixelsNotNear(
    const Backend::RenderFrameCapture& capture,
    Rgba8 clear,
    int tolerance)
{
    std::uint32_t count = 0;
    for (std::uint32_t y = 0; y < capture.height; ++y)
    {
        for (std::uint32_t x = 0; x < capture.width; ++x)
        {
            if (!ColorNear(PixelAt(capture, x, y), clear, tolerance))
            {
                ++count;
            }
        }
    }
    return count;
}

template <typename Predicate>
[[nodiscard]] inline std::uint32_t CountPixelsMatching(
    const Backend::RenderFrameCapture& capture,
    Predicate predicate)
{
    std::uint32_t count = 0;
    for (std::uint32_t y = 0; y < capture.height; ++y)
    {
        for (std::uint32_t x = 0; x < capture.width; ++x)
        {
            if (predicate(PixelAt(capture, x, y)))
            {
                ++count;
            }
        }
    }
    return count;
}

template <typename Predicate>
[[nodiscard]] inline std::uint32_t CountRegionMatching(
    const Backend::RenderFrameCapture& capture,
    std::uint32_t centerX,
    std::uint32_t centerY,
    std::uint32_t halfExtent,
    Predicate predicate)
{
    const auto minX = centerX > halfExtent ? centerX - halfExtent : 0u;
    const auto minY = centerY > halfExtent ? centerY - halfExtent : 0u;
    const auto maxX = std::min(capture.width - 1u, centerX + halfExtent);
    const auto maxY = std::min(capture.height - 1u, centerY + halfExtent);

    std::uint32_t count = 0;
    for (std::uint32_t y = minY; y <= maxY; ++y)
    {
        for (std::uint32_t x = minX; x <= maxX; ++x)
        {
            if (predicate(PixelAt(capture, x, y)))
            {
                ++count;
            }
        }
    }
    return count;
}

[[nodiscard]] inline float Luminance(Rgba8 c) noexcept
{
    return 0.2126f * static_cast<float>(c.r) +
           0.7152f * static_cast<float>(c.g) +
           0.0722f * static_cast<float>(c.b);
}

[[nodiscard]] inline float AverageRegionLuminance(
    const Backend::RenderFrameCapture& capture,
    std::uint32_t centerX,
    std::uint32_t centerY,
    std::uint32_t halfExtent)
{
    const auto minX = centerX > halfExtent ? centerX - halfExtent : 0u;
    const auto minY = centerY > halfExtent ? centerY - halfExtent : 0u;
    const auto maxX = std::min(capture.width - 1u, centerX + halfExtent);
    const auto maxY = std::min(capture.height - 1u, centerY + halfExtent);

    float total = 0.0f;
    std::uint32_t count = 0;
    for (std::uint32_t y = minY; y <= maxY; ++y)
    {
        for (std::uint32_t x = minX; x <= maxX; ++x)
        {
            total += Luminance(PixelAt(capture, x, y));
            ++count;
        }
    }
    return count > 0 ? total / static_cast<float>(count) : 0.0f;
}

[[nodiscard]] inline PixelBounds FindNonClearBounds(
    const Backend::RenderFrameCapture& capture,
    Rgba8 clear,
    int tolerance)
{
    PixelBounds bounds{};
    bounds.minX = std::numeric_limits<std::uint32_t>::max();
    bounds.minY = std::numeric_limits<std::uint32_t>::max();

    for (std::uint32_t y = 0; y < capture.height; ++y)
    {
        for (std::uint32_t x = 0; x < capture.width; ++x)
        {
            if (ColorNear(PixelAt(capture, x, y), clear, tolerance))
            {
                continue;
            }

            bounds.valid = true;
            bounds.minX = std::min(bounds.minX, x);
            bounds.minY = std::min(bounds.minY, y);
            bounds.maxX = std::max(bounds.maxX, x);
            bounds.maxY = std::max(bounds.maxY, y);
        }
    }

    if (!bounds.valid)
    {
        bounds.minX = bounds.minY = bounds.maxX = bounds.maxY = 0u;
    }
    return bounds;
}

[[nodiscard]] inline PixelCentroid FindDarkerPixelCentroid(
    const Backend::RenderFrameCapture& reference,
    const Backend::RenderFrameCapture& darker,
    float luminanceDelta)
{
    PixelCentroid centroid{};
    if (reference.width != darker.width || reference.height != darker.height)
        return centroid;

    double sumX = 0.0;
    double sumY = 0.0;
    for (std::uint32_t y = 0; y < reference.height; ++y)
    {
        for (std::uint32_t x = 0; x < reference.width; ++x)
        {
            const float refLum = Luminance(PixelAt(reference, x, y));
            const float darkLum = Luminance(PixelAt(darker, x, y));
            if (refLum > darkLum + luminanceDelta)
            {
                sumX += static_cast<double>(x);
                sumY += static_cast<double>(y);
                ++centroid.count;
            }
        }
    }

    if (centroid.count > 0)
    {
        centroid.x = static_cast<float>(sumX / centroid.count);
        centroid.y = static_cast<float>(sumY / centroid.count);
    }
    return centroid;
}

[[nodiscard]] inline std::vector<std::uint8_t> SolidTexturePixels(Rgba8 color)
{
    return {color.r, color.g, color.b, color.a};
}

[[nodiscard]] inline std::vector<std::uint8_t> CheckerTexturePixels(
    Rgba8 a,
    Rgba8 b)
{
    std::vector<std::uint8_t> pixels(2u * 2u * 4u);
    const std::array<Rgba8, 4> texels{a, b, b, a};
    for (std::size_t i = 0; i < texels.size(); ++i)
    {
        pixels[i * 4u + 0u] = texels[i].r;
        pixels[i * 4u + 1u] = texels[i].g;
        pixels[i * 4u + 2u] = texels[i].b;
        pixels[i * 4u + 3u] = texels[i].a;
    }
    return pixels;
}

[[nodiscard]] inline EngineRender::MeshAsset BuildIndexedCubeMeshAsset(
    const char* debugName = "DiligentGPUIndexedCube")
{
    EngineRender::MeshAsset asset{};
    asset.meshId = 1;
    asset.debugName = debugName;
    asset.lodCount = 1;
    asset.rootBounds.centre = {0.0f, 0.0f, 0.0f};
    asset.rootBounds.halfExtents = {0.5f, 0.5f, 0.5f};

    auto& lod = asset.lods[0];

    auto pushFace = [&](EngineRender::MeshVec3 p0,
                        EngineRender::MeshVec3 p1,
                        EngineRender::MeshVec3 p2,
                        EngineRender::MeshVec3 p3,
                        EngineRender::MeshVec3 normal,
                        EngineRender::MeshVec3 tangent)
    {
        const auto base = static_cast<std::uint32_t>(lod.vertices.size());

        auto makeVertex = [&](EngineRender::MeshVec3 position,
                              EngineRender::MeshVec2 uv)
        {
            EngineRender::MeshVertex vertex{};
            vertex.position = position;
            vertex.normal = normal;
            vertex.tangent = tangent;
            vertex.handedness = 1.0f;
            vertex.uv0 = uv;
            return vertex;
        };

        lod.vertices.push_back(makeVertex(p0, {0.0f, 1.0f}));
        lod.vertices.push_back(makeVertex(p1, {1.0f, 1.0f}));
        lod.vertices.push_back(makeVertex(p2, {1.0f, 0.0f}));
        lod.vertices.push_back(makeVertex(p3, {0.0f, 0.0f}));

        lod.indices.push_back(base);
        lod.indices.push_back(base + 1u);
        lod.indices.push_back(base + 2u);
        lod.indices.push_back(base);
        lod.indices.push_back(base + 2u);
        lod.indices.push_back(base + 3u);
    };

    constexpr float h = 0.5f;
    pushFace({-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h},
             {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f});
    pushFace({h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h},
             {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f});
    pushFace({h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h},
             {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f});
    pushFace({-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h},
             {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    pushFace({-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h},
             {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    pushFace({-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h},
             {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});

    lod.vertexCountHint = static_cast<std::uint32_t>(lod.vertices.size());
    lod.indexCountHint = static_cast<std::uint32_t>(lod.indices.size());
    lod.approxGpuBytes =
        static_cast<std::uint64_t>(lod.vertices.size()) *
            sizeof(EngineRender::MeshVertex) +
        static_cast<std::uint64_t>(lod.indices.size()) *
            sizeof(std::uint32_t);

    return asset;
}

[[nodiscard]] inline EngineRender::MeshAsset BuildQuadMeshAsset(
    float halfExtent,
    float z,
    const char* debugName = "DiligentGPUQuad")
{
    EngineRender::MeshAsset asset{};
    asset.meshId = 2;
    asset.debugName = debugName;
    asset.lodCount = 1;
    asset.rootBounds.centre = {0.0f, 0.0f, z};
    asset.rootBounds.halfExtents = {halfExtent, halfExtent, 0.0f};

    auto& lod = asset.lods[0];

    EngineRender::MeshVertex v0{}, v1{}, v2{}, v3{};
    v0.position = {-halfExtent, -halfExtent, z};
    v1.position = { halfExtent, -halfExtent, z};
    v2.position = { halfExtent,  halfExtent, z};
    v3.position = {-halfExtent,  halfExtent, z};
    v0.normal = v1.normal = v2.normal = v3.normal = {0.0f, 0.0f, 1.0f};
    v0.tangent = v1.tangent = v2.tangent = v3.tangent = {1.0f, 0.0f, 0.0f};
    v0.handedness = v1.handedness = v2.handedness = v3.handedness = 1.0f;
    v0.uv0 = {0.0f, 1.0f};
    v1.uv0 = {1.0f, 1.0f};
    v2.uv0 = {1.0f, 0.0f};
    v3.uv0 = {0.0f, 0.0f};

    lod.vertices = {v0, v1, v2, v3};
    lod.indices = {0u, 1u, 2u, 0u, 2u, 3u};
    lod.vertexCountHint = 4u;
    lod.indexCountHint = 6u;
    lod.approxGpuBytes =
        sizeof(EngineRender::MeshVertex) * 4u + sizeof(std::uint32_t) * 6u;
    return asset;
}

[[nodiscard]] inline Scene::Camera MakeCamera(float aspect = 4.0f / 3.0f)
{
    Scene::Camera camera{};
    camera.position = {0.0f, 0.0f, 3.0f};
    camera.view = Math::Mat4::LookAtRH(
        camera.position,
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f});
    camera.projection = Math::Mat4::PerspectiveRH_ZO(
        1.0472f, aspect, 0.1f, 100.0f);
    camera.RecomputeViewProj();
    camera.RecomputeFrustum();
    return camera;
}

[[nodiscard]] inline EngineRender::MaterialRuntime MakeMaterial(
    std::uint64_t materialId,
    EngineRender::TextureHandle albedo,
    EngineRender::MaterialCullMode cullMode =
        EngineRender::MaterialCullMode::Back,
    EngineRender::OpaqueShadingModel shadingModel =
        EngineRender::OpaqueShadingModel::Unlit)
{
    EngineRender::MaterialRuntime material{};
    material.materialId = materialId;
    material.renderQueue = EngineRender::MaterialRenderQueue::Opaque;
    material.cullMode = cullMode;
    material.shadingModel = shadingModel;
    material.writesDepth = true;
    material.textures[
        static_cast<std::size_t>(EngineRender::MaterialTextureSlot::Albedo)] =
            albedo;
    return material;
}

} // namespace SagaTests::Render
