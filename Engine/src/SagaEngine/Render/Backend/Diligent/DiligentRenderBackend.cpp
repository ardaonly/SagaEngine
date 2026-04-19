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
#include "SagaEngine/Render/Materials/MeshAsset.h"
#include "SagaEngine/Render/Materials/Material.h"

#include <imgui.h>

#include <algorithm>
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

// ─── String form of API enum ──────────────────────────────────────────

std::string_view ToString(DiligentBackendAPI api) noexcept
{
    switch (api)
    {
        case DiligentBackendAPI::kAuto:   return "Auto";
        case DiligentBackendAPI::kD3D12:  return "D3D12";
        case DiligentBackendAPI::kVulkan: return "Vulkan";
        case DiligentBackendAPI::kD3D11:  return "D3D11";
        case DiligentBackendAPI::kOpenGL: return "OpenGL";
    }
    return "Unknown";
}

// ─── GPU resource types ──────────────────────────────────────────────

struct MeshGPU
{
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer;
    Diligent::Uint32  vertexStride = 0;
    Diligent::Uint32  vertexCount  = 0;
    Diligent::Uint32  indexCount   = 0;
};

struct PSOCacheKey
{
    std::uint8_t  cullMode    = 0;   // MaterialCullMode
    std::uint8_t  renderQueue = 0;   // MaterialRenderQueue
    bool          writesDepth = true;

    bool operator==(const PSOCacheKey&) const = default;
};

struct PSOCacheKeyHash
{
    std::size_t operator()(const PSOCacheKey& k) const noexcept
    {
        // Simple hash — few unique keys expected.
        return std::size_t(k.cullMode) | (std::size_t(k.renderQueue) << 8)
             | (std::size_t(k.writesDepth) << 16);
    }
};

struct TextureGPU
{
    Diligent::RefCntAutoPtr<Diligent::ITexture>     texture;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
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
    // Skinned variant (created on demand when first skinned draw uses this material).
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         skinnedPso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> skinnedSrb;
    MaterialRenderQueue renderQueue = MaterialRenderQueue::Opaque;
    MaterialCullMode    cullMode   = MaterialCullMode::Back;
    bool                writesDepth = true;
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

// ─── Impl ─────────────────────────────────────────────────────────────

struct DiligentRenderBackend::Impl
{
    DiligentBackendConfig                    config{};
    DiligentBackendAPI                       selectedAPI = DiligentBackendAPI::kAuto;

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>  device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>     swapChain;

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
    TextureGPU defaultWhiteTex;   // 1x1 white fallback
    std::uint32_t nextTextureId = 1;

    // ── ID generators ───────────────────────────────────────────────
    std::uint32_t nextMeshId     = 1;
    std::uint32_t nextMaterialId = 1;

    std::uint64_t frameIndex   = 0;
    bool          initialized  = false;

    // ── ImGui rendering resources ───────────────────────────────────
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>        imguiPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> imguiSRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>               imguiCB;       // projection matrix
    Diligent::RefCntAutoPtr<Diligent::IBuffer>               imguiVB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>               imguiIB;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>          imguiFontSRV;
    Diligent::RefCntAutoPtr<Diligent::ITexture>              imguiFontTex;
    std::uint32_t imguiVBSize = 0;   // current VB capacity in bytes
    std::uint32_t imguiIBSize = 0;   // current IB capacity in bytes
    bool          imguiReady  = false;
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

// ─── Factory init helpers (D3D12, Vulkan, D3D11, OpenGL) ─────────────

#include "DiligentFactoryInit.inl"

// ─── Embedded HLSL shader source ─────────────────────────────────────

#include "DiligentShaders.inl"

// ─── Embedded HLSL for ImGui overlay ────────────────────────────────

#include "DiligentImGuiShaders.inl"

} // anonymous namespace

// ─── Construction / destruction ──────────────────────────────────────

DiligentRenderBackend::DiligentRenderBackend()
    : m_Impl(std::make_unique<Impl>()) {}

DiligentRenderBackend::DiligentRenderBackend(DiligentBackendConfig cfg)
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

    if (picked == DiligentBackendAPI::kAuto)
    {
        LogErr("No Diligent graphics API was available for the host");
        return false;
    }

    m_Impl->selectedAPI = picked;
    m_Impl->initialized = true;
    m_Impl->frameIndex  = 0;

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
        cbDesc.Size           = sizeof(float) * 40;  // g_ViewProj(16) + g_Model(16) + g_LightDir(4) + g_LightColor(4)
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
        using namespace Diligent;

        const std::uint8_t white[4] = { 255, 255, 255, 255 };

