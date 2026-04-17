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

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>

// ─── Diligent includes ────────────────────────────────────────────────

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

struct MaterialGPU
{
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
    MaterialRenderQueue renderQueue = MaterialRenderQueue::Opaque;
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

    // ── Compiled shaders (one VS/PS pair for all solid-color draws) ──
    Diligent::RefCntAutoPtr<Diligent::IShader> solidVS;
    Diligent::RefCntAutoPtr<Diligent::IShader> solidPS;

    // ── Caches ──────────────────────────────────────────────────────
    std::unordered_map<World::MeshId, MeshGPU, MeshIdHash>             meshCache;
    std::unordered_map<World::MaterialId, MaterialGPU, MaterialIdHash> materialCache;
    std::unordered_map<PSOCacheKey, Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PSOCacheKeyHash> psoCache;

    // ── ID generators ───────────────────────────────────────────────
    std::uint32_t nextMeshId     = 1;
    std::uint32_t nextMaterialId = 1;

    std::uint64_t frameIndex   = 0;
    bool          initialized  = false;
};

// ─── Small logging helpers ───────────────────────────────────────────

namespace
{

void LogInfo(const char* msg)
{
    std::fprintf(stdout, "[DiligentBackend] %s\n", msg);
}

void LogErr(const char* msg)
{
    std::fprintf(stderr, "[DiligentBackend][error] %s\n", msg);
}

Diligent::SwapChainDesc MakeSwapChainDesc(const SwapchainDesc& desc)
{
    Diligent::SwapChainDesc out{};
    out.Width             = desc.width;
    out.Height            = desc.height;
    out.ColorBufferFormat = desc.hdr ? Diligent::TEX_FORMAT_RGBA16_FLOAT
                                     : Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    out.DepthBufferFormat = Diligent::TEX_FORMAT_D32_FLOAT;
    out.BufferCount       = 2;
    return out;
}

#if PLATFORM_WIN32 || defined(_WIN32)
Diligent::Win32NativeWindow AsWin32NativeWindow(void* nativeWindow)
{
    Diligent::Win32NativeWindow w{};
    w.hWnd = nativeWindow;
    return w;
}
#endif

// ─── API-specific init helpers (unchanged from Phase 1) ──────────────

#if D3D12_SUPPORTED
bool InitD3D12(const SwapchainDesc& desc, const DiligentBackendConfig& cfg,
               Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
               Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
               Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap)
{
#   if ENGINE_DLL
    auto getFactoryFn = Diligent::LoadGraphicsEngineD3D12();
    if (!getFactoryFn) return false;
    auto* factory = getFactoryFn();
#   else
    auto* factory = Diligent::GetEngineFactoryD3D12();
#   endif
    if (!factory) return false;
    Diligent::EngineD3D12CreateInfo ci;
    ci.EnableValidation = cfg.enableValidation;
    factory->CreateDeviceAndContextsD3D12(ci, &outDevice, &outContext);
    if (!outDevice || !outContext) return false;
    const auto scDesc = MakeSwapChainDesc(desc);
#   if PLATFORM_WIN32 || defined(_WIN32)
    factory->CreateSwapChainD3D12(outDevice, outContext, scDesc,
                                  Diligent::FullScreenModeDesc{},
                                  AsWin32NativeWindow(desc.nativeWindow), &outSwap);
#   endif
    return outSwap != nullptr;
}
#endif

#if VULKAN_SUPPORTED
bool InitVulkan(const SwapchainDesc& desc, const DiligentBackendConfig& cfg,
                Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
                Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
                Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap)
{
#   if ENGINE_DLL
    auto getFactoryFn = Diligent::LoadGraphicsEngineVk();
    if (!getFactoryFn) return false;
    auto* factory = getFactoryFn();
#   else
    auto* factory = Diligent::GetEngineFactoryVk();
#   endif
    if (!factory) return false;
    Diligent::EngineVkCreateInfo ci;
    ci.EnableValidation = cfg.enableValidation;
    factory->CreateDeviceAndContextsVk(ci, &outDevice, &outContext);
    if (!outDevice || !outContext) return false;
    const auto scDesc = MakeSwapChainDesc(desc);
#   if PLATFORM_WIN32 || defined(_WIN32)
    factory->CreateSwapChainVk(outDevice, outContext, scDesc,
                               AsWin32NativeWindow(desc.nativeWindow), &outSwap);
#   else
    if (!desc.nativeWindow) return false;
    factory->CreateSwapChainVk(outDevice, outContext, scDesc,
                               *reinterpret_cast<Diligent::NativeWindow*>(desc.nativeWindow),
                               &outSwap);
#   endif
    return outSwap != nullptr;
}
#endif

#if D3D11_SUPPORTED
bool InitD3D11(const SwapchainDesc& desc, const DiligentBackendConfig& cfg,
               Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
               Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
               Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap)
{
#   if ENGINE_DLL
    auto getFactoryFn = Diligent::LoadGraphicsEngineD3D11();
    if (!getFactoryFn) return false;
    auto* factory = getFactoryFn();
#   else
    auto* factory = Diligent::GetEngineFactoryD3D11();
#   endif
    if (!factory) return false;
    Diligent::EngineD3D11CreateInfo ci;
    ci.EnableValidation = cfg.enableValidation;
    factory->CreateDeviceAndContextsD3D11(ci, &outDevice, &outContext);
    if (!outDevice || !outContext) return false;
    const auto scDesc = MakeSwapChainDesc(desc);
#   if PLATFORM_WIN32 || defined(_WIN32)
    factory->CreateSwapChainD3D11(outDevice, outContext, scDesc,
                                  Diligent::FullScreenModeDesc{},
                                  AsWin32NativeWindow(desc.nativeWindow), &outSwap);
#   endif
    return outSwap != nullptr;
}
#endif

#if GL_SUPPORTED
bool InitOpenGL(const SwapchainDesc& desc, const DiligentBackendConfig& /*cfg*/,
                Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
                Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
                Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap)
{
#   if ENGINE_DLL
    auto getFactoryFn = Diligent::LoadGraphicsEngineOpenGL();
    if (!getFactoryFn) return false;
    auto* factory = getFactoryFn();
#   else
    auto* factory = Diligent::GetEngineFactoryOpenGL();
#   endif
    if (!factory) return false;
    const auto scDesc = MakeSwapChainDesc(desc);
    Diligent::EngineGLCreateInfo ci;
#   if PLATFORM_WIN32 || defined(_WIN32)
    ci.Window.hWnd = desc.nativeWindow;
#   endif
    factory->CreateDeviceAndSwapChainGL(ci, &outDevice, &outContext, scDesc, &outSwap);
    return outDevice && outContext && outSwap;
}
#endif

DiligentBackendAPI TryInitAPI(
    DiligentBackendAPI preferred, const SwapchainDesc& desc,
    const DiligentBackendConfig& cfg,
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& outDevice,
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& outContext,
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>& outSwap)
{
    auto tryOne = [&](DiligentBackendAPI api) -> bool
    {
        outDevice.Release(); outContext.Release(); outSwap.Release();
        switch (api)
        {
#if D3D12_SUPPORTED
            case DiligentBackendAPI::kD3D12:  return InitD3D12(desc, cfg, outDevice, outContext, outSwap);
#endif
#if VULKAN_SUPPORTED
            case DiligentBackendAPI::kVulkan: return InitVulkan(desc, cfg, outDevice, outContext, outSwap);
#endif
#if D3D11_SUPPORTED
            case DiligentBackendAPI::kD3D11:  return InitD3D11(desc, cfg, outDevice, outContext, outSwap);
#endif
#if GL_SUPPORTED
            case DiligentBackendAPI::kOpenGL: return InitOpenGL(desc, cfg, outDevice, outContext, outSwap);
#endif
            default: return false;
        }
    };

    if (preferred != DiligentBackendAPI::kAuto)
        return tryOne(preferred) ? preferred : DiligentBackendAPI::kAuto;

    constexpr DiligentBackendAPI kOrder[] = {
        DiligentBackendAPI::kD3D12, DiligentBackendAPI::kVulkan,
        DiligentBackendAPI::kD3D11, DiligentBackendAPI::kOpenGL,
    };
    for (auto api : kOrder)
        if (tryOne(api)) return api;

    return DiligentBackendAPI::kAuto;
}

// ─── Solid-color shaders (shared by all Phase 3 materials) ───────────
//
// MeshVertex layout: pos(3) normal(3) tangent(3) handedness(1) uv0(2) uv1(2)
// = 14 floats = 56 bytes.  The VS reads pos + normal; PS outputs a flat
// colour derived from the normal (simple directional lighting).

static constexpr char kSolidVS[] = R"(
cbuffer CameraCB
{
    float4x4 g_ViewProj;
    float4x4 g_Model;
};

struct VSInput
{
    float3 Pos        : ATTRIB0;
    float3 Normal     : ATTRIB1;
    float3 Tangent    : ATTRIB2;
    float  Handedness : ATTRIB3;
    float2 UV0        : ATTRIB4;
    float2 UV1        : ATTRIB5;
};

struct PSInput
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    float4 worldPos = mul(float4(VSIn.Pos, 1.0), g_Model);
    PSIn.Pos    = mul(worldPos, g_ViewProj);
    PSIn.Normal = mul(float4(VSIn.Normal, 0.0), g_Model).xyz;
}
)";

