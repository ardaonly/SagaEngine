/// @file DiligentGraphicsRuntime.cpp
/// @brief Private Diligent runtime frame, upload, and command vocabulary.

#include "SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentGraphicsRuntime.h"

#include "SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentBindingCache.h"
#include "SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentDeviceFactory.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentFallbackResources.h"
#include "SagaEngine/Core/Log/LogCategories.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentFrameSlotTracker.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentGpuTimeline.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

#include "DeviceContext.h"
#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"
#include "SwapChain.h"

#include <algorithm>
#include <utility>

namespace SagaEngine::Graphics::Backends::Diligent::Runtime
{

struct DiligentGraphicsRuntime::NativeState
{
    struct MaterialBindingSlot
    {
        ::Diligent::RefCntAutoPtr<::Diligent::IShaderResourceBinding> srb;
        ::Diligent::IShaderResourceVariable* albedoVariable = nullptr;
        ::Diligent::IShaderResourceVariable* shadowVariable = nullptr;
        ::Diligent::IShaderResourceVariable* textureVariable = nullptr;
        std::uint32_t generation = 0;
        bool live = false;
    };
    struct PendingSrbRelease
    {
        ::Diligent::RefCntAutoPtr<::Diligent::IShaderResourceBinding> srb;
        std::uint64_t retireAfterSerial = 0;
    };

