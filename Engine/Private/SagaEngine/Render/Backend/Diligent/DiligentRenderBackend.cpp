/// @file DiligentRenderBackend.cpp
/// @brief Phase 3 Diligent backend: real mesh/material pipeline.
///
///   Initialize → device + context + swapchain + CameraCB + default PSO
///   CreateMesh → upload VB + IB, store in meshCache
///   CreateMaterial → lookup/create PSO, create SRB, store in materialCache
///   BeginFrame → bind + clear
///   Submit     → update CameraCB, iterate DrawItems, bind PSO/VB/IB, draw
///   EndFrame   → Present()
///   Shutdown   → release all

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentFrameCapture.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentGpuTimeline.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentOverlayRenderer.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentPipelineCache.h"
#include "SagaEngine/Platform/IWindow.h"
#include "SagaEngine/Render/Materials/MeshAsset.h"
#include "SagaEngine/Render/Materials/Material.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

// ─── Diligent includes ────────────────────────────────────────────────

// Diligent's PlatformDefinitions.h defines D3D12_SUPPORTED, D3D11_SUPPORTED,
// VULKAN_SUPPORTED, GL_SUPPORTED.  However, the D3D11 backend in the conan
// package depends on ATL (atls.lib) which may not be installed.  We define
// only the APIs we actually need — D3D12 (primary) and Vulkan (fallback).
// This avoids pulling in ATL-dependent objects from the D3D11 static lib.
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

#if D3D12_SUPPORTED
#   include "EngineFactoryD3D12.h"
#endif
#if VULKAN_SUPPORTED
#   include "EngineFactoryVk.h"
#endif
#if D3D11_SUPPORTED
#   include "EngineFactoryD3D11.h"
#endif
#if GL_SUPPORTED
#   include "EngineFactoryOpenGL.h"
#endif

#if PLATFORM_WIN32 || defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX 1
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN 1
#   endif
#   include <windows.h>
#endif

namespace SagaEngine::Render::Backend
{

namespace Gfx = ::SagaEngine::Graphics;

// ─── String form of API enum ──────────────────────────────────────────

std::string_view ToString(GraphicsBackendAPI api) noexcept
{
    switch (api)
    {
        case GraphicsBackendAPI::kAuto:   return "Auto";
        case GraphicsBackendAPI::kNativePrimary:  return "D3D12";
        case GraphicsBackendAPI::kNativePortable: return "Vulkan";
        case GraphicsBackendAPI::kNativeLegacy:  return "D3D11";
        case GraphicsBackendAPI::kCompatibility: return "OpenGL";
    }
    return "Unknown";
}

// ─── GPU resource types ──────────────────────────────────────────────

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
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
    Diligent::IShaderResourceVariable* albedoVariable = nullptr;
    Diligent::IShaderResourceVariable* shadowVariable = nullptr;
    // Skinned variant (created on demand when first skinned draw uses this material).
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         skinnedPso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> skinnedSrb;
    Diligent::IShaderResourceVariable* skinnedAlbedoVariable = nullptr;
    Diligent::IShaderResourceVariable* skinnedShadowVariable = nullptr;
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

// ─── Impl ─────────────────────────────────────────────────────────────

struct DiligentRenderBackend::Impl
{
    RenderBackendConfig                    config{};
    GraphicsBackendAPI                       selectedAPI = GraphicsBackendAPI::kAuto;

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>  device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>     swapChain;
    DiligentNativeResourceOwner nativeResources;
    DiligentGpuTimeline gpuTimeline;
    std::vector<std::uint64_t> frameSlotSerials;
    std::uint64_t activeFrameSerial = 0;
    std::uint32_t activeFrameSlot = 0;

    // ── Shared camera constant buffer ───────────────────────────────
    Diligent::RefCntAutoPtr<Diligent::IBuffer> cameraCB;

    // ── Compiled shaders ───────────────────────────────────────────
    Diligent::RefCntAutoPtr<Diligent::IShader> solidVS;    // static geometry
    Diligent::RefCntAutoPtr<Diligent::IShader> skinnedVS;  // skeletal skinning
    Diligent::RefCntAutoPtr<Diligent::IShader> solidPS;    // shared pixel shader

    // ── Bone matrix constant buffer (128 * 64 = 8192 bytes) ─────
    Diligent::RefCntAutoPtr<Diligent::IBuffer> boneCB;

    // ── Caches ──────────────────────────────────────────────────────
    std::unordered_map<World::MeshId, MeshGPU, MeshIdHash>             meshCache;
    std::unordered_map<World::MaterialId, MaterialGPU, MaterialIdHash> materialCache;
    std::unordered_map<PSOCacheKey, Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PSOCacheKeyHash> psoCache;
    std::unordered_map<PSOCacheKey, Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PSOCacheKeyHash> skinnedPsoCache;

    // ── Texture cache ────────────────────────────────────────────────
    std::unordered_map<TextureHandle, TextureGPU, TextureHandleHash> textureCache;
    Gfx::TextureHandle defaultWhiteTex;   // 1x1 white fallback
    std::uint32_t nextTextureId = 1;

    // ── ID generators ───────────────────────────────────────────────
    std::uint32_t nextMeshId     = 1;
    std::uint32_t nextMaterialId = 1;

    std::uint64_t frameIndex   = 0;
    bool          initialized  = false;
    bool          shadowSubmitAcceptedThisFrame = false;
    RenderFrameDiagnostics currentFrameDiagnostics{};
    RenderFrameDiagnostics lastFrameDiagnostics{};

    DiligentFrameCapture frameCapture;
    DiligentOverlayRenderer overlayRenderer;

