/// @file DiligentRenderBackend.cpp
/// @brief Diligent render backend lifecycle and factory entry points.

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackendPrivate.h"
#include "SagaEngine/Platform/IWindow.h"

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

namespace
{

#include "DiligentFactoryInit.inl"
#include "DiligentShaders.inl"

} // anonymous namespace

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


} // namespace SagaEngine::Render::Backend