        TextureDesc texDesc;
        texDesc.Name      = "DefaultWhite1x1";
        texDesc.Type      = RESOURCE_DIM_TEX_2D;
        texDesc.Width     = 1;
        texDesc.Height    = 1;
        texDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        texDesc.Usage     = USAGE_IMMUTABLE;
        texDesc.MipLevels = 1;

        TextureSubResData subRes;
        subRes.pData  = white;
        subRes.Stride = 4;

        TextureData texData;
        texData.pSubResources   = &subRes;
        texData.NumSubresources = 1;

        m_Impl->device->CreateTexture(texDesc, &texData, &m_Impl->defaultWhiteTex.texture);
        if (!m_Impl->defaultWhiteTex.texture)
        {
            LogErr("Failed to create default white texture");
            return false;
        }
        m_Impl->defaultWhiteTex.srv =
            m_Impl->defaultWhiteTex.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        LogDbg("Default 1x1 white texture created");
    }

    LogInfo("Phase 3: backend ready");
    return true;
}

void DiligentRenderBackend::Shutdown()
{
    if (!m_Impl || !m_Impl->initialized) return;

    ShutdownImGuiRendering();

    m_Impl->meshCache.clear();
    m_Impl->materialCache.clear();
    m_Impl->psoCache.clear();
    m_Impl->skinnedPsoCache.clear();
    m_Impl->textureCache.clear();
    m_Impl->defaultWhiteTex.srv.Release();
    m_Impl->defaultWhiteTex.texture.Release();

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
    m_Impl->selectedAPI = DiligentBackendAPI::kAuto;
    LogInfo("Shutdown complete");
}

void DiligentRenderBackend::OnResize(std::uint32_t width, std::uint32_t height)
{
    if (!m_Impl->initialized || !m_Impl->swapChain) return;
    if (width == 0 || height == 0) return;
    m_Impl->swapChain->Resize(width, height);
}

// ─── PSO helper (private to this TU) ────────────────────────────────

namespace
{

/// Creates or retrieves a cached PSO for the given key.
/// Takes raw Diligent pointers to avoid referencing the private Impl type
/// from the anonymous namespace (same fix as Phase 2 triangle helpers).
Diligent::RefCntAutoPtr<Diligent::IPipelineState> FindOrCreatePSO(
    std::unordered_map<PSOCacheKey, Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PSOCacheKeyHash>& cache,
    Diligent::IRenderDevice&  device,
    Diligent::ISwapChain&     swapChain,
    Diligent::IShader*        solidVS,
    Diligent::IShader*        solidPS,
    Diligent::IBuffer*        cameraCB,
    const PSOCacheKey&        key)
{
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    using namespace Diligent;

    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name         = "SolidPSO";
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    psoCI.pVS                  = solidVS;
    psoCI.pPS                  = solidPS;

    const auto& scDesc = swapChain.GetDesc();
    psoCI.GraphicsPipeline.NumRenderTargets  = 1;
    psoCI.GraphicsPipeline.RTVFormats[0]     = scDesc.ColorBufferFormat;
    psoCI.GraphicsPipeline.DSVFormat         = scDesc.DepthBufferFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Depth
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable      = True;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable  = key.writesDepth ? True : False;

    // Cull — CCW front face (standard 3D convention; matches glTF, OBJ, FBX).
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
    switch (static_cast<MaterialCullMode>(key.cullMode))
    {
        case MaterialCullMode::Back:  psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;  break;
        case MaterialCullMode::Front: psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_FRONT; break;
        case MaterialCullMode::None:  psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;  break;
    }

    // Input layout matching MeshVertex (76 bytes):
    //   pos(3f) normal(3f) tangent(3f) handedness(1f) uv0(2f) uv1(2f)
    //   boneIndices(4u8) boneWeights(4f)
    LayoutElement layoutElems[] =
    {
        {0, 0, 3, VT_FLOAT32, False},  // Pos
        {1, 0, 3, VT_FLOAT32, False},  // Normal
        {2, 0, 3, VT_FLOAT32, False},  // Tangent
        {3, 0, 1, VT_FLOAT32, False},  // Handedness
        {4, 0, 2, VT_FLOAT32, False},  // UV0
        {5, 0, 2, VT_FLOAT32, False},  // UV1
        {6, 0, 4, VT_UINT8,   False},  // BoneIndices
        {7, 0, 4, VT_FLOAT32, False},  // BoneWeights
    };
    psoCI.GraphicsPipeline.InputLayout.NumElements    = 8;
    psoCI.GraphicsPipeline.InputLayout.LayoutElements  = layoutElems;

    // Texture/sampler resource layout: g_Albedo as DYNAMIC (changes per material),
    // g_Sampler as immutable sampler with linear filtering.
    ShaderResourceVariableDesc vars[] =
    {
        { SHADER_TYPE_PIXEL, "g_Albedo", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };
    psoCI.PSODesc.ResourceLayout.Variables    = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = 1;

    SamplerDesc sceneSamplerDesc;
    sceneSamplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.AddressU  = TEXTURE_ADDRESS_WRAP;
    sceneSamplerDesc.AddressV  = TEXTURE_ADDRESS_WRAP;
    sceneSamplerDesc.AddressW  = TEXTURE_ADDRESS_WRAP;

    ImmutableSamplerDesc samplers[] =
    {
        { SHADER_TYPE_PIXEL, "g_Sampler", sceneSamplerDesc },
    };
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers    = samplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 1;

    RefCntAutoPtr<IPipelineState> pso;
    device.CreateGraphicsPipelineState(psoCI, &pso);
    if (!pso)
    {
        LogErr("Failed to create PSO");
        return {};
    }

    // Bind CameraCB as a static variable on VS and PS.
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "CameraCB"))
        var->Set(cameraCB);
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "CameraCB"))
        var->Set(cameraCB);

    cache[key] = pso;
    return pso;
}

