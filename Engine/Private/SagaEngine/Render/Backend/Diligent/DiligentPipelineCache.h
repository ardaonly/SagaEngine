/// @file DiligentPipelineCache.h
/// @brief Private Diligent graphics PSO cache helpers.

#pragma once

#include "SagaEngine/Render/Materials/Material.h"

#include "GraphicsTypes.h"
#include "RefCntAutoPtr.hpp"

#include <cstdint>
#include <unordered_map>

namespace Diligent
{
struct IBuffer;
struct IPipelineState;
struct IRenderDevice;
struct IShader;
struct ISwapChain;
} // namespace Diligent

namespace SagaEngine::Render::Backend
{

struct PSOCacheKey
{
    std::uint8_t cullMode = 0;
    std::uint8_t renderQueue = 0;
    bool writesDepth = true;

    bool operator==(const PSOCacheKey&) const = default;
};

struct PSOCacheKeyHash
{
    std::size_t operator()(const PSOCacheKey& key) const noexcept
    {
        return std::size_t(key.cullMode) |
            (std::size_t(key.renderQueue) << 8) |
            (std::size_t(key.writesDepth) << 16);
    }
};

Diligent::RefCntAutoPtr<Diligent::IPipelineState> FindOrCreatePSO(
    std::unordered_map<
        PSOCacheKey,
        Diligent::RefCntAutoPtr<Diligent::IPipelineState>,
        PSOCacheKeyHash>& cache,
    Diligent::IRenderDevice& device,
    Diligent::ISwapChain& swapChain,
    Diligent::IShader* solidVS,
    Diligent::IShader* solidPS,
    Diligent::IBuffer* cameraCB,
    const PSOCacheKey& key);

Diligent::RefCntAutoPtr<Diligent::IPipelineState> FindOrCreateSkinnedPSO(
    std::unordered_map<
        PSOCacheKey,
        Diligent::RefCntAutoPtr<Diligent::IPipelineState>,
        PSOCacheKeyHash>& cache,
    Diligent::IRenderDevice& device,
    Diligent::ISwapChain& swapChain,
    Diligent::IShader* skinnedVS,
    Diligent::IShader* solidPS,
    Diligent::IBuffer* cameraCB,
    Diligent::IBuffer* boneCB,
    const PSOCacheKey& key);

Diligent::RefCntAutoPtr<Diligent::IPipelineState> CreateShadowDepthPSO(
    Diligent::IRenderDevice& device,
    Diligent::IShader* shadowVS,
    Diligent::IBuffer* cameraCB,
    Diligent::TEXTURE_FORMAT depthFormat);

} // namespace SagaEngine::Render::Backend