    RenderBackend::RenderBackendConfig config{};
    RenderBackend::GraphicsBackendAPI selectedAPI =
        RenderBackend::GraphicsBackendAPI::kAuto;
    ::Diligent::RefCntAutoPtr<::Diligent::IRenderDevice> device;
    ::Diligent::RefCntAutoPtr<::Diligent::IDeviceContext> context;
    ::Diligent::RefCntAutoPtr<::Diligent::ISwapChain> swapChain;
    RenderBackend::DiligentNativeResourceOwner nativeResources;
    RenderBackend::DiligentGpuTimeline gpuTimeline;
    RenderBackend::DiligentFrameSlotTracker frameSlots;
    ::SagaEngine::Graphics::Backends::Diligent::DiligentBindingCache
        bindingCache;
    ::SagaEngine::Graphics::Backends::Diligent::DiligentFallbackResources
        fallbackResources;
    std::vector<MaterialBindingSlot> materialBindings;
    std::vector<PendingSrbRelease> pendingSrbs;
    std::uint64_t frameIndex = 0;
    bool initialized = false;
};

DiligentFrameUploadArena::DiligentFrameUploadArena(
    std::uint32_t frameSlotCount,
    std::uint64_t uniformChunkBytes,
    std::uint64_t transientChunkBytes,
    std::uint64_t constantBufferAlignment)
{
    Configure(
        frameSlotCount,
        uniformChunkBytes,
        transientChunkBytes,
        constantBufferAlignment);
}

void DiligentFrameUploadArena::Configure(
    std::uint32_t frameSlotCount,
    std::uint64_t uniformChunkBytes,
    std::uint64_t transientChunkBytes,
    std::uint64_t constantBufferAlignment)
{
    m_slots.assign(std::max(frameSlotCount, 1u), {});
    m_uniformChunkBytes = std::max<std::uint64_t>(uniformChunkBytes, 1u);
    m_transientChunkBytes = std::max<std::uint64_t>(transientChunkBytes, 1u);
    m_constantBufferAlignment =
        std::max<std::uint64_t>(constantBufferAlignment, 1u);
    m_activeToken = {};
    m_diagnostics = {};

    for (auto& slot : m_slots)
    {
        slot.uniformChunks.push_back({std::vector<std::byte>(
            static_cast<std::size_t>(m_uniformChunkBytes)), 0u});
        slot.transientChunks.push_back({std::vector<std::byte>(
            static_cast<std::size_t>(m_transientChunkBytes)), 0u});
    }
}

void DiligentFrameUploadArena::BeginFrame(FrameToken token)
{
    if (m_slots.empty())
    {
        Configure(1u, 1u, 1u, m_constantBufferAlignment);
    }

    token.frameSlot %= static_cast<std::uint32_t>(m_slots.size());
    auto& slot = m_slots[token.frameSlot];
    slot.generation = token.frameSlotGeneration;
    for (auto& chunk : slot.uniformChunks)
    {
        chunk.cursor = 0u;
    }
    for (auto& chunk : slot.transientChunks)
    {
        chunk.cursor = 0u;
    }
    m_activeToken = token;
}

ConstantSlice DiligentFrameUploadArena::AllocateUniform(
    std::uint64_t byteSize)
{
    if (m_slots.empty() || !m_activeToken.IsValid())
    {
        return {};
    }

    auto& slot = m_slots[m_activeToken.frameSlot % m_slots.size()];
    m_diagnostics.largestUniformAllocation =
        std::max(m_diagnostics.largestUniformAllocation, byteSize);
    return Allocate(
        slot.uniformChunks,
        m_uniformChunkBytes,
        byteSize,
        true);
}

ConstantSlice DiligentFrameUploadArena::AllocateTransient(
    std::uint64_t byteSize)
{
    if (m_slots.empty() || !m_activeToken.IsValid())
    {
        return {};
    }

    auto& slot = m_slots[m_activeToken.frameSlot % m_slots.size()];
    m_diagnostics.largestTransientAllocation =
        std::max(m_diagnostics.largestTransientAllocation, byteSize);
    return Allocate(
        slot.transientChunks,
        m_transientChunkBytes,
        byteSize,
        false);
}

UploadArenaDiagnostics DiligentFrameUploadArena::Diagnostics()
    const noexcept
{
    return m_diagnostics;
}

std::uint64_t DiligentFrameUploadArena::ConstantBufferAlignment()
    const noexcept
{
    return m_constantBufferAlignment;
}

ConstantSlice DiligentFrameUploadArena::Allocate(
    std::vector<Chunk>& chunks,
    std::uint64_t primaryChunkBytes,
    std::uint64_t byteSize,
    bool uniform)
{
    if (byteSize == 0u)
    {
        return {};
    }

    for (std::uint32_t index = 0; index < chunks.size(); ++index)
    {
        auto& chunk = chunks[index];
        const auto alignedOffset =
            AlignUp(chunk.cursor, m_constantBufferAlignment);
        if (alignedOffset + byteSize <= chunk.bytes.size())
        {
            chunk.cursor = alignedOffset + byteSize;
            return {
                m_activeToken,
                index,
                alignedOffset,
                byteSize,
                m_constantBufferAlignment,
                chunk.bytes.data() + alignedOffset,
            };
        }
    }

    if (uniform)
    {
        ++m_diagnostics.uniformOverflowCount;
    }
    else
    {
        ++m_diagnostics.transientOverflowCount;
    }
    ++m_diagnostics.secondaryChunkCount;

    const auto secondaryBytes = std::max(
        AlignUp(byteSize, m_constantBufferAlignment),
        primaryChunkBytes);
    chunks.push_back({std::vector<std::byte>(
        static_cast<std::size_t>(secondaryBytes)), 0u});

    auto& chunk = chunks.back();
    const auto alignedOffset = AlignUp(chunk.cursor, m_constantBufferAlignment);
    chunk.cursor = alignedOffset + byteSize;
    return {
        m_activeToken,
        static_cast<std::uint32_t>(chunks.size() - 1u),
        alignedOffset,
        byteSize,
        m_constantBufferAlignment,
        chunk.bytes.data() + alignedOffset,
    };
}

std::uint64_t DiligentFrameUploadArena::AlignUp(
    std::uint64_t value,
    std::uint64_t alignment) noexcept
{
    if (alignment <= 1u)
    {
        return value;
    }
    const auto remainder = value % alignment;
    return remainder == 0u ? value : value + (alignment - remainder);
}

GraphicsRenderPassEncoder::GraphicsRenderPassEncoder(
    GraphicsCommandEncoder& encoder)
    : m_encoder(&encoder)
{
}

bool GraphicsRenderPassEncoder::ClearColor(
    TextureHandle target,
    const float rgba[4])
{
    return m_encoder && m_encoder->ClearColor(target, rgba);
}

bool GraphicsRenderPassEncoder::ClearDepth(TextureHandle target, float depth)
{
    return m_encoder && m_encoder->ClearDepth(target, depth);
}

bool GraphicsRenderPassEncoder::SetPipeline(PipelineHandle pipeline)
{
    return m_encoder && m_encoder->SetPipeline(pipeline);
}

bool GraphicsRenderPassEncoder::SetBindingSet(BindingSetHandle bindingSet)
{
    return m_encoder && m_encoder->SetBindingSet(bindingSet);
}

bool GraphicsRenderPassEncoder::SetVertexBuffer(BufferHandle buffer)
{
    return m_encoder && m_encoder->SetVertexBuffer(buffer);
}

bool GraphicsRenderPassEncoder::SetIndexBuffer(BufferHandle buffer)
{
    return m_encoder && m_encoder->SetIndexBuffer(buffer);
}

bool GraphicsRenderPassEncoder::SetViewport(GraphicsViewport viewport)
{
    return m_encoder && m_encoder->SetViewport(viewport);
}

bool GraphicsRenderPassEncoder::SetScissor(GraphicsScissor scissor)
{
    return m_encoder && m_encoder->SetScissor(scissor);
}

bool GraphicsRenderPassEncoder::Draw(std::uint32_t vertexCount)
{
    return m_encoder && m_encoder->Draw(vertexCount);
}

bool GraphicsRenderPassEncoder::DrawIndexed(std::uint32_t indexCount)
{
    return m_encoder && m_encoder->DrawIndexed(indexCount);
}

bool GraphicsRenderPassEncoder::EndRenderPass()
{
    return m_encoder && m_encoder->EndRenderPass();
}

GraphicsCommandEncoder::GraphicsCommandEncoder(FrameToken token)
    : m_token(token)
{
}

GraphicsRenderPassEncoder GraphicsCommandEncoder::BeginRenderPass()
{
    if (!RequireFrameOpen())
    {
        return GraphicsRenderPassEncoder(*this);
    }
    if (m_passOpen)
    {
        (void)Reject(GraphicsCommandError::RenderPassAlreadyOpen);
        return GraphicsRenderPassEncoder(*this);
    }

    m_passOpen = true;
    ++m_diagnostics.passCount;
    return GraphicsRenderPassEncoder(*this);
}

bool GraphicsCommandEncoder::TransitionResource(GraphicsHandle)
{
    return RequireFrameOpen();
}

bool GraphicsCommandEncoder::ClearColor(TextureHandle, const float[4])
{
    return RequirePassOpen();
}

bool GraphicsCommandEncoder::ClearDepth(TextureHandle, float)
{
    return RequirePassOpen();
}

bool GraphicsCommandEncoder::SetPipeline(PipelineHandle)
{
    return RequirePassOpen();
}

bool GraphicsCommandEncoder::SetBindingSet(BindingSetHandle)
{
    return RequirePassOpen();
}

bool GraphicsCommandEncoder::SetVertexBuffer(BufferHandle)
{
    return RequirePassOpen();
}

bool GraphicsCommandEncoder::SetIndexBuffer(BufferHandle)
{
    return RequirePassOpen();
}

bool GraphicsCommandEncoder::SetViewport(GraphicsViewport)
{
    return RequirePassOpen();
}

bool GraphicsCommandEncoder::SetScissor(GraphicsScissor)
{
    return RequirePassOpen();
}

bool GraphicsCommandEncoder::Draw(std::uint32_t)
{
    if (!RequirePassOpen())
    {
        return false;
    }
    ++m_diagnostics.drawCount;
    return true;
}

bool GraphicsCommandEncoder::DrawIndexed(std::uint32_t)
{
    if (!RequirePassOpen())
    {
        return false;
    }
    ++m_diagnostics.drawCount;
    ++m_diagnostics.indexedDrawCount;
    return true;
}

bool GraphicsCommandEncoder::EndRenderPass()
{
    if (!RequirePassOpen())
    {
        return false;
    }
    m_passOpen = false;
    return true;
}

bool GraphicsCommandEncoder::Submit()
{
    if (!RequireFrameOpen())
    {
        return false;
    }
    if (m_passOpen)
    {
        return Reject(GraphicsCommandError::RenderPassAlreadyOpen);
    }
    m_submitted = true;
    ++m_diagnostics.submitCount;
    return true;
}

bool GraphicsCommandEncoder::Present()
{
    if (!m_submitted)
    {
        return Reject(GraphicsCommandError::Submitted);
    }
    ++m_diagnostics.presentCount;
    return true;
}

const FrameToken& GraphicsCommandEncoder::Token() const noexcept
{
    return m_token;
}

GraphicsCommandDiagnostics GraphicsCommandEncoder::Diagnostics()
    const noexcept
{
    return m_diagnostics;
}

bool GraphicsCommandEncoder::Reject(GraphicsCommandError error) noexcept
{
    m_diagnostics.lastError = error;
    ++m_diagnostics.rejectedCommandCount;
    return false;
}

bool GraphicsCommandEncoder::RequireFrameOpen() noexcept
{
    if (!m_token.IsValid())
    {
        return Reject(GraphicsCommandError::FrameNotOpen);
    }
    if (m_submitted)
    {
        return Reject(GraphicsCommandError::Submitted);
    }
    return true;
}

bool GraphicsCommandEncoder::RequirePassOpen() noexcept
{
    if (!RequireFrameOpen())
    {
        return false;
    }
    if (!m_passOpen)
    {
        return Reject(GraphicsCommandError::RenderPassNotOpen);
    }
    return true;
}

DiligentGraphicsRuntime::DiligentGraphicsRuntime(std::uint32_t frameSlotCount)
    : m_native(std::make_unique<NativeState>())
    , m_slotGenerations(std::max(frameSlotCount, 1u), 0u)
    , m_uploads(
          std::max(frameSlotCount, 1u),
          64u * 1024u,
          256u * 1024u,
          256u)
{
}

DiligentGraphicsRuntime::~DiligentGraphicsRuntime()
{
    try
    {
        Shutdown();
    }
    catch (...)
    {
    }
}

bool DiligentGraphicsRuntime::Initialize(
    const RenderBackend::RenderBackendConfig& config,
    const RenderBackend::SwapchainDesc& swapchain)
{
    if (m_native->initialized)
    {
        return true;
    }
    if (swapchain.width == 0u || swapchain.height == 0u)
    {
        LOG_CAT_ERROR(Render, "Initialize called with zero-sized swapchain");
        return false;
    }

    m_native->config = config;
    const auto picked = ::SagaEngine::Render::Backend::TryInitAPI(
        config.preferredAPI,
        swapchain,
        config,
        m_native->device,
        m_native->context,
        m_native->swapChain);
    if (picked == RenderBackend::GraphicsBackendAPI::kAuto)
    {
        LOG_CAT_ERROR(Render, "No Diligent graphics API was available for the host");
        return false;
    }

    m_native->selectedAPI = picked;
    m_native->initialized = true;
    m_native->frameIndex = 0;
    m_native->nativeResources.Bind(Services());
    if (!m_native->gpuTimeline.Initialize(
            m_native->device.RawPtr(),
            m_native->context.RawPtr()))
    {
        LOG_CAT_ERROR(Render, "Failed to create GPU completion fence");
        Shutdown();
        return false;
    }

    const auto& scDesc = m_native->swapChain->GetDesc();
    const auto frameSlotCount =
        scDesc.BufferCount == 0u ? 3u : scDesc.BufferCount;
    (void)m_native->frameSlots.Configure(frameSlotCount);
    m_slotGenerations.assign(frameSlotCount, 0u);
    m_uploads.Configure(
        frameSlotCount,
        64u * 1024u,
        256u * 1024u,
        std::max<std::uint64_t>(
            m_native->device->GetAdapterInfo().Buffer
                .ConstantBufferOffsetAlignment,
            1u));
    return true;
}

void DiligentGraphicsRuntime::Shutdown()
{
    if (!m_native || !m_native->initialized)
    {
        return;
    }

    m_native->gpuTimeline.ShutdownAndDrain();
    RetireCompleted(m_native->gpuTimeline.LastCompletedSerial());
    ::SagaEngine::Graphics::Backends::Diligent::DiligentNativeBindingDiagnostics
        shutdownDiagnostics{};
    m_native->fallbackResources.Release(
        m_native->nativeResources,
        shutdownDiagnostics);
    m_native->bindingCache.ForceRelease(shutdownDiagnostics);
    m_native->pendingSrbs.clear();
    ReleaseMaterialBindings();
    m_native->nativeResources.ReleaseAll();
    m_native->gpuTimeline.Reset();
    m_native->frameSlots.Reset();
    if (m_native->context)
    {
        m_native->context->Flush();
    }
    m_native->swapChain.Release();
    m_native->context.Release();
    m_native->device.Release();
    m_native->selectedAPI = RenderBackend::GraphicsBackendAPI::kAuto;
    m_native->initialized = false;
}

void DiligentGraphicsRuntime::Resize(
    std::uint32_t width,
    std::uint32_t height)
{
    if (!m_native->initialized || !m_native->swapChain)
    {
        return;
    }
    if (width == 0u || height == 0u)
    {
        return;
    }

    const auto oldFrameSlotCount = m_native->frameSlots.FrameSlotCount();
    m_native->swapChain->Resize(width, height);
    const auto& scDesc = m_native->swapChain->GetDesc();
    const auto newFrameSlotCount =
        scDesc.BufferCount == 0u ? 3u : scDesc.BufferCount;
    if (newFrameSlotCount == oldFrameSlotCount)
    {
        return;
    }

    const auto latestSlotSerial = m_native->frameSlots.LatestSubmittedSerial();
    if (latestSlotSerial != 0u)
    {
        m_native->gpuTimeline.WaitForSerial(latestSlotSerial);
        RetireCompleted(m_native->gpuTimeline.LastCompletedSerial());
    }
    (void)m_native->frameSlots.Configure(newFrameSlotCount);
    m_slotGenerations.assign(newFrameSlotCount, 0u);
    m_uploads.Configure(
        newFrameSlotCount,
        64u * 1024u,
        256u * 1024u,
        m_uploads.ConstantBufferAlignment());
}

void DiligentGraphicsRuntime::PresentActiveFrame()
{
    if (!m_native->initialized || !m_native->swapChain ||
        !m_native->frameSlots.ActiveFrameOpen())
    {
        return;
    }

    const auto activeFrameSerial = m_native->frameSlots.ActiveFrameSerial();
    if (activeFrameSerial != 0u)
    {
        m_native->gpuTimeline.SignalFrameSubmitted(activeFrameSerial);
    }
    m_native->swapChain->Present();
    m_native->frameSlots.EndFrame();
    ++m_native->frameIndex;
}

GraphicsFrameContext DiligentGraphicsRuntime::BeginFrame(
    std::uint64_t completedSerial)
{
    if (!m_native->initialized)
    {
        return {};
    }

    const auto completed = std::max(
        completedSerial,
        m_native->gpuTimeline.PollCompletion());
    RetireCompleted(completed);
    const auto slotBegin =
        m_native->frameSlots.BeginFrame(m_native->frameIndex, completed);
    if (!slotBegin.valid)
    {
        return {};
    }
    if (slotBegin.waitRequired)
    {
        m_native->gpuTimeline.WaitForSerial(slotBegin.waitSerial);
        RetireCompleted(m_native->gpuTimeline.LastCompletedSerial());
        m_native->frameSlots.MarkWaitCompleted(
            m_native->gpuTimeline.LastCompletedSerial());
    }
    const auto submissionSerial =
        m_native->gpuTimeline.BeginFrameSubmission();
    m_native->frameSlots.BeginSubmission(submissionSerial);

    if (m_slotGenerations.size() != m_native->frameSlots.FrameSlotCount())
    {
        m_slotGenerations.assign(m_native->frameSlots.FrameSlotCount(), 0u);
    }
    const auto slot = m_native->frameSlots.ActiveFrameSlot();
    auto& generation = m_slotGenerations[slot];
    ++generation;

    FrameToken token{};
    token.frameIndex = m_native->frameIndex;
    token.frameSlot = slot;
    token.frameSlotGeneration = generation;
    token.submissionSerial = submissionSerial;
    token.completedSerial = m_native->gpuTimeline.LastCompletedSerial();
    m_lastToken = token;

    m_uploads.BeginFrame(token);
    return {
        token,
        &m_uploads,
        std::make_unique<GraphicsCommandEncoder>(token),
    };
}

void DiligentGraphicsRuntime::Submit(GraphicsCommandEncoder& encoder)
{
    (void)encoder.Submit();
}

void DiligentGraphicsRuntime::Present(GraphicsCommandEncoder& encoder)
{
    if (!encoder.Present())
    {
        return;
    }
    PresentActiveFrame();
    const auto completed = m_native->gpuTimeline.PollCompletion();
    RetireCompleted(completed);
}

std::uint64_t DiligentGraphicsRuntime::DeferredReleaseSerial()
    const noexcept
{
    if (!m_native)
    {
        return 0u;
    }

    const auto active = m_native->frameSlots.ActiveFrameSerial();
    if (active != 0u)
    {
        return active;
    }
    return m_native->frameSlots.LatestSubmittedSerial();
}

void DiligentGraphicsRuntime::RetireCompleted(
    std::uint64_t completedSerial) noexcept
{
    if (!m_native)
    {
        return;
    }

    m_native->nativeResources.RetireCompleted(completedSerial);
    m_native->bindingCache.RetireCompleted(completedSerial);
    m_native->pendingSrbs.erase(
        std::remove_if(
            m_native->pendingSrbs.begin(),
            m_native->pendingSrbs.end(),
            [completedSerial](const NativeState::PendingSrbRelease& entry)
            {
                return entry.retireAfterSerial <= completedSerial;
            }),
        m_native->pendingSrbs.end());
}

const FrameToken& DiligentGraphicsRuntime::LastFrameToken() const noexcept
{
    return m_lastToken;
}

RenderBackend::RenderBackendStatus DiligentGraphicsRuntime::Status()
    const noexcept
{
    return {
        m_native->selectedAPI,
        m_native->frameIndex,
        m_native->initialized,
    };
}

bool DiligentGraphicsRuntime::IsInitialized() const noexcept
{
    return m_native && m_native->initialized;
}

::Diligent::IRenderDevice* DiligentGraphicsRuntime::Device() const noexcept
{
    return m_native ? m_native->device.RawPtr() : nullptr;
}

::Diligent::IDeviceContext* DiligentGraphicsRuntime::Context() const noexcept
{
    return m_native ? m_native->context.RawPtr() : nullptr;
}

::Diligent::ISwapChain* DiligentGraphicsRuntime::SwapChain() const noexcept
{
    return m_native ? m_native->swapChain.RawPtr() : nullptr;
}

RenderBackend::DiligentDeviceServices DiligentGraphicsRuntime::Services()
    const noexcept
{
    return {
        Device(),
        Context(),
        SwapChain(),
    };
}

RenderBackend::DiligentNativeResourceOwner&
DiligentGraphicsRuntime::NativeResources() noexcept
{
    return m_native->nativeResources;
}

const RenderBackend::DiligentNativeResourceOwner&
DiligentGraphicsRuntime::NativeResources() const noexcept
{
    return m_native->nativeResources;
}

RenderBackend::DiligentGpuTimeline& DiligentGraphicsRuntime::Timeline()
    noexcept
{
    return m_native->gpuTimeline;
}

const RenderBackend::DiligentGpuTimeline&
DiligentGraphicsRuntime::Timeline() const noexcept
{
    return m_native->gpuTimeline;
}

RenderBackend::DiligentFrameSlotTracker&
DiligentGraphicsRuntime::FrameSlots() noexcept
{
    return m_native->frameSlots;
}

const RenderBackend::DiligentFrameSlotTracker&
DiligentGraphicsRuntime::FrameSlots() const noexcept
{
    return m_native->frameSlots;
}

::SagaEngine::Graphics::Backends::Diligent::DiligentBindingCache&
DiligentGraphicsRuntime::BindingCache() noexcept
{
    return m_native->bindingCache;
}

const ::SagaEngine::Graphics::Backends::Diligent::DiligentBindingCache&
DiligentGraphicsRuntime::BindingCache() const noexcept
{
    return m_native->bindingCache;
}

::SagaEngine::Graphics::Backends::Diligent::DiligentFallbackResources&
DiligentGraphicsRuntime::FallbackResources() noexcept
{
    return m_native->fallbackResources;
}

const ::SagaEngine::Graphics::Backends::Diligent::DiligentFallbackResources&
DiligentGraphicsRuntime::FallbackResources() const noexcept
{
    return m_native->fallbackResources;
}

NativeShaderBindingHandle DiligentGraphicsRuntime::CreateMaterialBinding(
    ::Diligent::IPipelineState& pipeline)
{
    NativeShaderBindingHandle handle{};
    if (!m_native)
    {
        return handle;
    }

    std::uint32_t slotIndex = 0;
    for (std::uint32_t i = 0;
         i < static_cast<std::uint32_t>(m_native->materialBindings.size());
         ++i)
    {
        if (!m_native->materialBindings[i].live)
        {
            slotIndex = i + 1u;
            break;
        }
    }

    if (slotIndex == 0u)
    {
        m_native->materialBindings.push_back({});
        slotIndex = static_cast<std::uint32_t>(
            m_native->materialBindings.size());
    }

    auto& slot = m_native->materialBindings[slotIndex - 1u];
    if (slot.srb)
    {
        m_native->pendingSrbs.push_back(
            {slot.srb, DeferredReleaseSerial()});
        slot.srb.Release();
    }
    slot.albedoVariable = nullptr;
    slot.shadowVariable = nullptr;
    slot.textureVariable = nullptr;
    pipeline.CreateShaderResourceBinding(&slot.srb, true);
    if (!slot.srb)
    {
        return {};
    }

    slot.albedoVariable = slot.srb->GetVariableByName(
        ::Diligent::SHADER_TYPE_PIXEL,
        "g_Albedo");
    slot.shadowVariable = slot.srb->GetVariableByName(
        ::Diligent::SHADER_TYPE_PIXEL,
        "g_ShadowMap");
    ++slot.generation;
    if (slot.generation == 0u)
    {
        slot.generation = 1u;
    }
    slot.live = true;
    handle.index = slotIndex;
    handle.generation = slot.generation;
    return handle;
}

NativeShaderBindingHandle DiligentGraphicsRuntime::CreateOverlayBinding(
    ::Diligent::IPipelineState& pipeline)
{
    auto handle = CreateMaterialBinding(pipeline);
    if (!handle.IsValid())
    {
        return {};
    }

    auto& slot = m_native->materialBindings[handle.index - 1u];
    slot.textureVariable = slot.srb->GetVariableByName(
        ::Diligent::SHADER_TYPE_PIXEL,
        "g_Texture");
    if (!slot.textureVariable)
    {
        DestroyMaterialBinding(handle);
        return {};
    }
    return handle;
}

void DiligentGraphicsRuntime::DestroyMaterialBinding(
    NativeShaderBindingHandle handle) noexcept
{
    if (!m_native || !handle.IsValid() ||
        handle.index > m_native->materialBindings.size())
    {
        return;
    }

    auto& slot = m_native->materialBindings[handle.index - 1u];
    if (!slot.live || slot.generation != handle.generation)
    {
        return;
    }

    if (slot.srb)
    {
        m_native->pendingSrbs.push_back(
            {slot.srb, DeferredReleaseSerial()});
        slot.srb.Release();
    }
    slot.albedoVariable = nullptr;
    slot.shadowVariable = nullptr;
    slot.textureVariable = nullptr;
    slot.live = false;
}

NativeShaderBindingView DiligentGraphicsRuntime::ResolveMaterialBinding(
    NativeShaderBindingHandle handle) noexcept
{
    if (!m_native || !handle.IsValid() ||
        handle.index > m_native->materialBindings.size())
    {
        return {};
    }

    auto& slot = m_native->materialBindings[handle.index - 1u];
    if (!slot.live || slot.generation != handle.generation || !slot.srb)
    {
        return {};
    }

    return {
        slot.srb.RawPtr(),
        slot.albedoVariable,
        slot.shadowVariable,
    };
}

NativeOverlayBindingView DiligentGraphicsRuntime::ResolveOverlayBinding(
    NativeShaderBindingHandle handle) noexcept
{
    if (!m_native || !handle.IsValid() ||
        handle.index > m_native->materialBindings.size())
    {
        return {};
    }

    auto& slot = m_native->materialBindings[handle.index - 1u];
    if (!slot.live || slot.generation != handle.generation || !slot.srb)
    {
        return {};
    }

    return {
        slot.srb.RawPtr(),
        slot.textureVariable,
    };
}

void DiligentGraphicsRuntime::ReleaseMaterialBindings() noexcept
{
    if (!m_native)
    {
        return;
    }

    for (auto& slot : m_native->materialBindings)
    {
        slot.srb.Release();
        slot.albedoVariable = nullptr;
        slot.shadowVariable = nullptr;
        slot.textureVariable = nullptr;
        slot.live = false;
    }
}

std::uint32_t DiligentGraphicsRuntime::FrameSlotCount() const noexcept
{
    return m_native && m_native->initialized
        ? m_native->frameSlots.FrameSlotCount()
        : static_cast<std::uint32_t>(m_slotGenerations.size());
}

DiligentFrameUploadArena& DiligentGraphicsRuntime::Uploads() noexcept
{
    return m_uploads;
}

const DiligentFrameUploadArena& DiligentGraphicsRuntime::Uploads() const noexcept
{
    return m_uploads;
}

} // namespace SagaEngine::Graphics::Backends::Diligent::Runtime