    DirectionalShadowSettings shadowSettings{};
    ShadowResources           shadow{};
};

// ─── Small logging helpers ───────────────────────────────────────────

namespace
{

bool g_verboseGPU = false;   // Set to true for deep GPU diagnostics.

void LogInfo(const char* msg)
{
    std::fprintf(stdout, "[DiligentBackend] %s\n", msg);
    std::fflush(stdout);
}

void LogErr(const char* msg)
{
    std::fprintf(stderr, "[DiligentBackend][error] %s\n", msg);
    std::fflush(stderr);
}

void LogDbg(const char* msg)
{
    if (!g_verboseGPU) return;
    std::fprintf(stdout, "[DiligentBackend][dbg] %s\n", msg);
    std::fflush(stdout);
}

struct NormalMatrixRows
{
    float rows[12]{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };
};

bool ComputeNormalMatrixRows(const ::SagaEngine::Math::Mat4& model,
                             NormalMatrixRows& outRows) noexcept
{
    const float a00 = model.At(0, 0), a01 = model.At(0, 1), a02 = model.At(0, 2);
    const float a10 = model.At(1, 0), a11 = model.At(1, 1), a12 = model.At(1, 2);
    const float a20 = model.At(2, 0), a21 = model.At(2, 1), a22 = model.At(2, 2);
    if (!std::isfinite(a00) || !std::isfinite(a01) || !std::isfinite(a02) ||
        !std::isfinite(a10) || !std::isfinite(a11) || !std::isfinite(a12) ||
        !std::isfinite(a20) || !std::isfinite(a21) || !std::isfinite(a22))
    {
        return false;
    }

    const float c00 =  (a11 * a22 - a12 * a21);
    const float c01 = -(a10 * a22 - a12 * a20);
    const float c02 =  (a10 * a21 - a11 * a20);
    const float c10 = -(a01 * a22 - a02 * a21);
    const float c11 =  (a00 * a22 - a02 * a20);
    const float c12 = -(a00 * a21 - a01 * a20);
    const float c20 =  (a01 * a12 - a02 * a11);
    const float c21 = -(a00 * a12 - a02 * a10);
    const float c22 =  (a00 * a11 - a01 * a10);

    const float det = a00 * c00 + a01 * c01 + a02 * c02;
    if (std::fabs(det) < 1.0e-6f)
        return false;

    const float invDet = 1.0f / det;

    // Rows of inverse-transpose. Translation is intentionally excluded.
    outRows.rows[0]  = c00 * invDet;
    outRows.rows[1]  = c01 * invDet;
    outRows.rows[2]  = c02 * invDet;
    outRows.rows[3]  = 0.0f;
    outRows.rows[4]  = c10 * invDet;
    outRows.rows[5]  = c11 * invDet;
    outRows.rows[6]  = c12 * invDet;
    outRows.rows[7]  = 0.0f;
    outRows.rows[8]  = c20 * invDet;
    outRows.rows[9]  = c21 * invDet;
    outRows.rows[10] = c22 * invDet;
    outRows.rows[11] = 0.0f;
    return true;
}

[[nodiscard]] bool NormalizeDirectionalLight(Scene::RenderLightingData& lighting) noexcept
{
    if (!lighting.directionalEnabled)
        return false;

    const float lenSq = lighting.directional.direction.LengthSq();
    if (lenSq < 1.0e-6f)
    {
        lighting.directionalEnabled = false;
        lighting.shadowsEnabled = false;
        return false;
    }

    lighting.directional.direction =
        lighting.directional.direction * (1.0f / std::sqrt(lenSq));
    return true;
}

[[nodiscard]] ::SagaEngine::Math::Mat4 BuildDirectionalLightViewProjection(
    const Scene::RenderLightingData& lighting,
    const DirectionalShadowSettings& settings) noexcept
{
    using ::SagaEngine::Math::Mat4;
    using ::SagaEngine::Math::Vec3;

    Vec3 dir = lighting.directional.direction;
    const float lenSq = dir.LengthSq();
    if (lenSq < 1.0e-6f)
        dir = {0.5f, -1.0f, 0.25f};
    dir = dir.Normalized();

    const Vec3 center{0.0f, 0.0f, 0.0f};
    const Vec3 eye = center - dir * (settings.farPlane * 0.5f);
    Vec3 up = Vec3::Up();
    if (std::fabs(dir.Dot(up)) > 0.95f)
        up = Vec3::Back();

    const Mat4 view = Mat4::LookAtRH(eye, center, up);
    const float e = settings.orthographicExtent;
    const Mat4 proj = Mat4::OrthoRH_ZO(
        -e, e, -e, e, settings.nearPlane, settings.farPlane);
    return proj * view;
}

// ─── Factory init helpers (D3D12, Vulkan, D3D11, OpenGL) ─────────────

#include "DiligentFactoryInit.inl"

// ─── Embedded HLSL shader source ─────────────────────────────────────

#include "DiligentShaders.inl"

} // anonymous namespace

// ─── Construction / destruction ──────────────────────────────────────

DiligentRenderBackend::DiligentRenderBackend()
    : m_Impl(std::make_unique<Impl>()) {}

DiligentRenderBackend::DiligentRenderBackend(RenderBackendConfig cfg)
    : m_Impl(std::make_unique<Impl>())
{
    m_Impl->config = cfg;
}

DiligentRenderBackend::~DiligentRenderBackend()
{
    try { Shutdown(); } catch (...) {}
}

// ─── Lifecycle ───────────────────────────────────────────────────────

bool DiligentRenderBackend::Initialize(const SwapchainDesc& desc)
{
    if (m_Impl->initialized) return true;

    g_verboseGPU = m_Impl->config.enableValidation;

    if (desc.width == 0 || desc.height == 0)
    {
        LogErr("Initialize called with zero-sized swapchain");
        return false;
    }

    const auto picked = TryInitAPI(
        m_Impl->config.preferredAPI, desc, m_Impl->config,
        m_Impl->device, m_Impl->context, m_Impl->swapChain);

    if (picked == GraphicsBackendAPI::kAuto)
    {
        LogErr("No Diligent graphics API was available for the host");
        return false;
    }

    m_Impl->selectedAPI = picked;
    m_Impl->initialized = true;
    m_Impl->frameIndex  = 0;
    m_Impl->nativeResources.Bind({
        m_Impl->device.RawPtr(),
        m_Impl->context.RawPtr(),
        m_Impl->swapChain.RawPtr(),
    });
    if (!m_Impl->gpuTimeline.Initialize(
            m_Impl->device.RawPtr(),
            m_Impl->context.RawPtr()))
    {
        LogErr("Failed to create GPU completion fence");
        return false;
    }
    const auto& scDesc = m_Impl->swapChain->GetDesc();
    const auto frameSlotCount = scDesc.BufferCount == 0u ? 3u : scDesc.BufferCount;
    m_Impl->frameSlotSerials.assign(frameSlotCount, 0u);
    m_Impl->activeFrameSerial = 0u;
    m_Impl->activeFrameSlot = 0u;

    std::string msg = "Initialized with ";
    msg += ToString(picked);
    msg += " (";
    msg += std::to_string(desc.width);
    msg += "x";
    msg += std::to_string(desc.height);
    msg += ")";
    LogInfo(msg.c_str());

    // ── Create CameraCB ──────────────────────────────────────────────
    {
        using namespace Diligent;
        BufferDesc cbDesc;
        cbDesc.Name           = "CameraCB";
        cbDesc.Size           = sizeof(float) * 80;
        cbDesc.Usage          = USAGE_DYNAMIC;
        cbDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_Impl->device->CreateBuffer(cbDesc, nullptr, &m_Impl->cameraCB);
        if (!m_Impl->cameraCB)
        {
            LogErr("Failed to create CameraCB");
            return false;
        }
    }

    // ── Compile solid-color shaders ──────────────────────────────────
    {
        using namespace Diligent;
        ShaderCreateInfo ci;
        ci.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.UseCombinedTextureSamplers = True;
        ci.Desc.CombinedSamplerSuffix      = "_sampler";

        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name       = "Solid VS";
        ci.EntryPoint      = "main";
        ci.Source           = kSolidVS;
        m_Impl->device->CreateShader(ci, &m_Impl->solidVS);

        ci.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ci.Desc.Name       = "Solid PS";
        ci.Source           = kSolidPS;
        m_Impl->device->CreateShader(ci, &m_Impl->solidPS);

        if (!m_Impl->solidVS || !m_Impl->solidPS)
        {
            LogErr("Failed to compile solid-color shaders");
            return false;
        }
        LogDbg("Solid shaders compiled OK");

        // Skinned vertex shader (shares the same PS).
        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name       = "Skinned VS";
        ci.EntryPoint      = "main";
        ci.Source           = kSkinnedVS;
        m_Impl->device->CreateShader(ci, &m_Impl->skinnedVS);

        if (!m_Impl->skinnedVS)
        {
            LogErr("Failed to compile skinned vertex shader");
            return false;
        }
        LogDbg("Skinned VS compiled OK");

        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name       = "Shadow Depth VS";
        ci.EntryPoint      = "main";
        ci.Source          = kShadowDepthVS;
        m_Impl->device->CreateShader(ci, &m_Impl->shadow.depthVS);
        if (!m_Impl->shadow.depthVS)
        {
            LogErr("Failed to compile shadow depth vertex shader");
            return false;
        }
        LogDbg("Shadow depth VS compiled OK");
    }

    // ── Create BoneCB (128 mat4 = 8192 bytes) ──────────────────────
    {
        using namespace Diligent;
        BufferDesc cbDesc;
        cbDesc.Name           = "BoneCB";
        cbDesc.Size           = 128 * 64;   // 128 * sizeof(float4x4)
        cbDesc.Usage          = USAGE_DYNAMIC;
        cbDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_Impl->device->CreateBuffer(cbDesc, nullptr, &m_Impl->boneCB);
        if (!m_Impl->boneCB)
        {
            LogErr("Failed to create BoneCB");
            return false;
        }
        LogDbg("BoneCB created (8192 bytes)");
    }

    // ── Create default 1x1 white texture ────────────────────────────
    {
        const std::uint8_t white[4] = { 255, 255, 255, 255 };

        Gfx::TextureDesc texDesc;
        texDesc.debugName = "DefaultWhite1x1";
        texDesc.width = 1;
        texDesc.height = 1;
        texDesc.format = Gfx::ResourceFormat::Rgba8Unorm;

        const Gfx::GraphicsDataView data{
            white,
            sizeof(white),
            4u,
            0u,
        };

        m_Impl->defaultWhiteTex =
            m_Impl->nativeResources.CreateStandaloneTexture(texDesc, data);
        if (!m_Impl->defaultWhiteTex.IsValid())
        {
            LogErr("Failed to create default white texture");
            return false;
        }

        LogDbg("Default 1x1 white texture created");
    }

    LogInfo("Phase 3: backend ready");
    return true;
}

void DiligentRenderBackend::Shutdown()
{
    if (!m_Impl || !m_Impl->initialized) return;

    ShutdownOverlayRendering();

    m_Impl->gpuTimeline.ShutdownAndDrain();
    m_Impl->nativeResources.RetireCompleted(
        m_Impl->gpuTimeline.LastCompletedSerial());
    m_Impl->meshCache.clear();
    m_Impl->materialCache.clear();
    m_Impl->psoCache.clear();
    m_Impl->skinnedPsoCache.clear();
    m_Impl->textureCache.clear();
    m_Impl->frameCapture.Reset();
    m_Impl->shadow.depthPSO.Release();
    m_Impl->shadow.depthVS.Release();
    m_Impl->shadow.dsv.Release();
    m_Impl->shadow.srv.Release();
    m_Impl->shadow.texture.Release();
    m_Impl->defaultWhiteTex = {};
    m_Impl->nativeResources.ReleaseAll();
    m_Impl->gpuTimeline.Reset();
    m_Impl->frameSlotSerials.clear();
    m_Impl->activeFrameSerial = 0u;
    m_Impl->activeFrameSlot = 0u;

    m_Impl->boneCB.Release();
    m_Impl->cameraCB.Release();
    m_Impl->solidVS.Release();
    m_Impl->skinnedVS.Release();
    m_Impl->solidPS.Release();

    if (m_Impl->context) m_Impl->context->Flush();
    m_Impl->swapChain.Release();
    m_Impl->context.Release();
    m_Impl->device.Release();

    m_Impl->initialized = false;
    m_Impl->selectedAPI = GraphicsBackendAPI::kAuto;
    LogInfo("Shutdown complete");
}

void DiligentRenderBackend::OnResize(std::uint32_t width, std::uint32_t height)
{
    if (!m_Impl->initialized || !m_Impl->swapChain) return;
    if (width == 0 || height == 0) return;
    m_Impl->frameCapture.Reset();
    const auto oldFrameSlotCount =
        static_cast<std::uint32_t>(m_Impl->frameSlotSerials.size());
    m_Impl->swapChain->Resize(width, height);

    const auto& scDesc = m_Impl->swapChain->GetDesc();
    const auto newFrameSlotCount =
        scDesc.BufferCount == 0u ? 3u : scDesc.BufferCount;
    if (newFrameSlotCount == oldFrameSlotCount)
    {
        return;
    }

    std::uint64_t latestSlotSerial = 0u;
    for (const auto serial : m_Impl->frameSlotSerials)
    {
        latestSlotSerial = std::max(latestSlotSerial, serial);
    }
    if (latestSlotSerial != 0u)
    {
        m_Impl->gpuTimeline.WaitForSerial(latestSlotSerial);
        m_Impl->nativeResources.RetireCompleted(
            m_Impl->gpuTimeline.LastCompletedSerial());
    }

    m_Impl->frameSlotSerials.assign(newFrameSlotCount, 0u);
    if (m_Impl->activeFrameSlot >= newFrameSlotCount)
    {
        m_Impl->activeFrameSlot = 0u;
    }
    if (m_Impl->overlayRenderer.IsReady())
    {
        (void)m_Impl->overlayRenderer.ReconfigureFrameSlots(
            newFrameSlotCount);
    }
}

// ─── Resource upload ─────────────────────────────────────────────────

World::MeshId DiligentRenderBackend::CreateMesh(const MeshAsset& asset)
{
    if (!m_Impl->initialized || !m_Impl->device) return World::MeshId::kInvalid;
    if (asset.lodCount == 0) return World::MeshId::kInvalid;

    const auto& lod = asset.lods[0];
    if (lod.vertices.empty()) return World::MeshId::kInvalid;

    MeshGPU gpu{};
    gpu.vertexStride = static_cast<Diligent::Uint32>(sizeof(MeshVertex));
    gpu.vertexCount  = static_cast<Diligent::Uint32>(lod.vertices.size());

    // VB
    {
        Gfx::BufferDesc vbDesc;
        vbDesc.debugName = "MeshVB";
        vbDesc.sizeBytes =
            static_cast<std::uint64_t>(gpu.vertexCount) * gpu.vertexStride;
        vbDesc.usage = Gfx::BufferUsage::Vertex;

        const Gfx::GraphicsDataView vbData{
            lod.vertices.data(),
            vbDesc.sizeBytes,
            0u,
            0u,
        };

        gpu.vertexBuffer =
            m_Impl->nativeResources.CreateStandaloneBuffer(vbDesc, vbData);
        if (!gpu.vertexBuffer.IsValid())
        {
            LogErr("CreateMesh: VB creation failed");
            return World::MeshId::kInvalid;
        }
    }

    // IB
    if (!lod.indices.empty())
    {
        gpu.indexCount = static_cast<Diligent::Uint32>(lod.indices.size());

        Gfx::BufferDesc ibDesc;
        ibDesc.debugName = "MeshIB";
        ibDesc.sizeBytes =
            static_cast<std::uint64_t>(gpu.indexCount) * sizeof(std::uint32_t);
        ibDesc.usage = Gfx::BufferUsage::Index;

        const Gfx::GraphicsDataView ibData{
            lod.indices.data(),
            ibDesc.sizeBytes,
            0u,
            0u,
        };

        gpu.indexBuffer =
            m_Impl->nativeResources.CreateStandaloneBuffer(ibDesc, ibData);
        if (!gpu.indexBuffer.IsValid())
        {
            LogErr("CreateMesh: IB creation failed");
            return World::MeshId::kInvalid;
        }
    }

    auto id = static_cast<World::MeshId>(m_Impl->nextMeshId++);
    m_Impl->meshCache[id] = std::move(gpu);
    return id;
}

World::MaterialId DiligentRenderBackend::CreateMaterial(const MaterialRuntime& runtime)
{
    if (!m_Impl->initialized || !m_Impl->device) return World::MaterialId::kInvalid;

    PSOCacheKey key{};
    key.cullMode    = static_cast<std::uint8_t>(runtime.cullMode);
    key.renderQueue = static_cast<std::uint8_t>(runtime.renderQueue);
    key.writesDepth = runtime.writesDepth;

    auto pso = FindOrCreatePSO(m_Impl->psoCache, *m_Impl->device, *m_Impl->swapChain,
                                m_Impl->solidVS, m_Impl->solidPS, m_Impl->cameraCB, key);
    if (!pso) return World::MaterialId::kInvalid;

    using namespace Diligent;

    MaterialGPU gpu{};
    gpu.pso         = pso;
    gpu.renderQueue = runtime.renderQueue;
    gpu.cullMode    = runtime.cullMode;
    gpu.shadingModel = runtime.shadingModel;
    gpu.writesDepth = runtime.writesDepth;
    gpu.receivesShadows = runtime.receivesShadows;
    gpu.castsShadows = runtime.castsShadows;
    gpu.albedoTex   = runtime.textures[static_cast<std::size_t>(MaterialTextureSlot::Albedo)];

    pso->CreateShaderResourceBinding(&gpu.srb, true);
    if (!gpu.srb)
    {
        LogErr("CreateMaterial: SRB creation failed");
        return World::MaterialId::kInvalid;
    }
    gpu.albedoVariable = gpu.srb->GetVariableByName(
        SHADER_TYPE_PIXEL, "g_Albedo");
    gpu.shadowVariable = gpu.srb->GetVariableByName(
        SHADER_TYPE_PIXEL, "g_ShadowMap");

    auto id = static_cast<World::MaterialId>(m_Impl->nextMaterialId++);
    m_Impl->materialCache[id] = std::move(gpu);
    return id;
}

void DiligentRenderBackend::DestroyMesh(World::MeshId id)
{
    auto it = m_Impl->meshCache.find(id);
    if (it != m_Impl->meshCache.end())
    {
        m_Impl->nativeResources.DestroyBuffer(it->second.vertexBuffer);
        m_Impl->nativeResources.DestroyBuffer(it->second.indexBuffer);
        m_Impl->meshCache.erase(it);
    }
}

void DiligentRenderBackend::DestroyMaterial(World::MaterialId id)
{
    m_Impl->materialCache.erase(id);
}

// ─── Texture upload ─────────────────────────────────────────────────

TextureHandle DiligentRenderBackend::CreateTexture(uint32_t width, uint32_t height,
                                                    const uint8_t* rgba)
{
    if (!m_Impl->initialized || !m_Impl->device || !rgba)
        return TextureHandle::kInvalid;
    if (width == 0 || height == 0)
        return TextureHandle::kInvalid;

    Gfx::TextureDesc texDesc;
    texDesc.debugName = "UserTexture";
    texDesc.width = width;
    texDesc.height = height;
    texDesc.format = Gfx::ResourceFormat::Rgba8Unorm;

    const Gfx::GraphicsDataView texData{
        rgba,
        static_cast<std::uint64_t>(width) * height * 4u,
        width * 4u,
        0u,
    };

    TextureGPU gpu{};
    gpu.texture =
        m_Impl->nativeResources.CreateStandaloneTexture(texDesc, texData);
    if (!gpu.texture.IsValid())
    {
        LogErr("CreateTexture: texture creation failed");
        return TextureHandle::kInvalid;
    }

    auto handle = static_cast<TextureHandle>(m_Impl->nextTextureId++);
    m_Impl->textureCache[handle] = std::move(gpu);

    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "CreateTexture: %ux%u → handle %u",
                      width, height, static_cast<unsigned>(handle));
        LogDbg(buf);
    }

    return handle;
}