/// Same as FindOrCreatePSO but uses the skinned VS and binds BoneCB.
Diligent::RefCntAutoPtr<Diligent::IPipelineState> FindOrCreateSkinnedPSO(
    std::unordered_map<PSOCacheKey, Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PSOCacheKeyHash>& cache,
    Diligent::IRenderDevice&  device,
    Diligent::ISwapChain&     swapChain,
    Diligent::IShader*        skinnedVS,
    Diligent::IShader*        solidPS,
    Diligent::IBuffer*        cameraCB,
    Diligent::IBuffer*        boneCB,
    const PSOCacheKey&        key)
{
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    using namespace Diligent;

    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name         = "SkinnedPSO";
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    psoCI.pVS                  = skinnedVS;
    psoCI.pPS                  = solidPS;

    const auto& scDesc = swapChain.GetDesc();
    psoCI.GraphicsPipeline.NumRenderTargets  = 1;
    psoCI.GraphicsPipeline.RTVFormats[0]     = scDesc.ColorBufferFormat;
    psoCI.GraphicsPipeline.DSVFormat         = scDesc.DepthBufferFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable      = True;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable  = key.writesDepth ? True : False;

    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
    switch (static_cast<MaterialCullMode>(key.cullMode))
    {
        case MaterialCullMode::Back:  psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;  break;
        case MaterialCullMode::Front: psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_FRONT; break;
        case MaterialCullMode::None:  psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;  break;
    }

    LayoutElement layoutElems[] =
    {
        {0, 0, 3, VT_FLOAT32, False},  // Pos
        {1, 0, 3, VT_FLOAT32, False},  // Normal
        {2, 0, 3, VT_FLOAT32, False},  // Tangent
        {3, 0, 1, VT_FLOAT32, False},  // Handedness
        {4, 0, 2, VT_FLOAT32, False},  // UV0
        {5, 0, 2, VT_FLOAT32, False},  // UV1
        {6, 0, 4, VT_UINT8,   False},  // BoneIndices
        {7, 0, 4, VT_FLOAT32, False},  // BoneWeights
    };
    psoCI.GraphicsPipeline.InputLayout.NumElements    = 8;
    psoCI.GraphicsPipeline.InputLayout.LayoutElements  = layoutElems;

    ShaderResourceVariableDesc vars[] =
    {
        { SHADER_TYPE_PIXEL, "g_Albedo", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };
    psoCI.PSODesc.ResourceLayout.Variables    = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = 1;

    SamplerDesc sceneSamplerDesc;
    sceneSamplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.AddressU  = TEXTURE_ADDRESS_WRAP;
    sceneSamplerDesc.AddressV  = TEXTURE_ADDRESS_WRAP;
    sceneSamplerDesc.AddressW  = TEXTURE_ADDRESS_WRAP;

    ImmutableSamplerDesc samplers[] =
    {
        { SHADER_TYPE_PIXEL, "g_Sampler", sceneSamplerDesc },
    };
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers    = samplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 1;

    RefCntAutoPtr<IPipelineState> pso;
    device.CreateGraphicsPipelineState(psoCI, &pso);
    if (!pso)
    {
        LogErr("Failed to create skinned PSO");
        return {};
    }

    // Bind CameraCB and BoneCB as static variables.
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "CameraCB"))
        var->Set(cameraCB);
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "CameraCB"))
        var->Set(cameraCB);
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "BoneCB"))
        var->Set(boneCB);

    cache[key] = pso;
    return pso;
}

} // anonymous namespace

// ─── Resource upload ─────────────────────────────────────────────────

