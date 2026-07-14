/// @file DiligentRenderBackend.cpp
/// @brief Diligent render backend lifecycle and factory entry points.

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackendPrivate.h"
#include "SagaEngine/Platform/IWindow.h"

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

#include "DiligentShaders.inl"

} // anonymous namespace

DiligentRenderBackend::DiligentRenderBackend()
    : m_Impl(std::make_unique<Impl>())
{
    m_Impl->runtime =
        std::make_unique<DiligentRuntime::DiligentGraphicsRuntime>();
}

DiligentRenderBackend::DiligentRenderBackend(RenderBackendConfig cfg)
    : m_Impl(std::make_unique<Impl>())
{
    m_Impl->config = cfg;
    m_Impl->runtime =
        std::make_unique<DiligentRuntime::DiligentGraphicsRuntime>();
}

DiligentRenderBackend::~DiligentRenderBackend()
{
    try { Shutdown(); } catch (...) {}
}

// ─── Lifecycle ───────────────────────────────────────────────────────

bool DiligentRenderBackend::Initialize(const SwapchainDesc& desc)
{
    if (m_Impl->rendererInitialized) return true;
    if (m_Impl->runtime->IsInitialized())
    {
        Shutdown();
    }

    m_Impl->gpuValidation = m_Impl->config.enableValidation;

    if (!m_Impl->runtime->Initialize(m_Impl->config, desc))
    {
        return false;
    }
    auto rollback = [this]() {
        Shutdown();
        return false;
    };

    std::string msg = "Initialized with ";
    msg += ToString(m_Impl->runtime->Status().selectedAPI);
    msg += " (";
    msg += std::to_string(desc.width);
    msg += "x";
    msg += std::to_string(desc.height);
    msg += ")";
    LOG_CAT_INFO(Render, msg.c_str());

    // ── Create CameraCB ──────────────────────────────────────────────
    {
        using namespace Diligent;
        BufferDesc cbDesc;
        cbDesc.Name           = "CameraCB";
        cbDesc.Size           = sizeof(float) * 80;
        cbDesc.Usage          = USAGE_DYNAMIC;
        cbDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_Impl->runtime->Device()->CreateBuffer(
            cbDesc, nullptr, &m_Impl->cameraCB);
        if (!m_Impl->cameraCB)
        {
            LOG_CAT_ERROR(Render, "Failed to create CameraCB");
            return rollback();
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
        m_Impl->runtime->Device()->CreateShader(ci, &m_Impl->solidVS);

        ci.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ci.Desc.Name       = "Solid PS";
        ci.Source           = kSolidPS;
        m_Impl->runtime->Device()->CreateShader(ci, &m_Impl->solidPS);

        if (!m_Impl->solidVS || !m_Impl->solidPS)
        {
            LOG_CAT_ERROR(Render, "Failed to compile solid-color shaders");
            return rollback();
        }
        LOG_CAT_DEBUG(Render, "Solid shaders compiled OK");

        // Skinned vertex shader (shares the same PS).
        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name       = "Skinned VS";
        ci.EntryPoint      = "main";
        ci.Source           = kSkinnedVS;
        m_Impl->runtime->Device()->CreateShader(ci, &m_Impl->skinnedVS);

        if (!m_Impl->skinnedVS)
        {
            LOG_CAT_ERROR(Render, "Failed to compile skinned vertex shader");
            return rollback();
        }
        LOG_CAT_DEBUG(Render, "Skinned VS compiled OK");

        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name       = "Shadow Depth VS";
        ci.EntryPoint      = "main";
        ci.Source          = kShadowDepthVS;
        m_Impl->runtime->Device()->CreateShader(ci, &m_Impl->shadow.depthVS);
        if (!m_Impl->shadow.depthVS)
        {
            LOG_CAT_ERROR(Render, "Failed to compile shadow depth vertex shader");
            return rollback();
        }
        LOG_CAT_DEBUG(Render, "Shadow depth VS compiled OK");
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
        m_Impl->runtime->Device()->CreateBuffer(
            cbDesc, nullptr, &m_Impl->boneCB);
        if (!m_Impl->boneCB)
        {
            LOG_CAT_ERROR(Render, "Failed to create BoneCB");
            return rollback();
        }
        LOG_CAT_DEBUG(Render, "BoneCB created (8192 bytes)");
    }

    LOG_CAT_INFO(Render, "Diligent render backend ready");
    m_Impl->rendererInitialized = true;
    return true;
}

void DiligentRenderBackend::Shutdown()
{
    if (!m_Impl || !m_Impl->runtime)
    {
        return;
    }
    if (!m_Impl->runtime->IsInitialized() && !m_Impl->rendererInitialized)
    {
        return;
    }

    ShutdownOverlayRendering();

    for (auto& [id, material] : m_Impl->materialCache)
    {
        (void)id;
        m_Impl->runtime->DestroyMaterialBinding(material.binding);
        m_Impl->runtime->DestroyMaterialBinding(material.skinnedBinding);
    }
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
    m_Impl->boneCB.Release();
    m_Impl->cameraCB.Release();
    m_Impl->solidVS.Release();
    m_Impl->skinnedVS.Release();
    m_Impl->solidPS.Release();

    m_Impl->runtime->Shutdown();
    m_Impl->rendererInitialized = false;

    LOG_CAT_INFO(Render, "Shutdown complete");
}

void DiligentRenderBackend::OnResize(std::uint32_t width, std::uint32_t height)
{
    if (!m_Impl->runtime->IsInitialized() || !m_Impl->runtime->SwapChain())
        return;
    if (width == 0 || height == 0) return;
    m_Impl->frameCapture.Reset();
    const auto oldFrameSlotCount = m_Impl->runtime->FrameSlotCount();
    m_Impl->runtime->Resize(width, height);

    const auto newFrameSlotCount = m_Impl->runtime->FrameSlotCount();
    if (newFrameSlotCount == oldFrameSlotCount)
    {
        return;
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
    return m_Impl && m_Impl->runtime
        ? m_Impl->runtime->Status().selectedAPI
        : GraphicsBackendAPI::kAuto;
}

std::uint64_t DiligentRenderBackend::FrameIndex() const noexcept
{
    return m_Impl && m_Impl->runtime ? m_Impl->runtime->Status().frameIndex
                                     : 0ull;
}

bool DiligentRenderBackend::IsInitialized() const noexcept
{
    return m_Impl && m_Impl->runtime && m_Impl->runtime->IsInitialized();
}

::SagaEngine::Graphics::Backends::Diligent::Runtime::DiligentGraphicsRuntime&
DiligentRenderBackend::RuntimeForIntegrationTesting() noexcept
{
    return *m_Impl->runtime;
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