void DiligentRenderBackend::DestroyTexture(TextureHandle tex)
{
    auto it = m_Impl->textureCache.find(tex);
    if (it != m_Impl->textureCache.end())
    {
        m_Impl->nativeResources.DestroyTexture(it->second.texture);
        m_Impl->textureCache.erase(it);
    }
}

// ─── Frame lifecycle ─────────────────────────────────────────────────

void DiligentRenderBackend::BeginFrame()
{
    if (!m_Impl->initialized || !m_Impl->context || !m_Impl->swapChain)
        return;

    m_Impl->currentFrameDiagnostics = {};
    m_Impl->shadowSubmitAcceptedThisFrame = false;
    const auto completed = m_Impl->gpuTimeline.PollCompletion();
    m_Impl->nativeResources.RetireCompleted(completed);
    if (!m_Impl->frameSlotSerials.empty())
    {
        m_Impl->activeFrameSlot =
            static_cast<std::uint32_t>(
                m_Impl->frameIndex % m_Impl->frameSlotSerials.size());
        const auto slotSerial =
            m_Impl->frameSlotSerials[m_Impl->activeFrameSlot];
        if (slotSerial != 0u && completed < slotSerial)
        {
            m_Impl->gpuTimeline.WaitForSerial(slotSerial);
            m_Impl->nativeResources.RetireCompleted(
                m_Impl->gpuTimeline.LastCompletedSerial());
        }
    }
    m_Impl->activeFrameSerial = m_Impl->gpuTimeline.BeginFrameSubmission();
    m_Impl->currentFrameDiagnostics.gpuSubmissionSerial =
        m_Impl->activeFrameSerial;
    {
        const auto timeline = m_Impl->gpuTimeline.Diagnostics();
        m_Impl->currentFrameDiagnostics.gpuCompletedSerial =
            timeline.lastCompletedSerial;
        m_Impl->currentFrameDiagnostics.gpuTargetedWaitCount =
            timeline.targetedWaitCount;
        m_Impl->currentFrameDiagnostics.gpuDeviceWideWaitCount =
            timeline.deviceWideWaitCount;
        m_Impl->currentFrameDiagnostics.gpuTimelinePollCount =
            timeline.pollCount;
        m_Impl->currentFrameDiagnostics.gpuTimelineSignalCount =
            timeline.signalCount;
    }

    LogDbg("BeginFrame: enter");

    auto* ctx = m_Impl->context.RawPtr();
    auto* sc  = m_Impl->swapChain.RawPtr();

    auto* rtv = sc->GetCurrentBackBufferRTV();
    auto* dsv = sc->GetDepthBufferDSV();

    if (g_verboseGPU)
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "BeginFrame: rtv=%p dsv=%p frame=%llu",
                      static_cast<void*>(rtv), static_cast<void*>(dsv),
                      static_cast<unsigned long long>(m_Impl->frameIndex));
        LogDbg(buf);
    }

    ctx->SetRenderTargets(
        rtv ? 1u : 0u, rtv ? &rtv : nullptr, dsv,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    LogDbg("BeginFrame: SetRenderTargets OK");

    if (rtv)
        ctx->ClearRenderTarget(rtv, m_Impl->config.clearColor,
                               Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    LogDbg("BeginFrame: ClearRenderTarget OK");

    if (dsv && !m_Impl->config.skipDepthClear)
        ctx->ClearDepthStencil(dsv,
                               Diligent::CLEAR_DEPTH_FLAG | Diligent::CLEAR_STENCIL_FLAG,
                               m_Impl->config.clearDepth, m_Impl->config.clearStencil,
                               Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    LogDbg("BeginFrame: done");
}

void DiligentRenderBackend::Submit(const Scene::Camera&     camera,
                                   const Scene::RenderView& view)
{
    if (!m_Impl->initialized || !m_Impl->context || !m_Impl->cameraCB)
        return;

    auto* ctx = m_Impl->context.RawPtr();

    // ── Nothing to draw? ─────────────────────────────────────────────
    if (view.drawItems.empty()) return;

    Scene::RenderLightingData lighting = view.lighting;
    const bool hasValidDirectional = NormalizeDirectionalLight(lighting);
    if (view.lighting.directionalEnabled && !hasValidDirectional)
    {
        LogDbg("Submit: zero-length directional light disabled");
    }

    const bool shadowRequested =
        lighting.directionalEnabled && lighting.shadowsEnabled;
    if (shadowRequested && m_Impl->shadowSubmitAcceptedThisFrame)
    {
        ++m_Impl->currentFrameDiagnostics.additionalFrameSubmitsRejected;
        m_Impl->currentFrameDiagnostics.rejectedDrawItems +=
            static_cast<std::uint32_t>(view.drawItems.size());
        LogDbg("Submit: additional shadow-enabled Submit rejected");
        return;
    }
    if (shadowRequested)
        m_Impl->shadowSubmitAcceptedThisFrame = true;

    {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "Submit: %zu draw item(s) on frame %llu",
                      view.drawItems.size(),
                      static_cast<unsigned long long>(m_Impl->frameIndex));
        LogDbg(msg);
        if (m_Impl->frameIndex == 0) LogInfo(msg);
    }

    const float* vpData = camera.viewProj.Data();
    const auto lightViewProj =
        BuildDirectionalLightViewProjection(lighting, m_Impl->shadowSettings);
    const float* lightVpData = lightViewProj.Data();

    auto ensureShadowResources = [&]() -> bool
    {
        using namespace Diligent;

        const auto resolution = m_Impl->shadowSettings.resolution;
        if (resolution == 0)
            return false;

        const bool needsRecreate =
            !m_Impl->shadow.texture ||
            m_Impl->shadow.width != resolution ||
            m_Impl->shadow.height != resolution;

        if (needsRecreate)
        {
            if (m_Impl->shadow.texture)
            {
                ++m_Impl->shadow.recreationCount;
                ++m_Impl->currentFrameDiagnostics.shadowResourceRecreationCount;
                m_Impl->shadow.dsv.Release();
                m_Impl->shadow.srv.Release();
                m_Impl->shadow.texture.Release();
            }

            TextureDesc texDesc;
            texDesc.Name      = "DirectionalShadowMap";
            texDesc.Type      = RESOURCE_DIM_TEX_2D;
            texDesc.Width     = resolution;
            texDesc.Height    = resolution;
            texDesc.Format    = TEX_FORMAT_D32_FLOAT;
            texDesc.BindFlags = BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE;
            texDesc.Usage     = USAGE_DEFAULT;
            texDesc.MipLevels = 1;

            m_Impl->device->CreateTexture(
                texDesc, nullptr, &m_Impl->shadow.texture);
            if (!m_Impl->shadow.texture)
            {
                LogErr("Submit: failed to create directional shadow map");
                return false;
            }

            m_Impl->shadow.dsv = m_Impl->shadow.texture->GetDefaultView(
                TEXTURE_VIEW_DEPTH_STENCIL);
            m_Impl->shadow.srv = m_Impl->shadow.texture->GetDefaultView(
                TEXTURE_VIEW_SHADER_RESOURCE);
            if (!m_Impl->shadow.dsv || !m_Impl->shadow.srv)
            {
                LogErr("Submit: failed to create directional shadow views");
                m_Impl->shadow.dsv.Release();
                m_Impl->shadow.srv.Release();
                m_Impl->shadow.texture.Release();
                return false;
            }

            m_Impl->shadow.width = resolution;
            m_Impl->shadow.height = resolution;
            m_Impl->shadow.format = TEX_FORMAT_D32_FLOAT;
            ++m_Impl->shadow.creationCount;
            ++m_Impl->currentFrameDiagnostics.shadowResourceCreationCount;
        }

        if (!m_Impl->shadow.depthPSO)
        {
            m_Impl->shadow.depthPSO = CreateShadowDepthPSO(
                *m_Impl->device,
                m_Impl->shadow.depthVS,
                m_Impl->cameraCB,
                m_Impl->shadow.format);
            if (!m_Impl->shadow.depthPSO)
                return false;
        }

        m_Impl->currentFrameDiagnostics.shadowMapWidth = m_Impl->shadow.width;
        m_Impl->currentFrameDiagnostics.shadowMapHeight = m_Impl->shadow.height;
        return true;
    };

    auto uploadCameraCB = [&](const SagaEngine::Math::Mat4& model,
                              const NormalMatrixRows& normalRows,
                              float shadingModel,
                              bool shadowsEnabled)
    {
        Diligent::MapHelper<float> cbData(ctx, m_Impl->cameraCB,
                                          Diligent::MAP_WRITE,
                                          Diligent::MAP_FLAG_DISCARD);
        for (int i = 0; i < 16; ++i) cbData[i] = vpData[i];
        const float* mData = model.Data();
        for (int i = 0; i < 16; ++i) cbData[16 + i] = mData[i];
        for (int i = 0; i < 12; ++i) cbData[32 + i] = normalRows.rows[i];

        cbData[44] = lighting.directional.direction.x;
        cbData[45] = lighting.directional.direction.y;
        cbData[46] = lighting.directional.direction.z;
        cbData[47] = lighting.directionalEnabled ? 1.0f : 0.0f;

        cbData[48] = lighting.directional.color.x;
        cbData[49] = lighting.directional.color.y;
        cbData[50] = lighting.directional.color.z;
        cbData[51] = lighting.directional.intensity;

        cbData[52] = lighting.ambient.color.x;
        cbData[53] = lighting.ambient.color.y;
        cbData[54] = lighting.ambient.color.z;
        cbData[55] = lighting.ambient.intensity;

        for (int i = 0; i < 16; ++i) cbData[56 + i] = lightVpData[i];

        cbData[72] = m_Impl->shadowSettings.constantBias;
        cbData[73] = m_Impl->shadowSettings.normalBias;
        cbData[74] = m_Impl->shadow.width > 0
            ? 1.0f / static_cast<float>(m_Impl->shadow.width)
            : 1.0f / static_cast<float>(m_Impl->shadowSettings.resolution);
        cbData[75] = shadowsEnabled ? 1.0f : 0.0f;

        cbData[76] = shadingModel;
        cbData[77] = static_cast<float>(m_Impl->shadowSettings.pcfRadius);
        cbData[78] = 0.0f;
        cbData[79] = 0.0f;
    };

    auto bindMeshBuffers = [&](const MeshGPU& mesh,
                               Diligent::IBuffer*& lastVB,
                               Diligent::IBuffer*& lastIB)
    {
        auto* vb = m_Impl->nativeResources.ResolveBuffer(mesh.vertexBuffer);
        if (!vb)
        {
            return;
        }
        m_Impl->nativeResources.MarkBufferUsed(
            mesh.vertexBuffer,
            m_Impl->activeFrameSerial);
        if (vb != lastVB)
        {
            Diligent::Uint64 offsets[] = {0};
            ctx->SetVertexBuffers(0, 1, &vb, offsets,
                                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                  Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            lastVB = vb;
        }

        if (mesh.indexBuffer.IsValid() && mesh.indexCount > 0)
        {
            auto* ib = m_Impl->nativeResources.ResolveBuffer(mesh.indexBuffer);
            if (!ib)
            {
                return;
            }
            m_Impl->nativeResources.MarkBufferUsed(
                mesh.indexBuffer,
                m_Impl->activeFrameSerial);
            if (ib != lastIB)
            {
                ctx->SetIndexBuffer(ib, 0,
                                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                lastIB = ib;
            }
        }
    };

    if (shadowRequested && ensureShadowResources())
    {
        using namespace Diligent;

        ctx->SetRenderTargets(
            0, nullptr, m_Impl->shadow.dsv,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Viewport shadowVp{
            0.0f, 0.0f,
            static_cast<float>(m_Impl->shadow.width),
            static_cast<float>(m_Impl->shadow.height),
            0.0f, 1.0f};
        ctx->SetViewports(1, &shadowVp, m_Impl->shadow.width, m_Impl->shadow.height);
        Rect shadowScissor{
            0, 0,
            static_cast<Int32>(m_Impl->shadow.width),
            static_cast<Int32>(m_Impl->shadow.height)};
        ctx->SetScissorRects(1, &shadowScissor, m_Impl->shadow.width, m_Impl->shadow.height);
        ctx->ClearDepthStencil(
            m_Impl->shadow.dsv,
            CLEAR_DEPTH_FLAG,
            1.0f, 0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->SetPipelineState(m_Impl->shadow.depthPSO);

        IBuffer* lastShadowVB = nullptr;
        IBuffer* lastShadowIB = nullptr;
        for (const auto& item : view.drawItems)
        {
            auto meshIt = m_Impl->meshCache.find(item.mesh);
            auto matIt = m_Impl->materialCache.find(item.material);
            if (meshIt == m_Impl->meshCache.end() ||
                matIt == m_Impl->materialCache.end() ||
                !matIt->second.castsShadows)
            {
                continue;
            }

            NormalMatrixRows normalRows{};
            uploadCameraCB(item.model, normalRows, 0.0f, false);
            bindMeshBuffers(meshIt->second, lastShadowVB, lastShadowIB);

            if (meshIt->second.indexBuffer.IsValid() &&
                meshIt->second.indexCount > 0)
            {
                DrawIndexedAttribs drawAttribs;
                drawAttribs.NumIndices = meshIt->second.indexCount;
                drawAttribs.IndexType  = VT_UINT32;
                drawAttribs.Flags      = g_verboseGPU ? DRAW_FLAG_VERIFY_ALL
                                                      : DRAW_FLAG_NONE;
                ctx->DrawIndexed(drawAttribs);
                ++m_Impl->currentFrameDiagnostics.shadowPassDrawCalls;
            }
        }

        ++m_Impl->currentFrameDiagnostics.shadowPassExecuted;

        auto* rtv = m_Impl->swapChain->GetCurrentBackBufferRTV();
        auto* dsv = m_Impl->swapChain->GetDepthBufferDSV();
        ctx->SetRenderTargets(
            rtv ? 1u : 0u, rtv ? &rtv : nullptr, dsv,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->SetViewports(1, nullptr, 0, 0);
        ctx->SetScissorRects(0, nullptr, 0, 0);
    }

    // ── Draw loop ────────────────────────────────────────────────────
    Diligent::IPipelineState* lastPSO = nullptr;
    Diligent::IBuffer*        lastVB  = nullptr;
    Diligent::IBuffer*        lastIB  = nullptr;

    int drawIdx = 0;
    for (const auto& item : view.drawItems)
    {
        auto meshIt = m_Impl->meshCache.find(item.mesh);
        if (meshIt == m_Impl->meshCache.end())
        {
            LogDbg("Submit: mesh not in cache, skip");
            ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
            continue;
        }

        auto matIt = m_Impl->materialCache.find(item.material);
        if (matIt == m_Impl->materialCache.end())
        {
            LogDbg("Submit: material not in cache, skip");
            ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
            continue;
        }

        auto& mesh = meshIt->second;
        auto& mat  = matIt->second;

        NormalMatrixRows normalRows{};
        if (mat.shadingModel == OpaqueShadingModel::LitDiffuse &&
            !ComputeNormalMatrixRows(item.model, normalRows))
        {
            LogDbg("Submit: singular normal transform, skip lit draw");
            ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
            ++m_Impl->currentFrameDiagnostics.invalidNormalTransformDraws;
            continue;
        }

        ++m_Impl->currentFrameDiagnostics.submittedDrawItems;

        if (g_verboseGPU)
        {
            auto* dbgVB = m_Impl->nativeResources.ResolveBuffer(mesh.vertexBuffer);
            auto* dbgIB = m_Impl->nativeResources.ResolveBuffer(mesh.indexBuffer);
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "Submit[%d]: verts=%u idx=%u stride=%u pso=%p srb=%p vb=%p ib=%p",
                drawIdx, mesh.vertexCount, mesh.indexCount, mesh.vertexStride,
                static_cast<void*>(mat.pso.RawPtr()),
                static_cast<void*>(mat.srb.RawPtr()),
                static_cast<void*>(dbgVB),
                static_cast<void*>(dbgIB));
            LogDbg(buf);
        }

        const bool sampleShadows =
            shadowRequested &&
            mat.shadingModel == OpaqueShadingModel::LitDiffuse &&
            mat.receivesShadows &&
            m_Impl->shadow.srv;
        uploadCameraCB(
            item.model,
            normalRows,
            mat.shadingModel == OpaqueShadingModel::LitDiffuse ? 1.0f : 0.0f,
            sampleShadows);
        if (sampleShadows)
            ++m_Impl->currentFrameDiagnostics.shadowSamplingEnabled;
        LogDbg("Submit: CameraCB mapped OK");

        // ── Skinned draw? Upload bone palette to BoneCB. ────────────
        const bool isSkinned = (item.boneMatrices != nullptr && item.boneCount > 0);
        if (isSkinned)
        {
            Diligent::MapHelper<float> boneData(ctx, m_Impl->boneCB,
                                                 Diligent::MAP_WRITE,
                                                 Diligent::MAP_FLAG_DISCARD);
            // Copy bone matrices (each Mat4 = 16 floats = 64 bytes).
            const std::size_t floatCount = static_cast<std::size_t>(item.boneCount) * 16;
            std::memcpy(&boneData[0], item.boneMatrices, floatCount * sizeof(float));
            // Zero remaining slots so the shader reads identity-ish data
            // if any index overshoots (defensive).
            if (item.boneCount < 128)
            {
                const std::size_t remaining = (128 - item.boneCount) * 16;
                std::memset(&boneData[floatCount], 0, remaining * sizeof(float));
            }
            LogDbg("Submit: BoneCB uploaded");

            // Lazily create skinned PSO + SRB for this material if needed.
            if (!mat.skinnedPso)
            {
                PSOCacheKey key{};
                key.cullMode    = static_cast<std::uint8_t>(mat.cullMode);
                key.renderQueue = static_cast<std::uint8_t>(mat.renderQueue);
                key.writesDepth = mat.writesDepth;

                mat.skinnedPso = FindOrCreateSkinnedPSO(
                    m_Impl->skinnedPsoCache, *m_Impl->device, *m_Impl->swapChain,
                    m_Impl->skinnedVS, m_Impl->solidPS,
                    m_Impl->cameraCB, m_Impl->boneCB, key);

                if (mat.skinnedPso)
                {
                    mat.skinnedPso->CreateShaderResourceBinding(&mat.skinnedSrb, true);
                    if (mat.skinnedSrb)
                    {
                        mat.skinnedAlbedoVariable =
                            mat.skinnedSrb->GetVariableByName(
                                Diligent::SHADER_TYPE_PIXEL, "g_Albedo");
                        mat.skinnedShadowVariable =
                            mat.skinnedSrb->GetVariableByName(
                                Diligent::SHADER_TYPE_PIXEL, "g_ShadowMap");
                    }
                }
            }
        }

        // Choose PSO and SRB based on skinning.
        Diligent::IPipelineState*         activePso = nullptr;
        Diligent::IShaderResourceBinding* activeSrb = nullptr;
        Diligent::IShaderResourceVariable* albedoVariable = nullptr;
        Diligent::IShaderResourceVariable* shadowVariable = nullptr;

        if (isSkinned && mat.skinnedPso && mat.skinnedSrb)
        {
            activePso = mat.skinnedPso.RawPtr();
            activeSrb = mat.skinnedSrb.RawPtr();
            albedoVariable = mat.skinnedAlbedoVariable;
            shadowVariable = mat.skinnedShadowVariable;
        }
        else
        {
            activePso = mat.pso.RawPtr();
            activeSrb = mat.srb.RawPtr();
            albedoVariable = mat.albedoVariable;
            shadowVariable = mat.shadowVariable;
        }

        // Bind albedo texture SRV on the active SRB.
        {
            Diligent::ITextureView* texSRV = nullptr;
            if (mat.albedoTex != TextureHandle::kInvalid)
            {
                auto texIt = m_Impl->textureCache.find(mat.albedoTex);
                if (texIt != m_Impl->textureCache.end())
                {
                    texSRV = m_Impl->nativeResources.ResolveTextureSrv(
                        texIt->second.texture);
                    if (texSRV)
                    {
                        m_Impl->nativeResources.MarkTextureUsed(
                            texIt->second.texture,
                            m_Impl->activeFrameSerial);
                    }
                }
            }
            if (!texSRV)
            {
                texSRV = m_Impl->nativeResources.ResolveTextureSrv(
                    m_Impl->defaultWhiteTex);
                if (texSRV)
                {
                    m_Impl->nativeResources.MarkTextureUsed(
                        m_Impl->defaultWhiteTex,
                        m_Impl->activeFrameSerial);
                }
            }

            if (texSRV && albedoVariable)
            {
                albedoVariable->Set(texSRV);
            }

            if (shadowVariable)
            {
                if (m_Impl->shadow.srv)
                    shadowVariable->Set(m_Impl->shadow.srv);
                else if (texSRV)
                    shadowVariable->Set(texSRV);
            }
        }

        // PSO switch
        if (activePso != lastPSO)
        {
            ctx->SetPipelineState(activePso);
            lastPSO = activePso;
            LogDbg("Submit: SetPipelineState OK");
        }

        // SRB
        ctx->CommitShaderResources(activeSrb,
                                   Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        LogDbg("Submit: CommitShaderResources OK");

        // VB
        auto* vb = m_Impl->nativeResources.ResolveBuffer(mesh.vertexBuffer);
        if (!vb)
        {
            ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
            continue;
        }
        m_Impl->nativeResources.MarkBufferUsed(
            mesh.vertexBuffer,
            m_Impl->activeFrameSerial);
        if (vb != lastVB)
        {
            Diligent::Uint64 offsets[] = {0};
            ctx->SetVertexBuffers(0, 1, &vb, offsets,
                                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                  Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            lastVB = vb;
            LogDbg("Submit: SetVertexBuffers OK");
        }

        // IB + Draw
        if (mesh.indexBuffer.IsValid() && mesh.indexCount > 0)
        {
            auto* ib = m_Impl->nativeResources.ResolveBuffer(mesh.indexBuffer);
            if (!ib)
            {
                ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
                continue;
            }
            m_Impl->nativeResources.MarkBufferUsed(
                mesh.indexBuffer,
                m_Impl->activeFrameSerial);
            if (ib != lastIB)
            {
                ctx->SetIndexBuffer(ib, 0,
                                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                lastIB = ib;
                LogDbg("Submit: SetIndexBuffer OK");
            }

            Diligent::DrawIndexedAttribs drawAttribs;
            drawAttribs.NumIndices = mesh.indexCount;
            drawAttribs.IndexType  = Diligent::VT_UINT32;
            drawAttribs.Flags      = g_verboseGPU ? Diligent::DRAW_FLAG_VERIFY_ALL
                                                  : Diligent::DRAW_FLAG_NONE;
            ctx->DrawIndexed(drawAttribs);
            ++m_Impl->currentFrameDiagnostics.indexedDrawCalls;
            ++m_Impl->currentFrameDiagnostics.mainPassDrawCalls;
            m_Impl->currentFrameDiagnostics.lastIndexedIndexCount = mesh.indexCount;
            LogDbg("Submit: DrawIndexed OK");
        }
        else
        {
            Diligent::DrawAttribs drawAttribs;
            drawAttribs.NumVertices = mesh.vertexCount;
            drawAttribs.Flags       = g_verboseGPU ? Diligent::DRAW_FLAG_VERIFY_ALL
                                                   : Diligent::DRAW_FLAG_NONE;
            ctx->Draw(drawAttribs);
            ++m_Impl->currentFrameDiagnostics.nonIndexedDrawCalls;
            ++m_Impl->currentFrameDiagnostics.mainPassDrawCalls;
            LogDbg("Submit: Draw OK");
        }

        ++drawIdx;
    }
    LogDbg("Submit: done");
}

void DiligentRenderBackend::EndFrame()
{
    if (!m_Impl->initialized || !m_Impl->swapChain) return;

    if (m_Impl->activeFrameSerial != 0u)
    {
        m_Impl->gpuTimeline.SignalFrameSubmitted(m_Impl->activeFrameSerial);
    }
    LogDbg("EndFrame: Present…");
    m_Impl->swapChain->Present();
    if (!m_Impl->frameSlotSerials.empty())
    {
        m_Impl->frameSlotSerials[m_Impl->activeFrameSlot] =
            m_Impl->activeFrameSerial;
    }
    ++m_Impl->currentFrameDiagnostics.presentCalls;
    {
        const auto timeline = m_Impl->gpuTimeline.Diagnostics();
        m_Impl->currentFrameDiagnostics.gpuCompletedSerial =
            timeline.lastCompletedSerial;
        m_Impl->currentFrameDiagnostics.gpuTargetedWaitCount =
            timeline.targetedWaitCount;
        m_Impl->currentFrameDiagnostics.gpuDeviceWideWaitCount =
            timeline.deviceWideWaitCount;
        m_Impl->currentFrameDiagnostics.gpuTimelinePollCount =
            timeline.pollCount;
        m_Impl->currentFrameDiagnostics.gpuTimelineSignalCount =
            timeline.signalCount;
    }
    m_Impl->lastFrameDiagnostics = m_Impl->currentFrameDiagnostics;
    ++m_Impl->frameIndex;
    m_Impl->activeFrameSerial = 0u;

    if (g_verboseGPU)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "EndFrame: frame %llu done",
                      static_cast<unsigned long long>(m_Impl->frameIndex));
        LogDbg(buf);
    }
}

// ─── Overlay rendering ───────────────────────────────────────────────

bool DiligentRenderBackend::InitOverlayRendering()
{
    if (!m_Impl || !m_Impl->initialized) return false;
    return m_Impl->overlayRenderer.Initialize(
        GetDiligentDeviceServices(),
        m_Impl->nativeResources,
        static_cast<std::uint32_t>(m_Impl->frameSlotSerials.size()));
}

RenderOverlayTextureHandle DiligentRenderBackend::CreateOverlayTexture(
    const RenderOverlayTextureDesc& desc,
    const std::uint8_t* rgbaPixels)
{
    if (!m_Impl || !m_Impl->initialized) return {};
    if (!m_Impl->overlayRenderer.IsReady() && !InitOverlayRendering())
    {
        return {};
    }

    return m_Impl->overlayRenderer.CreateTexture(desc, rgbaPixels);
}

void DiligentRenderBackend::DestroyOverlayTexture(
    RenderOverlayTextureHandle texture)
{
    if (!m_Impl) return;
    m_Impl->overlayRenderer.DestroyTexture(texture);
}

void DiligentRenderBackend::RenderOverlayFrame(
    const ::SagaEngine::Render::Backend::RenderOverlayFrame& frame)
{
    if (!m_Impl || !m_Impl->initialized) return;
    m_Impl->overlayRenderer.Render(
        frame,
        m_Impl->activeFrameSerial,
        m_Impl->activeFrameSlot);
}

void DiligentRenderBackend::ShutdownOverlayRendering()
{
    if (!m_Impl) return;
    m_Impl->overlayRenderer.Shutdown();
}

RenderCaptureResult DiligentRenderBackend::CaptureCurrentColorFrame(
    RenderFrameCapture& outCapture)
{
    if (!m_Impl || !m_Impl->initialized)
    {
        outCapture = {};
        return RenderCaptureResult::kNotInitialized;
    }

    return m_Impl->frameCapture.Capture(
        GetDiligentDeviceServices(), outCapture);
}

// ─── Diagnostics ─────────────────────────────────────────────────────

GraphicsBackendAPI DiligentRenderBackend::SelectedAPI() const noexcept
{
    return m_Impl ? m_Impl->selectedAPI : GraphicsBackendAPI::kAuto;
}

std::uint64_t DiligentRenderBackend::FrameIndex() const noexcept
{
    return m_Impl ? m_Impl->frameIndex : 0ull;
}

bool DiligentRenderBackend::IsInitialized() const noexcept
{
    return m_Impl && m_Impl->initialized;
}

DiligentDeviceServices DiligentRenderBackend::GetDiligentDeviceServices()
    const noexcept
{
    if (!m_Impl || !m_Impl->initialized)
    {
        return {};
    }

    return {
        m_Impl->device.RawPtr(),
        m_Impl->context.RawPtr(),
        m_Impl->swapChain.RawPtr(),
    };
}

RenderFrameDiagnostics DiligentRenderBackend::LastFrameDiagnostics() const noexcept
{
    return m_Impl ? m_Impl->lastFrameDiagnostics : RenderFrameDiagnostics{};
}

// ─── Public factory surface ─────────────────────────────────────────

std::unique_ptr<IRenderBackend> CreateRenderBackend()
{
    return std::make_unique<DiligentRenderBackend>();
}

std::unique_ptr<IRenderBackend> CreateRenderBackend(
    RenderBackendConfig config)
{
    return std::make_unique<DiligentRenderBackend>(config);
}

RenderBackendStatus GetRenderBackendStatus(
    const IRenderBackend& backend) noexcept
{
    const auto* diligent = dynamic_cast<const DiligentRenderBackend*>(&backend);
    if (!diligent)
    {
        return {};
    }

    return {
        diligent->SelectedAPI(),
        diligent->FrameIndex(),
        diligent->IsInitialized(),
    };
}

bool InitBackendOverlayRendering(IRenderBackend& backend)
{
    auto* diligent = dynamic_cast<DiligentRenderBackend*>(&backend);
    return diligent && diligent->InitOverlayRendering();
}

RenderOverlayTextureHandle CreateBackendOverlayTexture(
    IRenderBackend& backend,
    const RenderOverlayTextureDesc& desc,
    const std::uint8_t* rgbaPixels)
{
    auto* diligent = dynamic_cast<DiligentRenderBackend*>(&backend);
    if (!diligent)
    {
        return {};
    }

    return diligent->CreateOverlayTexture(desc, rgbaPixels);
}

void DestroyBackendOverlayTexture(
    IRenderBackend& backend,
    RenderOverlayTextureHandle texture)
{
    auto* diligent = dynamic_cast<DiligentRenderBackend*>(&backend);
    if (diligent)
    {
        diligent->DestroyOverlayTexture(texture);
    }
}

void RenderBackendOverlayFrame(
    IRenderBackend& backend,
    const RenderOverlayFrame& frame)
{
    auto* diligent = dynamic_cast<DiligentRenderBackend*>(&backend);
    if (diligent)
    {
        diligent->RenderOverlayFrame(frame);
    }
}

void ShutdownBackendOverlayRendering(IRenderBackend& backend)
{
    auto* diligent = dynamic_cast<DiligentRenderBackend*>(&backend);
    if (diligent)
    {
        diligent->ShutdownOverlayRendering();
    }
}

} // namespace SagaEngine::Render::Backend