World::MeshId DiligentRenderBackend::CreateMesh(const MeshAsset& asset)
{
    if (!m_Impl->initialized || !m_Impl->device) return World::MeshId::kInvalid;
    if (asset.lodCount == 0) return World::MeshId::kInvalid;

    const auto& lod = asset.lods[0];
    if (lod.vertices.empty()) return World::MeshId::kInvalid;

    using namespace Diligent;

    MeshGPU gpu{};
    gpu.vertexStride = static_cast<Uint32>(sizeof(MeshVertex));
    gpu.vertexCount  = static_cast<Uint32>(lod.vertices.size());

    // VB
    {
        BufferDesc vbDesc;
        vbDesc.Name      = "MeshVB";
        vbDesc.Size      = static_cast<Uint64>(gpu.vertexCount) * gpu.vertexStride;
        vbDesc.BindFlags = BIND_VERTEX_BUFFER;
        vbDesc.Usage     = USAGE_IMMUTABLE;

        BufferData vbData;
        vbData.pData    = lod.vertices.data();
        vbData.DataSize = vbDesc.Size;

        m_Impl->device->CreateBuffer(vbDesc, &vbData, &gpu.vertexBuffer);
        if (!gpu.vertexBuffer)
        {
            LogErr("CreateMesh: VB creation failed");
            return World::MeshId::kInvalid;
        }
    }

    // IB
    if (!lod.indices.empty())
    {
        gpu.indexCount = static_cast<Uint32>(lod.indices.size());

        BufferDesc ibDesc;
        ibDesc.Name      = "MeshIB";
        ibDesc.Size      = static_cast<Uint64>(gpu.indexCount) * sizeof(std::uint32_t);
        ibDesc.BindFlags = BIND_INDEX_BUFFER;
        ibDesc.Usage     = USAGE_IMMUTABLE;

        BufferData ibData;
        ibData.pData    = lod.indices.data();
        ibData.DataSize = ibDesc.Size;

        m_Impl->device->CreateBuffer(ibDesc, &ibData, &gpu.indexBuffer);
        if (!gpu.indexBuffer)
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
    gpu.writesDepth = runtime.writesDepth;
    gpu.albedoTex   = runtime.textures[static_cast<std::size_t>(MaterialTextureSlot::Albedo)];

    pso->CreateShaderResourceBinding(&gpu.srb, true);
    if (!gpu.srb)
    {
        LogErr("CreateMaterial: SRB creation failed");
        return World::MaterialId::kInvalid;
    }

    auto id = static_cast<World::MaterialId>(m_Impl->nextMaterialId++);
    m_Impl->materialCache[id] = std::move(gpu);
    return id;
}

void DiligentRenderBackend::DestroyMesh(World::MeshId id)
{
    m_Impl->meshCache.erase(id);
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

    using namespace Diligent;

    TextureDesc texDesc;
    texDesc.Name      = "UserTexture";
    texDesc.Type      = RESOURCE_DIM_TEX_2D;
    texDesc.Width     = width;
    texDesc.Height    = height;
    texDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;
    texDesc.Usage     = USAGE_IMMUTABLE;
    texDesc.MipLevels = 1;

    TextureSubResData subRes;
    subRes.pData  = rgba;
    subRes.Stride = static_cast<Uint64>(width) * 4;

    TextureData texData;
    texData.pSubResources   = &subRes;
    texData.NumSubresources = 1;

    TextureGPU gpu{};
    m_Impl->device->CreateTexture(texDesc, &texData, &gpu.texture);
    if (!gpu.texture)
    {
        LogErr("CreateTexture: texture creation failed");
        return TextureHandle::kInvalid;
    }
    gpu.srv = gpu.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

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
    m_Impl->textureCache.erase(tex);
}

// ─── Frame lifecycle ─────────────────────────────────────────────────

void DiligentRenderBackend::BeginFrame()
{
    if (!m_Impl->initialized || !m_Impl->context || !m_Impl->swapChain)
        return;

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

    {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "Submit: %zu draw item(s) on frame %llu",
                      view.drawItems.size(),
                      static_cast<unsigned long long>(m_Impl->frameIndex));
        LogDbg(msg);
        if (m_Impl->frameIndex == 0) LogInfo(msg);
    }

    const float* vpData = camera.viewProj.Data();

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
            continue;
        }

        auto matIt = m_Impl->materialCache.find(item.material);
        if (matIt == m_Impl->materialCache.end())
        {
            LogDbg("Submit: material not in cache, skip");
            continue;
        }

        auto& mesh = meshIt->second;
        auto& mat  = matIt->second;

        if (g_verboseGPU)
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "Submit[%d]: verts=%u idx=%u stride=%u pso=%p srb=%p vb=%p ib=%p",
                drawIdx, mesh.vertexCount, mesh.indexCount, mesh.vertexStride,
                static_cast<void*>(mat.pso.RawPtr()),
                static_cast<void*>(mat.srb.RawPtr()),
                static_cast<void*>(mesh.vertexBuffer.RawPtr()),
                static_cast<void*>(mesh.indexBuffer.RawPtr()));
            LogDbg(buf);
        }

        // Update CameraCB per DrawItem: g_ViewProj(16) + g_Model(16) + g_LightDir(4) + g_LightColor(4).
        {
            Diligent::MapHelper<float> cbData(ctx, m_Impl->cameraCB,
                                              Diligent::MAP_WRITE,
                                              Diligent::MAP_FLAG_DISCARD);
            for (int i = 0; i < 16; ++i) cbData[i]      = vpData[i];
            const float* mData = item.model.Data();
            for (int i = 0; i < 16; ++i) cbData[16 + i] = mData[i];

            // g_LightDir: world-space direction TO the light (normalised).
            // Default: sun from upper-right-front.
            cbData[32] =  0.5774f;   // x
            cbData[33] =  0.5774f;   // y
            cbData[34] =  0.5774f;   // z
            cbData[35] =  0.0f;      // w (padding)

            // g_LightColor: xyz = light colour, w = ambient strength.
            cbData[36] =  1.0f;      // R
            cbData[37] =  0.95f;     // G
            cbData[38] =  0.9f;      // B (warm white)
            cbData[39] =  0.15f;     // ambient
        }
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
                    mat.skinnedPso->CreateShaderResourceBinding(&mat.skinnedSrb, true);
            }
        }

        // Choose PSO and SRB based on skinning.
        Diligent::IPipelineState*         activePso = nullptr;
        Diligent::IShaderResourceBinding* activeSrb = nullptr;

        if (isSkinned && mat.skinnedPso && mat.skinnedSrb)
        {
            activePso = mat.skinnedPso.RawPtr();
            activeSrb = mat.skinnedSrb.RawPtr();
        }
        else
        {
            activePso = mat.pso.RawPtr();
            activeSrb = mat.srb.RawPtr();
        }

        // Bind albedo texture SRV on the active SRB.
        {
            Diligent::ITextureView* texSRV = nullptr;
            if (mat.albedoTex != TextureHandle::kInvalid)
            {
                auto texIt = m_Impl->textureCache.find(mat.albedoTex);
                if (texIt != m_Impl->textureCache.end())
                    texSRV = texIt->second.srv.RawPtr();
            }
            if (!texSRV)
                texSRV = m_Impl->defaultWhiteTex.srv.RawPtr();

            if (texSRV)
            {
                if (auto* var = activeSrb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Albedo"))
                    var->Set(texSRV);
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
        auto* vb = mesh.vertexBuffer.RawPtr();
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
        if (mesh.indexBuffer && mesh.indexCount > 0)
        {
            auto* ib = mesh.indexBuffer.RawPtr();
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
            LogDbg("Submit: DrawIndexed OK");
        }
        else
        {
            Diligent::DrawAttribs drawAttribs;
            drawAttribs.NumVertices = mesh.vertexCount;
            drawAttribs.Flags       = g_verboseGPU ? Diligent::DRAW_FLAG_VERIFY_ALL
                                                   : Diligent::DRAW_FLAG_NONE;
            ctx->Draw(drawAttribs);
            LogDbg("Submit: Draw OK");
        }

        ++drawIdx;
    }
    LogDbg("Submit: done");
}

void DiligentRenderBackend::EndFrame()
{
    if (!m_Impl->initialized || !m_Impl->swapChain) return;

    LogDbg("EndFrame: Present…");
    m_Impl->swapChain->Present();
    ++m_Impl->frameIndex;

    if (g_verboseGPU)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "EndFrame: frame %llu done",
                      static_cast<unsigned long long>(m_Impl->frameIndex));
        LogDbg(buf);
    }
}