static constexpr char kSolidPS[] = R"(
struct PSInput
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
};

float4 main(in PSInput PSIn) : SV_Target
{
    float3 lightDir = normalize(float3(0.3, 1.0, 0.5));
    float  ndl      = saturate(dot(normalize(PSIn.Normal), lightDir));
    float3 color    = float3(0.8, 0.8, 0.8) * (0.15 + 0.85 * ndl);
    return float4(color, 1.0);
}
)";

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
        cbDesc.Size           = sizeof(float) * 32;  // g_ViewProj + g_Model
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
    }

    LogInfo("Phase 3: backend ready");
    return true;
}

void DiligentRenderBackend::Shutdown()
{
    if (!m_Impl || !m_Impl->initialized) return;

    m_Impl->meshCache.clear();
    m_Impl->materialCache.clear();
    m_Impl->psoCache.clear();

    m_Impl->cameraCB.Release();
    m_Impl->solidVS.Release();
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

    // Cull
    switch (static_cast<MaterialCullMode>(key.cullMode))
    {
        case MaterialCullMode::Back:  psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;  break;
        case MaterialCullMode::Front: psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_FRONT; break;
        case MaterialCullMode::None:  psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;  break;
    }

    // Input layout matching MeshVertex (56 bytes):
    //   pos(3f) normal(3f) tangent(3f) handedness(1f) uv0(2f) uv1(2f)
    LayoutElement layoutElems[] =
    {
        {0, 0, 3, VT_FLOAT32, False},  // Pos
        {1, 0, 3, VT_FLOAT32, False},  // Normal
        {2, 0, 3, VT_FLOAT32, False},  // Tangent
        {3, 0, 1, VT_FLOAT32, False},  // Handedness
        {4, 0, 2, VT_FLOAT32, False},  // UV0
        {5, 0, 2, VT_FLOAT32, False},  // UV1
    };
    psoCI.GraphicsPipeline.InputLayout.NumElements    = 6;
    psoCI.GraphicsPipeline.InputLayout.LayoutElements  = layoutElems;

    RefCntAutoPtr<IPipelineState> pso;
    device.CreateGraphicsPipelineState(psoCI, &pso);
    if (!pso)
    {
        LogErr("Failed to create PSO");
        return {};
    }

    // Bind CameraCB as a static variable on the VS.
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "CameraCB"))
        var->Set(cameraCB);

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

