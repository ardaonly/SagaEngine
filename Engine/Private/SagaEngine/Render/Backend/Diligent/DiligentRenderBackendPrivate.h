/// @file DiligentRenderBackendPrivate.h
/// @brief Private shared state for the Diligent render backend split.

#pragma once

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentFrameCapture.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentFrameSlotTracker.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentGpuTimeline.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentOverlayRenderer.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentPipelineCache.h"
#include "SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentGraphicsRuntime.h"
#include "SagaEngine/Core/Log/LogCategories.h"
#include "SagaEngine/Render/Materials/Material.h"
#include "SagaEngine/Render/Materials/MeshAsset.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

// Diligent's PlatformDefinitions.h defines D3D12_SUPPORTED, D3D11_SUPPORTED,
// VULKAN_SUPPORTED, GL_SUPPORTED. Define the platform/API switches before the
// Diligent headers so every split translation unit sees the same backend set.
#if defined(_WIN32)
#   ifndef D3D12_SUPPORTED
#       define D3D12_SUPPORTED  1
#   endif
#   ifndef VULKAN_SUPPORTED
#       define VULKAN_SUPPORTED 1
#   endif
#   ifndef PLATFORM_WIN32
#       define PLATFORM_WIN32   1
#   endif
#elif defined(__linux__)
#   ifndef VULKAN_SUPPORTED
#       define VULKAN_SUPPORTED 1
#   endif
#   ifndef GL_SUPPORTED
#       define GL_SUPPORTED     1
#   endif
#   ifndef PLATFORM_LINUX
#       define PLATFORM_LINUX   1
#   endif
#elif defined(__APPLE__)
#   ifndef VULKAN_SUPPORTED
#       define VULKAN_SUPPORTED 1
#   endif
#   ifndef PLATFORM_MACOS
#       define PLATFORM_MACOS   1
#   endif
#endif

#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "Texture.h"
#include "PipelineState.h"
#include "ShaderResourceBinding.h"
#include "MapHelper.hpp"

namespace SagaEngine::Render::Backend
{

namespace Gfx = ::SagaEngine::Graphics;
namespace DiligentRuntime = ::SagaEngine::Graphics::Backends::Diligent::Runtime;

struct MeshGPU
{
    Gfx::BufferHandle vertexBuffer;
    Gfx::BufferHandle indexBuffer;
    Diligent::Uint32  vertexStride = 0;
    Diligent::Uint32  vertexCount  = 0;
    Diligent::Uint32  indexCount   = 0;
};

struct TextureGPU
{
    Gfx::TextureHandle texture;
};

struct TextureHandleHash
{
    std::size_t operator()(TextureHandle id) const noexcept
    { return std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(id)); }
};

struct MaterialGPU
{
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pso;
    DiligentRuntime::NativeShaderBindingHandle binding;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         skinnedPso;
    DiligentRuntime::NativeShaderBindingHandle skinnedBinding;
    MaterialRenderQueue renderQueue = MaterialRenderQueue::Opaque;
    MaterialCullMode    cullMode   = MaterialCullMode::Back;
    OpaqueShadingModel  shadingModel = OpaqueShadingModel::Unlit;
    bool                writesDepth = true;
    bool                receivesShadows = true;
    bool                castsShadows = true;
    TextureHandle       albedoTex  = TextureHandle::kInvalid;
};

struct MeshIdHash
{
    std::size_t operator()(World::MeshId id) const noexcept
    { return std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(id)); }
};

struct MaterialIdHash
{
    std::size_t operator()(World::MaterialId id) const noexcept
    { return std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(id)); }
};

struct DirectionalShadowSettings
{
    std::uint32_t resolution = 1024;
    float orthographicExtent = 8.0f;
    float nearPlane = 0.1f;
    float farPlane = 24.0f;
    float constantBias = 0.003f;
    float normalBias = 0.006f;
    std::uint32_t pcfRadius = 1;
};

struct ShadowResources
{
    Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> dsv;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
    Diligent::RefCntAutoPtr<Diligent::IShader> depthVS;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> depthPSO;
    Diligent::TEXTURE_FORMAT format = Diligent::TEX_FORMAT_D32_FLOAT;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t creationCount = 0;
    std::uint32_t recreationCount = 0;
};

struct DiligentRenderBackend::Impl
{
    RenderBackendConfig                    config{};
    std::unique_ptr<DiligentRuntime::DiligentGraphicsRuntime> runtime;

    Diligent::RefCntAutoPtr<Diligent::IBuffer> cameraCB;

    Diligent::RefCntAutoPtr<Diligent::IShader> solidVS;
    Diligent::RefCntAutoPtr<Diligent::IShader> skinnedVS;
    Diligent::RefCntAutoPtr<Diligent::IShader> solidPS;

    Diligent::RefCntAutoPtr<Diligent::IBuffer> boneCB;

    std::unordered_map<World::MeshId, MeshGPU, MeshIdHash>             meshCache;
    std::unordered_map<World::MaterialId, MaterialGPU, MaterialIdHash> materialCache;
    std::unordered_map<PSOCacheKey, Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PSOCacheKeyHash> psoCache;
    std::unordered_map<PSOCacheKey, Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PSOCacheKeyHash> skinnedPsoCache;

    std::unordered_map<TextureHandle, TextureGPU, TextureHandleHash> textureCache;
    std::uint32_t nextTextureId = 1;

    std::uint32_t nextMeshId     = 1;
    std::uint32_t nextMaterialId = 1;

    std::uint64_t frameIndex   = 0;
    bool          shadowSubmitAcceptedThisFrame = false;
    bool          gpuValidation = false;
    bool          rendererInitialized = false;
    RenderFrameDiagnostics currentFrameDiagnostics{};
    RenderFrameDiagnostics lastFrameDiagnostics{};

    DiligentFrameCapture frameCapture;
    DiligentOverlayRenderer overlayRenderer;

    DirectionalShadowSettings shadowSettings{};
    ShadowResources           shadow{};
};

} // namespace SagaEngine::Render::Backend