// ─── ImGui rendering ─────────────────────────────────────────────────

bool DiligentRenderBackend::InitImGuiRendering()
{
    if (!m_Impl || !m_Impl->initialized) return false;
    if (m_Impl->imguiReady) return true;

    using namespace Diligent;

    auto& dev = *m_Impl->device;
    auto& sc  = *m_Impl->swapChain;

    // ── Compile ImGui shaders ────────────────────────────────────────
    RefCntAutoPtr<IShader> vsShader, psShader;
    {
        ShaderCreateInfo ci;
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name       = "ImGui VS";
        ci.EntryPoint      = "main";
        ci.Source           = kImGuiVS;
        dev.CreateShader(ci, &vsShader);

        ci.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ci.Desc.Name       = "ImGui PS";
        ci.Source           = kImGuiPS;
        dev.CreateShader(ci, &psShader);

        if (!vsShader || !psShader)
        {
            LogErr("ImGui: failed to compile shaders");
            return false;
        }
    }

    // ── Create PSO ───────────────────────────────────────────────────
    {
        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name         = "ImGuiPSO";
        psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        psoCI.pVS                  = vsShader;
        psoCI.pPS                  = psShader;

        const auto& scDesc = sc.GetDesc();
        psoCI.GraphicsPipeline.NumRenderTargets  = 1;
        psoCI.GraphicsPipeline.RTVFormats[0]     = scDesc.ColorBufferFormat;
        psoCI.GraphicsPipeline.DSVFormat         = scDesc.DepthBufferFormat;
        psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // No depth test/write for UI overlay.
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable     = False;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;

        // No culling.
        psoCI.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
        psoCI.GraphicsPipeline.RasterizerDesc.ScissorEnable  = True;

        // Alpha blending: SrcAlpha * Src + (1 - SrcAlpha) * Dst.
        auto& rt0 = psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
        rt0.BlendEnable   = True;
        rt0.SrcBlend      = BLEND_FACTOR_SRC_ALPHA;
        rt0.DestBlend     = BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOp       = BLEND_OPERATION_ADD;
        rt0.SrcBlendAlpha = BLEND_FACTOR_ONE;
        rt0.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOpAlpha  = BLEND_OPERATION_ADD;
        rt0.RenderTargetWriteMask = COLOR_MASK_ALL;

        // Input layout matching ImDrawVert (20 bytes):
        //   pos(float2, 0) uv(float2, 8) col(uint8x4_norm, 16)
        LayoutElement layoutElems[] =
        {
            { 0, 0, 2, VT_FLOAT32, False },       // ATTRIB0: pos
            { 1, 0, 2, VT_FLOAT32, False },       // ATTRIB1: uv
            { 2, 0, 4, VT_UINT8,   True  },       // ATTRIB2: col (normalized)
        };
        psoCI.GraphicsPipeline.InputLayout.NumElements   = 3;
        psoCI.GraphicsPipeline.InputLayout.LayoutElements = layoutElems;

        // Shader resource layout: one texture + sampler in pixel shader.
        ShaderResourceVariableDesc vars[] =
        {
            { SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        };
        psoCI.PSODesc.ResourceLayout.Variables    = vars;
        psoCI.PSODesc.ResourceLayout.NumVariables = 1;

        // Default SamplerDesc is LINEAR + CLAMP — exactly what ImGui needs.
        SamplerDesc imguiSamplerDesc;
        imguiSamplerDesc.MinFilter = FILTER_TYPE_LINEAR;
        imguiSamplerDesc.MagFilter = FILTER_TYPE_LINEAR;
        imguiSamplerDesc.MipFilter = FILTER_TYPE_LINEAR;
        imguiSamplerDesc.AddressU  = TEXTURE_ADDRESS_CLAMP;
        imguiSamplerDesc.AddressV  = TEXTURE_ADDRESS_CLAMP;
        imguiSamplerDesc.AddressW  = TEXTURE_ADDRESS_CLAMP;

        ImmutableSamplerDesc samplers[] =
        {
            { SHADER_TYPE_PIXEL, "g_Sampler", imguiSamplerDesc },
        };
        psoCI.PSODesc.ResourceLayout.ImmutableSamplers    = samplers;
        psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 1;

        dev.CreateGraphicsPipelineState(psoCI, &m_Impl->imguiPSO);
        if (!m_Impl->imguiPSO)
        {
            LogErr("ImGui: failed to create PSO");
            return false;
        }
    }

    // ── Create projection constant buffer ────────────────────────────
    {
        BufferDesc cbDesc;
        cbDesc.Name           = "ImGuiCB";
        cbDesc.Size           = sizeof(float) * 16;
        cbDesc.Usage          = USAGE_DYNAMIC;
        cbDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        dev.CreateBuffer(cbDesc, nullptr, &m_Impl->imguiCB);
        if (!m_Impl->imguiCB)
        {
            LogErr("ImGui: failed to create CB");
            return false;
        }

        if (auto* var = m_Impl->imguiPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "ImGuiCB"))
            var->Set(m_Impl->imguiCB);
    }

    // ── Create font atlas texture ────────────────────────────────────
    {
        ImGuiIO& io = ImGui::GetIO();
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        TextureDesc texDesc;
        texDesc.Name      = "ImGui Font Atlas";
        texDesc.Type      = RESOURCE_DIM_TEX_2D;
        texDesc.Width     = static_cast<Uint32>(width);
        texDesc.Height    = static_cast<Uint32>(height);
        texDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        texDesc.Usage     = USAGE_IMMUTABLE;
        texDesc.MipLevels = 1;

        TextureSubResData subRes;
        subRes.pData  = pixels;
        subRes.Stride = static_cast<Uint64>(width) * 4;

        TextureData texData;
        texData.pSubResources   = &subRes;
        texData.NumSubresources = 1;

        dev.CreateTexture(texDesc, &texData, &m_Impl->imguiFontTex);
        if (!m_Impl->imguiFontTex)
        {
            LogErr("ImGui: failed to create font atlas texture");
            return false;
        }

        m_Impl->imguiFontSRV = m_Impl->imguiFontTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        // Store font atlas texture ID so ImGui can reference it.
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(m_Impl->imguiFontSRV.RawPtr()));
    }

    // ── Create SRB ───────────────────────────────────────────────────
    m_Impl->imguiPSO->CreateShaderResourceBinding(&m_Impl->imguiSRB, true);
    if (!m_Impl->imguiSRB)
    {
        LogErr("ImGui: failed to create SRB");
        return false;
    }

    m_Impl->imguiReady = true;
    LogInfo("ImGui rendering initialised");
    return true;
}