// ─── Frame lifecycle ─────────────────────────────────────────────────

void DiligentRenderBackend::BeginFrame()
{
    if (!m_Impl->initialized || !m_Impl->context || !m_Impl->swapChain)
        return;

    auto* ctx = m_Impl->context.RawPtr();
    auto* sc  = m_Impl->swapChain.RawPtr();

    auto* rtv = sc->GetCurrentBackBufferRTV();
    auto* dsv = sc->GetDepthBufferDSV();

    ctx->SetRenderTargets(
        rtv ? 1u : 0u, rtv ? &rtv : nullptr, dsv,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (rtv)
        ctx->ClearRenderTarget(rtv, m_Impl->config.clearColor,
                               Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (dsv && !m_Impl->config.skipDepthClear)
        ctx->ClearDepthStencil(dsv,
                               Diligent::CLEAR_DEPTH_FLAG | Diligent::CLEAR_STENCIL_FLAG,
                               m_Impl->config.clearDepth, m_Impl->config.clearStencil,
                               Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void DiligentRenderBackend::Submit(const Scene::Camera&     camera,
                                   const Scene::RenderView& view)
{
    if (!m_Impl->initialized || !m_Impl->context || !m_Impl->cameraCB)
        return;

    auto* ctx = m_Impl->context.RawPtr();

    // ── Nothing to draw? ─────────────────────────────────────────────
    if (view.drawItems.empty()) return;

    const float* vpData = camera.viewProj.Data();

    // ── Draw loop ────────────────────────────────────────────────────
    Diligent::IPipelineState* lastPSO = nullptr;
    Diligent::IBuffer*        lastVB  = nullptr;
    Diligent::IBuffer*        lastIB  = nullptr;

    for (const auto& item : view.drawItems)
    {
        auto meshIt = m_Impl->meshCache.find(item.mesh);
        if (meshIt == m_Impl->meshCache.end()) continue;

        auto matIt = m_Impl->materialCache.find(item.material);
        if (matIt == m_Impl->materialCache.end()) continue;

        auto& mesh = meshIt->second;
        auto& mat  = matIt->second;

        // Update CameraCB per DrawItem (g_ViewProj + g_Model).
        {
            Diligent::MapHelper<float> cbData(ctx, m_Impl->cameraCB,
                                              Diligent::MAP_WRITE,
                                              Diligent::MAP_FLAG_DISCARD);
            for (int i = 0; i < 16; ++i) cbData[i]      = vpData[i];
            const float* mData = item.model.Data();
            for (int i = 0; i < 16; ++i) cbData[16 + i] = mData[i];
        }

        // PSO switch
        auto* pso = mat.pso.RawPtr();
        if (pso != lastPSO)
        {
            ctx->SetPipelineState(pso);
            lastPSO = pso;
        }

        // SRB
        ctx->CommitShaderResources(mat.srb,
                                   Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // VB
        auto* vb = mesh.vertexBuffer.RawPtr();
        if (vb != lastVB)
        {
            Diligent::Uint64 offsets[] = {0};
            ctx->SetVertexBuffers(0, 1, &vb, offsets,
                                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                  Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            lastVB = vb;
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
            }

            Diligent::DrawIndexedAttribs drawAttribs;
            drawAttribs.NumIndices = mesh.indexCount;
            drawAttribs.IndexType  = Diligent::VT_UINT32;
            drawAttribs.Flags      = Diligent::DRAW_FLAG_VERIFY_ALL;
            ctx->DrawIndexed(drawAttribs);
        }
        else
        {
            Diligent::DrawAttribs drawAttribs;
            drawAttribs.NumVertices = mesh.vertexCount;
            drawAttribs.Flags       = Diligent::DRAW_FLAG_VERIFY_ALL;
            ctx->Draw(drawAttribs);
        }
    }
}

void DiligentRenderBackend::EndFrame()
{
    if (!m_Impl->initialized || !m_Impl->swapChain) return;
    m_Impl->swapChain->Present();
    ++m_Impl->frameIndex;
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
