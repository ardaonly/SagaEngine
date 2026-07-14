/// @file DiligentOverlayRenderer.cpp
/// @brief Diligent implementation of generic overlay draw-list rendering.

#include "SagaEngine/Render/Backend/Diligent/DiligentOverlayRenderer.h"

#include "SagaEngine/Core/Log/LogCategories.h"
#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"
#include "SagaEngine/Graphics/Descs/ResourceDescs.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentDeviceServices.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

#include "DiligentOverlayShaders.inl"

#include "Buffer.h"
#include "DeviceContext.h"
#include "MapHelper.hpp"
#include "PipelineState.h"
#include "RenderDevice.h"
#include "Shader.h"
#include "ShaderResourceBinding.h"
#include "SwapChain.h"
#include "Texture.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <type_traits>

namespace SagaEngine::Render::Backend
{

namespace
{

static_assert(std::is_standard_layout_v<RenderOverlayVertex>);
static_assert(sizeof(RenderOverlayVertex) == 20u);
static_assert(offsetof(RenderOverlayVertex, pos) == 0u);
static_assert(offsetof(RenderOverlayVertex, uv) == 8u);
static_assert(offsetof(RenderOverlayVertex, colorRgba8) == 16u);

constexpr std::size_t kMaxOverlayVertices = 1'000'000u;
constexpr std::size_t kMaxOverlayIndices = 3'000'000u;
constexpr std::uint32_t kDefaultOverlayFrameSlots = 3u;

[[nodiscard]] std::uint32_t ByteSizeOrZero(
    std::size_t count,
    std::size_t stride) noexcept
{
    if (count == 0 || stride == 0)
    {
        return 0;
    }

    constexpr auto kMax = static_cast<std::size_t>(
        std::numeric_limits<std::uint32_t>::max());
    if (count > kMax / stride)
    {
        return 0;
    }

    return static_cast<std::uint32_t>(count * stride);
}

[[nodiscard]] bool AddWillOverflow(
    std::uint32_t lhs,
    std::uint32_t rhs) noexcept
{
    return lhs > std::numeric_limits<std::uint32_t>::max() - rhs;
}

} // namespace

bool ValidateOverlayFrameForTests(const RenderOverlayFrame& frame) noexcept
{
    if (!std::isfinite(frame.displayPos[0]) ||
        !std::isfinite(frame.displayPos[1]) ||
        !std::isfinite(frame.displaySize[0]) ||
        !std::isfinite(frame.displaySize[1]) ||
        !std::isfinite(frame.framebufferScale[0]) ||
        !std::isfinite(frame.framebufferScale[1]) ||
        frame.displaySize[0] <= 0.0f || frame.displaySize[1] <= 0.0f ||
        frame.framebufferScale[0] <= 0.0f || frame.framebufferScale[1] <= 0.0f)
    {
        return false;
    }

    constexpr float kMaxFramebufferDimension =
        static_cast<float>(std::numeric_limits<std::int32_t>::max());
    const float framebufferWidth = frame.displaySize[0] * frame.framebufferScale[0];
    const float framebufferHeight = frame.displaySize[1] * frame.framebufferScale[1];
    if (!std::isfinite(framebufferWidth) ||
        !std::isfinite(framebufferHeight) ||
        framebufferWidth > kMaxFramebufferDimension ||
        framebufferHeight > kMaxFramebufferDimension)
    {
        LOG_CAT_ERROR(Render, "overlay framebuffer dimensions are invalid");
        return false;
    }

    std::size_t totalVertexCount = 0;
    std::size_t totalIndexCount = 0;
    std::size_t submittedIndexCount = 0;
    for (const auto& list : frame.drawLists)
    {
        if (list.vertices.size() > kMaxOverlayVertices ||
            list.indices.size() > kMaxOverlayIndices ||
            totalVertexCount > kMaxOverlayVertices - list.vertices.size() ||
            totalIndexCount > kMaxOverlayIndices - list.indices.size())
        {
            LOG_CAT_ERROR(Render, "overlay frame exceeds vertex/index budget");
            return false;
        }

        totalVertexCount += list.vertices.size();
        totalIndexCount += list.indices.size();

        if (list.vertices.size() >
                static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
            list.indices.size() >
                static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            LOG_CAT_ERROR(Render, "overlay draw list exceeds 32-bit offsets");
            return false;
        }

        const auto vertexCount =
            static_cast<std::uint32_t>(list.vertices.size());
        const auto indexCount =
            static_cast<std::uint32_t>(list.indices.size());
        for (const auto& cmd : list.commands)
        {
            if (cmd.elementCount == 0u)
            {
                continue;
            }
            if (cmd.indexOffset > indexCount ||
                cmd.elementCount > indexCount - cmd.indexOffset)
            {
                LOG_CAT_ERROR(Render, "overlay command index range is out of bounds");
                return false;
            }
            if (cmd.vertexOffset > vertexCount)
            {
                LOG_CAT_ERROR(Render, "overlay command vertex offset is out of bounds");
                return false;
            }
            if (submittedIndexCount >
                kMaxOverlayIndices - static_cast<std::size_t>(cmd.elementCount))
            {
                LOG_CAT_ERROR(Render, "overlay frame exceeds submitted index budget");
                return false;
            }
            submittedIndexCount += static_cast<std::size_t>(cmd.elementCount);

            std::uint32_t maxIndex = 0u;
            for (std::uint32_t i = 0u; i < cmd.elementCount; ++i)
            {
                maxIndex = std::max(
                    maxIndex,
                    list.indices[static_cast<std::size_t>(cmd.indexOffset + i)]);
            }
            if (maxIndex >= vertexCount - cmd.vertexOffset)
            {
                LOG_CAT_ERROR(Render, "overlay command references an out-of-range vertex");
                return false;
            }
        }
    }

    if (totalVertexCount == 0u || totalIndexCount == 0u)
    {
        return true;
    }

    return ByteSizeOrZero(totalVertexCount, sizeof(RenderOverlayVertex)) != 0u &&
           ByteSizeOrZero(totalIndexCount, sizeof(std::uint32_t)) != 0u;
}

std::uint32_t DiligentOverlayTextureRegistry::AdvanceGeneration(
    std::uint32_t generation) noexcept
{
    ++generation;
    if (generation == 0u)
    {
        generation = 1u;
    }
    return generation;
}

RenderOverlayTextureHandle DiligentOverlayTextureRegistry::Create(
    Graphics::TextureHandle nativeTexture)
{
    if (!nativeTexture.IsValid())
    {
        return {};
    }

    std::uint32_t publicIndex = 0u;
    if (!m_freeSlots.empty())
    {
        publicIndex = m_freeSlots.back();
        m_freeSlots.pop_back();
    }
    else
    {
        if (m_slots.size() >=
            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            return {};
        }
        publicIndex = static_cast<std::uint32_t>(m_slots.size() + 1u);
        m_slots.push_back({});
    }

    auto& slot = m_slots[static_cast<std::size_t>(publicIndex - 1u)];
    if (slot.generation == 0u)
    {
        slot.generation = 1u;
    }
    slot.state = SlotState::Ready;
    slot.nativeTexture = nativeTexture;
    return {publicIndex, slot.generation};
}

DiligentOverlayTextureRegistry::DestroyResult
DiligentOverlayTextureRegistry::Destroy(
    RenderOverlayTextureHandle texture) noexcept
{
    if (!texture)
    {
        return {};
    }

    const auto slotIndex = texture.index - 1u;
    if (slotIndex >= m_slots.size())
    {
        return {};
    }

    auto& slot = m_slots[slotIndex];
    if (slot.state != SlotState::Ready ||
        slot.generation != texture.generation)
    {
        return {};
    }

    DestroyResult result{};
    result.destroyed = slot.nativeTexture.IsValid();
    result.nativeTexture = slot.nativeTexture;

    slot.state = SlotState::Free;
    slot.nativeTexture = {};
    slot.generation = AdvanceGeneration(slot.generation);
    m_freeSlots.push_back(texture.index);
    return result;
}

Graphics::TextureHandle DiligentOverlayTextureRegistry::Resolve(
    RenderOverlayTextureHandle texture) const noexcept
{
    if (!texture)
    {
        return {};
    }

    const auto slotIndex = texture.index - 1u;
    if (slotIndex >= m_slots.size())
    {
        return {};
    }

    const auto& slot = m_slots[slotIndex];
    if (slot.state != SlotState::Ready ||
        slot.generation != texture.generation)
    {
        return {};
    }

    return slot.nativeTexture;
}

std::vector<Graphics::TextureHandle> DiligentOverlayTextureRegistry::DestroyAll()
{
    std::vector<Graphics::TextureHandle> nativeTextures;
    nativeTextures.reserve(m_slots.size());
    m_freeSlots.clear();

    for (std::uint32_t i = 0u; i < m_slots.size(); ++i)
    {
        auto& slot = m_slots[i];
        if (slot.state == SlotState::Ready && slot.nativeTexture.IsValid())
        {
            nativeTextures.push_back(slot.nativeTexture);
        }

        slot.state = SlotState::Free;
        slot.nativeTexture = {};
        slot.generation = AdvanceGeneration(slot.generation);
        m_freeSlots.push_back(i + 1u);
    }

    return nativeTextures;
}

std::uint32_t DiligentOverlayTextureRegistry::GenerationForTests(
    std::uint32_t publicIndex) const noexcept
{
    if (publicIndex == 0u ||
        publicIndex > static_cast<std::uint32_t>(m_slots.size()))
    {
        return 0u;
    }

    return m_slots[static_cast<std::size_t>(publicIndex - 1u)].generation;
}

void DiligentOverlayTextureRegistry::SetGenerationForTests(
    std::uint32_t publicIndex,
    std::uint32_t generation)
{
    if (publicIndex == 0u ||
        publicIndex > static_cast<std::uint32_t>(m_slots.size()))
    {
        return;
    }

    m_slots[static_cast<std::size_t>(publicIndex - 1u)].generation =
        generation == 0u ? 1u : generation;
}

bool DiligentOverlayRenderer::IsRenderThread() const noexcept
{
    return m_ownerThread == std::thread::id{} ||
           m_ownerThread == std::this_thread::get_id();
}

bool DiligentOverlayRenderer::CheckRenderThread(
    const char* operation) const noexcept
{
    if (IsRenderThread())
    {
        return true;
    }

    LOG_CAT_ERROR(
        Render,
        "%s called from non-render thread",
        operation ? operation : "overlay operation");
    return false;
}

bool DiligentOverlayRenderer::Initialize(
    DiligentRuntime::DiligentGraphicsRuntime& runtime,
    std::uint32_t frameSlotCount)
{
    if (m_ready)
    {
        return CheckRenderThread("Initialize");
    }
    auto services = runtime.Services();
    if (!services.IsBound())
    {
        return false;
    }

    m_ownerThread = std::this_thread::get_id();
    auto rollback = [this]() noexcept
    {
        Shutdown();
        return false;
    };

    using namespace Diligent;

    m_runtime = &runtime;
    m_nativeResources = &runtime.NativeResources();
    auto& dev = *services.Device();
    auto& sc = *services.SwapChain();

    RefCntAutoPtr<IShader> vsShader;
    RefCntAutoPtr<IShader> psShader;
    {
        ShaderCreateInfo ci;
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.UseCombinedTextureSamplers = True;
        ci.Desc.CombinedSamplerSuffix = "_sampler";

        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name = "Overlay VS";
        ci.EntryPoint = "main";
        ci.Source = kOverlayVS;
        dev.CreateShader(ci, &vsShader);

        ci.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ci.Desc.Name = "Overlay PS";
        ci.Source = kOverlayPS;
        dev.CreateShader(ci, &psShader);

        if (!vsShader || !psShader)
        {
            LOG_CAT_ERROR(Render, "failed to compile shaders");
            return rollback();
        }
    }

    {
        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "OverlayPSO";
        psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
        psoCI.pVS = vsShader;
        psoCI.pPS = psShader;

        const auto& scDesc = sc.GetDesc();
        psoCI.GraphicsPipeline.NumRenderTargets = 1;
        psoCI.GraphicsPipeline.RTVFormats[0] = scDesc.ColorBufferFormat;
        psoCI.GraphicsPipeline.DSVFormat = scDesc.DepthBufferFormat;
        psoCI.GraphicsPipeline.PrimitiveTopology =
            PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;
        psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        psoCI.GraphicsPipeline.RasterizerDesc.ScissorEnable = True;

        auto& rt0 = psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
        rt0.BlendEnable = True;
        rt0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
        rt0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOp = BLEND_OPERATION_ADD;
        rt0.SrcBlendAlpha = BLEND_FACTOR_ONE;
        rt0.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOpAlpha = BLEND_OPERATION_ADD;
        rt0.RenderTargetWriteMask = COLOR_MASK_ALL;

        LayoutElement layoutElems[] =
        {
            {0, 0, 2, VT_FLOAT32, False},
            {1, 0, 2, VT_FLOAT32, False},
            {2, 0, 4, VT_UINT8, True},
        };
        psoCI.GraphicsPipeline.InputLayout.NumElements = 3;
        psoCI.GraphicsPipeline.InputLayout.LayoutElements = layoutElems;

        ShaderResourceVariableDesc vars[] =
        {
            {SHADER_TYPE_PIXEL, "g_Texture",
             SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        psoCI.PSODesc.ResourceLayout.Variables = vars;
        psoCI.PSODesc.ResourceLayout.NumVariables = 1;

        SamplerDesc samplerDesc;
        samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
        samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
        samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
        samplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = TEXTURE_ADDRESS_CLAMP;

        ImmutableSamplerDesc samplers[] =
        {
            {SHADER_TYPE_PIXEL, "g_Texture", samplerDesc},
        };
        psoCI.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
        psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 1;

        dev.CreateGraphicsPipelineState(psoCI, &m_pso);
        if (!m_pso)
        {
            LOG_CAT_ERROR(Render, "failed to create PSO");
            return rollback();
        }
    }

    {
        BufferDesc cbDesc;
        cbDesc.Name = "OverlayCB";
        cbDesc.Size = sizeof(float) * 16;
        cbDesc.Usage = USAGE_DYNAMIC;
        cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        dev.CreateBuffer(cbDesc, nullptr, &m_cb);
        if (!m_cb)
        {
            LOG_CAT_ERROR(Render, "failed to create projection constant buffer");
            return rollback();
        }

        if (auto* var = m_pso->GetStaticVariableByName(
                SHADER_TYPE_VERTEX, "OverlayCB"))
        {
            var->Set(m_cb);
        }
    }

    m_binding = runtime.CreateOverlayBinding(*m_pso);
    if (!m_binding.IsValid())
    {
        LOG_CAT_ERROR(Render, "failed to create shader resource binding");
        return rollback();
    }

    if (frameSlotCount == 0u)
    {
        frameSlotCount = kDefaultOverlayFrameSlots;
    }
    try
    {
        m_frameBuffers.assign(frameSlotCount, {});
    }
    catch (const std::bad_alloc&)
    {
        LOG_CAT_ERROR(Render, "failed to allocate overlay frame buffers");
        return rollback();
    }

    m_runtime = &runtime;
    m_ready = true;
    return true;
}

void DiligentOverlayRenderer::Shutdown()
{
    if (!CheckRenderThread("Shutdown"))
    {
        return;
    }

    if (m_nativeResources)
    {
        for (const auto nativeTexture : m_textureRegistry.DestroyAll())
        {
            m_nativeResources->DestroyTexture(nativeTexture);
        }
    }
    if (m_runtime)
    {
        m_runtime->DestroyMaterialBinding(m_binding);
    }
    m_binding = {};
    m_runtime = nullptr;
    m_pso.Release();
    m_cb.Release();
    m_frameBuffers.clear();
    m_ready = false;
    m_nativeResources = nullptr;
    m_ownerThread = {};
}

RenderOverlayTextureHandle DiligentOverlayRenderer::CreateTexture(
    const RenderOverlayTextureDesc& desc,
    const std::uint8_t* rgbaPixels)
{
    if (!CheckRenderThread("CreateTexture"))
    {
        return {};
    }
    if (!m_ready || !m_nativeResources || desc.width == 0 ||
        desc.height == 0 || !rgbaPixels)
    {
        return {};
    }

    if (desc.width >
        std::numeric_limits<std::uint32_t>::max() / 4u)
    {
        return {};
    }

    const auto rowBytes = desc.width * 4u;
    const auto rowPitch = desc.rowPitchBytes != 0
        ? desc.rowPitchBytes
        : rowBytes;
    if (rowPitch < rowBytes)
    {
        return {};
    }
    if (desc.height >
        std::numeric_limits<std::uint64_t>::max() /
            static_cast<std::uint64_t>(rowPitch))
    {
        return {};
    }

    Graphics::TextureDesc textureDesc;
    textureDesc.debugName = "Overlay Texture";
    textureDesc.width = desc.width;
    textureDesc.height = desc.height;
    textureDesc.format = Graphics::ResourceFormat::Rgba8Unorm;
    textureDesc.usage = Graphics::TextureUsageFlags::Sampled;

    const Graphics::GraphicsDataView data{
        rgbaPixels,
        static_cast<std::uint64_t>(rowPitch) * desc.height,
        rowPitch,
        0u,
    };

    auto texture = m_nativeResources->CreateStandaloneTexture(
        textureDesc, data);
    if (!texture.IsValid())
    {
        return {};
    }

    auto handle = m_textureRegistry.Create(texture);
    if (!handle)
    {
        m_nativeResources->DestroyTexture(texture);
    }
    return handle;
}

void DiligentOverlayRenderer::DestroyTexture(
    RenderOverlayTextureHandle texture)
{
    if (!CheckRenderThread("DestroyTexture"))
    {
        return;
    }
    if (!texture || !m_nativeResources)
    {
        return;
    }

    const auto destroyed = m_textureRegistry.Destroy(texture);
    if (destroyed.destroyed)
    {
        m_nativeResources->DestroyTexture(destroyed.nativeTexture);
    }
}

void DiligentOverlayRenderer::Render(
    const RenderOverlayFrame& frame,
    std::uint64_t activeFrameSerial,
    std::uint32_t activeFrameSlot)
{
    if (!CheckRenderThread("Render"))
    {
        return;
    }
    const auto services = m_runtime ? m_runtime->Services()
                                    : DiligentDeviceServices{};
    if (!m_ready || !services.IsBound() || m_frameBuffers.empty())
    {
        return;
    }
    if (!ValidateOverlayFrameForTests(frame))
    {
        return;
    }
    auto binding = m_runtime ? m_runtime->ResolveOverlayBinding(m_binding)
                             : DiligentRuntime::NativeOverlayBindingView{};
    if (!binding.IsValid())
    {
        return;
    }

    std::size_t totalVertexCount = 0;
    std::size_t totalIndexCount = 0;
    for (const auto& list : frame.drawLists)
    {
        totalVertexCount += list.vertices.size();
        totalIndexCount += list.indices.size();
    }
    if (totalVertexCount == 0 || totalIndexCount == 0)
    {
        return;
    }

    auto& frameBuffers =
        m_frameBuffers[activeFrameSlot % m_frameBuffers.size()];

    using namespace Diligent;

    auto* ctx = services.ImmediateContext();
    auto* dev = services.Device();

    const auto vbNeeded = ByteSizeOrZero(
        totalVertexCount, sizeof(RenderOverlayVertex));
    if (vbNeeded == 0)
    {
        return;
    }
    if (vbNeeded > frameBuffers.vbSize)
    {
        frameBuffers.vb.Release();
        frameBuffers.vbSize = vbNeeded + 4096u;

        BufferDesc desc;
        desc.Name = "Overlay VB";
        desc.Size = frameBuffers.vbSize;
        desc.BindFlags = BIND_VERTEX_BUFFER;
        desc.Usage = USAGE_DYNAMIC;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        dev->CreateBuffer(desc, nullptr, &frameBuffers.vb);
    }

    const auto ibNeeded = ByteSizeOrZero(
        totalIndexCount, sizeof(std::uint32_t));
    if (ibNeeded == 0)
    {
        return;
    }
    if (ibNeeded > frameBuffers.ibSize)
    {
        frameBuffers.ib.Release();
        frameBuffers.ibSize = ibNeeded + 4096u;

        BufferDesc desc;
        desc.Name = "Overlay IB";
        desc.Size = frameBuffers.ibSize;
        desc.BindFlags = BIND_INDEX_BUFFER;
        desc.Usage = USAGE_DYNAMIC;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        dev->CreateBuffer(desc, nullptr, &frameBuffers.ib);
    }

    if (!frameBuffers.vb || !frameBuffers.ib)
    {
        return;
    }

    {
        MapHelper<RenderOverlayVertex> vtx(
            ctx, frameBuffers.vb, MAP_WRITE, MAP_FLAG_DISCARD);
        MapHelper<std::uint32_t> idx(
            ctx, frameBuffers.ib, MAP_WRITE, MAP_FLAG_DISCARD);

        auto* vtxDst = static_cast<RenderOverlayVertex*>(vtx);
        auto* idxDst = static_cast<std::uint32_t*>(idx);
        for (const auto& list : frame.drawLists)
        {
            if (!list.vertices.empty())
            {
                const auto vertexBytes =
                    list.vertices.size() * sizeof(RenderOverlayVertex);
                std::memcpy(vtxDst, list.vertices.data(), vertexBytes);
                vtxDst += list.vertices.size();
            }
            if (!list.indices.empty())
            {
                const auto indexBytes =
                    list.indices.size() * sizeof(std::uint32_t);
                std::memcpy(idxDst, list.indices.data(), indexBytes);
                idxDst += list.indices.size();
            }
        }
    }

    {
        const float L = frame.displayPos[0];
        const float R = L + frame.displaySize[0];
        const float T = frame.displayPos[1];
        const float B = T + frame.displaySize[1];

        const float mvp[16] =
        {
            2.0f / (R - L),      0.0f,                0.0f, 0.0f,
            0.0f,                2.0f / (T - B),      0.0f, 0.0f,
            0.0f,                0.0f,                0.5f, 0.0f,
            (R + L) / (L - R),   (T + B) / (B - T),   0.5f, 1.0f,
        };

        MapHelper<float> cb(ctx, m_cb, MAP_WRITE, MAP_FLAG_DISCARD);
        std::memcpy(cb, mvp, sizeof(mvp));
    }

    ctx->SetPipelineState(m_pso);

    {
        IBuffer* vbs[] = {frameBuffers.vb};
        Uint64 offsets[] = {0};
        ctx->SetVertexBuffers(
            0, 1, vbs, offsets,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            SET_VERTEX_BUFFERS_FLAG_RESET);
        ctx->SetIndexBuffer(
            frameBuffers.ib, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    const float clipOffX = frame.displayPos[0];
    const float clipOffY = frame.displayPos[1];
    const float scaleX = frame.framebufferScale[0];
    const float scaleY = frame.framebufferScale[1];
    const auto framebufferWidth = static_cast<Int32>(
        std::max(0.0f, frame.displaySize[0] * scaleX));
    const auto framebufferHeight = static_cast<Int32>(
        std::max(0.0f, frame.displaySize[1] * scaleY));
    const auto clampScissor =
        [](float value, Int32 maxValue) noexcept -> Int32
        {
            const float clamped = std::clamp(
                value, 0.0f, static_cast<float>(maxValue));
            return static_cast<Int32>(clamped);
        };

    std::uint32_t globalIndexOffset = 0;
    std::uint32_t globalVertexOffset = 0;

    for (const auto& list : frame.drawLists)
    {
        for (const auto& cmd : list.commands)
        {
            if (cmd.elementCount == 0)
            {
                continue;
            }

            const auto texture = ResolveTexture(cmd.texture);
            if (!texture.srv)
            {
                continue;
            }
            if (m_nativeResources && activeFrameSerial != 0u)
            {
                m_nativeResources->MarkTextureUsed(
                    texture.handle, activeFrameSerial);
            }

            const float clipLeft =
                (cmd.clipRect.left - clipOffX) * scaleX;
            const float clipTop =
                (cmd.clipRect.top - clipOffY) * scaleY;
            const float clipRight =
                (cmd.clipRect.right - clipOffX) * scaleX;
            const float clipBottom =
                (cmd.clipRect.bottom - clipOffY) * scaleY;
            if (!std::isfinite(clipLeft) || !std::isfinite(clipTop) ||
                !std::isfinite(clipRight) || !std::isfinite(clipBottom))
            {
                LOG_CAT_ERROR(Render, "overlay command clip rect is not finite");
                continue;
            }

            Rect scissor;
            scissor.left = clampScissor(clipLeft, framebufferWidth);
            scissor.top = clampScissor(clipTop, framebufferHeight);
            scissor.right = clampScissor(clipRight, framebufferWidth);
            scissor.bottom = clampScissor(clipBottom, framebufferHeight);
            if (scissor.right <= scissor.left ||
                scissor.bottom <= scissor.top)
            {
                continue;
            }

            ctx->SetScissorRects(1, &scissor, 0, 0);

            binding.textureVariable->Set(texture.srv);
            ctx->CommitShaderResources(
                binding.srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            DrawIndexedAttribs drawAttribs;
            drawAttribs.NumIndices = cmd.elementCount;
            drawAttribs.IndexType = VT_UINT32;
            drawAttribs.FirstIndexLocation =
                globalIndexOffset + cmd.indexOffset;
            drawAttribs.BaseVertex = static_cast<Int32>(
                globalVertexOffset + cmd.vertexOffset);
            drawAttribs.Flags = DRAW_FLAG_NONE;
            ctx->DrawIndexed(drawAttribs);
        }

        if (AddWillOverflow(
                globalIndexOffset,
                static_cast<std::uint32_t>(list.indices.size())) ||
            AddWillOverflow(
                globalVertexOffset,
                static_cast<std::uint32_t>(list.vertices.size())))
        {
            LOG_CAT_ERROR(Render, "overlay global offsets overflowed");
            return;
        }
        globalIndexOffset += static_cast<std::uint32_t>(list.indices.size());
        globalVertexOffset += static_cast<std::uint32_t>(list.vertices.size());
    }
}

bool DiligentOverlayRenderer::ReconfigureFrameSlots(
    std::uint32_t frameSlotCount)
{
    if (!CheckRenderThread("ReconfigureFrameSlots"))
    {
        return false;
    }
    if (frameSlotCount == 0u)
    {
        frameSlotCount = kDefaultOverlayFrameSlots;
    }
    if (m_frameBuffers.size() == frameSlotCount)
    {
        return true;
    }

    m_frameBuffers.assign(frameSlotCount, {});
    return true;
}

DiligentOverlayRenderer::ResolvedTexture DiligentOverlayRenderer::ResolveTexture(
    RenderOverlayTextureHandle texture) noexcept
{
    if (!CheckRenderThread("ResolveTexture") || !texture || !m_nativeResources)
    {
        return {};
    }

    const auto nativeTexture = m_textureRegistry.Resolve(texture);
    if (!nativeTexture.IsValid())
    {
        return {};
    }

    return {nativeTexture, m_nativeResources->ResolveTextureSrv(nativeTexture)};
}

} // namespace SagaEngine::Render::Backend