void DiligentRenderBackend::RenderImGuiDrawData(const void* rawDrawData)
{
    if (!m_Impl || !m_Impl->imguiReady || !rawDrawData) return;

    const auto* drawData = static_cast<const ImDrawData*>(rawDrawData);
    if (drawData->TotalVtxCount <= 0) return;

    using namespace Diligent;

    auto* ctx = m_Impl->context.RawPtr();

    // ── Grow vertex buffer if needed ─────────────────────────────────
    const auto vbNeeded = static_cast<Uint32>(drawData->TotalVtxCount) * static_cast<Uint32>(sizeof(ImDrawVert));
    if (vbNeeded > m_Impl->imguiVBSize)
    {
        m_Impl->imguiVB.Release();
        m_Impl->imguiVBSize = vbNeeded + 4096;

        BufferDesc desc;
        desc.Name           = "ImGui VB";
        desc.Size           = m_Impl->imguiVBSize;
        desc.BindFlags      = BIND_VERTEX_BUFFER;
        desc.Usage          = USAGE_DYNAMIC;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_Impl->device->CreateBuffer(desc, nullptr, &m_Impl->imguiVB);
    }

    // ── Grow index buffer if needed ──────────────────────────────────
    const auto ibNeeded = static_cast<Uint32>(drawData->TotalIdxCount) * static_cast<Uint32>(sizeof(ImDrawIdx));
    if (ibNeeded > m_Impl->imguiIBSize)
    {
        m_Impl->imguiIB.Release();
        m_Impl->imguiIBSize = ibNeeded + 4096;

        BufferDesc desc;
        desc.Name           = "ImGui IB";
        desc.Size           = m_Impl->imguiIBSize;
        desc.BindFlags      = BIND_INDEX_BUFFER;
        desc.Usage          = USAGE_DYNAMIC;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_Impl->device->CreateBuffer(desc, nullptr, &m_Impl->imguiIB);
    }

    // ── Upload vertex/index data ─────────────────────────────────────
    {
        MapHelper<ImDrawVert> vtx(ctx, m_Impl->imguiVB, MAP_WRITE, MAP_FLAG_DISCARD);
        MapHelper<ImDrawIdx>  idx(ctx, m_Impl->imguiIB, MAP_WRITE, MAP_FLAG_DISCARD);

        ImDrawVert* vtxDst = vtx;
        ImDrawIdx*  idxDst = idx;

        for (int n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            std::memcpy(vtxDst, cmdList->VtxBuffer.Data,
                        static_cast<size_t>(cmdList->VtxBuffer.Size) * sizeof(ImDrawVert));
            std::memcpy(idxDst, cmdList->IdxBuffer.Data,
                        static_cast<size_t>(cmdList->IdxBuffer.Size) * sizeof(ImDrawIdx));
            vtxDst += cmdList->VtxBuffer.Size;
            idxDst += cmdList->IdxBuffer.Size;
        }
    }

    // ── Update projection matrix ─────────────────────────────────────
    {
        const float L = drawData->DisplayPos.x;
        const float R = L + drawData->DisplaySize.x;
        const float T = drawData->DisplayPos.y;
        const float B = T + drawData->DisplaySize.y;

        // Column-major orthographic projection (D3D depth 0..1).
        // mul(ProjectionMatrix, float4(pos, 0, 1)) in the shader.
        const float mvp[16] =
        {
            2.0f / (R - L),       0.0f,                 0.0f, 0.0f,
            0.0f,                 2.0f / (T - B),       0.0f, 0.0f,
            0.0f,                 0.0f,                 0.5f, 0.0f,
            (R + L) / (L - R),   (T + B) / (B - T),   0.5f, 1.0f,
        };

        MapHelper<float> cb(ctx, m_Impl->imguiCB, MAP_WRITE, MAP_FLAG_DISCARD);
        std::memcpy(cb, mvp, sizeof(mvp));
    }

    // ── Set pipeline state ───────────────────────────────────────────
    ctx->SetPipelineState(m_Impl->imguiPSO);

    // ── Bind VB / IB ─────────────────────────────────────────────────
    {
        IBuffer* vbs[] = { m_Impl->imguiVB };
        Uint64 offsets[] = { 0 };
        ctx->SetVertexBuffers(0, 1, vbs, offsets,
                              RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                              SET_VERTEX_BUFFERS_FLAG_RESET);
        ctx->SetIndexBuffer(m_Impl->imguiIB, 0,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    // ── Draw each command list ───────────────────────────────────────
    int globalIdxOffset = 0;
    int globalVtxOffset = 0;

    const ImVec2 clipOff = drawData->DisplayPos;

    for (int n = 0; n < drawData->CmdListsCount; ++n)
    {
        const ImDrawList* cmdList = drawData->CmdLists[n];

        for (int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; ++cmdIdx)
        {
            const ImDrawCmd& cmd = cmdList->CmdBuffer[cmdIdx];

            if (cmd.UserCallback)
            {
                cmd.UserCallback(cmdList, &cmd);
                continue;
            }

            // Scissor rect.
            const float clipX = cmd.ClipRect.x - clipOff.x;
            const float clipY = cmd.ClipRect.y - clipOff.y;
            const float clipW = cmd.ClipRect.z - clipOff.x;
            const float clipH = cmd.ClipRect.w - clipOff.y;

            Rect scissor;
            scissor.left   = static_cast<Int32>(clipX);
            scissor.top    = static_cast<Int32>(clipY);
            scissor.right  = static_cast<Int32>(clipW);
            scissor.bottom = static_cast<Int32>(clipH);
            ctx->SetScissorRects(1, &scissor, 0, 0);

            // Bind texture.
            auto* texSRV = reinterpret_cast<ITextureView*>(cmd.GetTexID());
            if (texSRV)
            {
                if (auto* var = m_Impl->imguiSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture"))
                    var->Set(texSRV);
            }

            ctx->CommitShaderResources(m_Impl->imguiSRB,
                                       RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            DrawIndexedAttribs drawAttribs;
            drawAttribs.NumIndices  = cmd.ElemCount;
            drawAttribs.IndexType   = sizeof(ImDrawIdx) == 2 ? VT_UINT16 : VT_UINT32;
            drawAttribs.FirstIndexLocation = static_cast<Uint32>(cmd.IdxOffset + globalIdxOffset);
            drawAttribs.BaseVertex         = static_cast<Int32>(cmd.VtxOffset + globalVtxOffset);
            drawAttribs.Flags              = g_verboseGPU ? DRAW_FLAG_VERIFY_ALL : DRAW_FLAG_NONE;
            ctx->DrawIndexed(drawAttribs);
        }

        globalIdxOffset += cmdList->IdxBuffer.Size;
        globalVtxOffset += cmdList->VtxBuffer.Size;
    }
}

void DiligentRenderBackend::ShutdownImGuiRendering()
{
    if (!m_Impl || !m_Impl->imguiReady) return;

    m_Impl->imguiSRB.Release();
    m_Impl->imguiPSO.Release();
    m_Impl->imguiCB.Release();
    m_Impl->imguiVB.Release();
    m_Impl->imguiIB.Release();
    m_Impl->imguiFontSRV.Release();
    m_Impl->imguiFontTex.Release();
    m_Impl->imguiVBSize = 0;
    m_Impl->imguiIBSize = 0;
    m_Impl->imguiReady  = false;

    LogInfo("ImGui rendering shut down");
}

// ─── Diagnostics ─────────────────────────────────────────────────────

DiligentBackendAPI DiligentRenderBackend::SelectedAPI() const noexcept
{
    return m_Impl ? m_Impl->selectedAPI : DiligentBackendAPI::kAuto;
}

std::uint64_t DiligentRenderBackend::FrameIndex() const noexcept
{
    return m_Impl ? m_Impl->frameIndex : 0ull;
}

bool DiligentRenderBackend::IsInitialized() const noexcept
{
    return m_Impl && m_Impl->initialized;
}

} // namespace SagaEngine::Render::Backend
