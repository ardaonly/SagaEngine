/// @file DiligentGraphicsRuntime.h
/// @brief Private Diligent runtime frame, upload, and command vocabulary.

#pragma once

#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentDeviceServices.h"
#include "SagaEngine/Render/Backend/RenderBackendFactory.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Graphics::Backends::Diligent
{
class DiligentBindingCache;
class DiligentFallbackResources;
struct DiligentBindingCacheResolveResult;
struct DiligentNativeBindingDiagnostics;
struct DiligentResolvedBindingSet;

} // namespace SagaEngine::Graphics::Backends::Diligent

namespace Diligent
{
struct IBuffer;
struct IDeviceContext;
struct IPipelineState;
struct IRenderDevice;
struct IShader;
struct IShaderResourceBinding;
struct IShaderResourceVariable;
struct ISwapChain;
} // namespace Diligent

namespace SagaEngine::Render::Backend
{
class DiligentFrameSlotTracker;
class DiligentGpuTimeline;
class DiligentNativeResourceOwner;
} // namespace SagaEngine::Render::Backend

namespace SagaEngine::Graphics::Backends::Diligent::Runtime
{

namespace RenderBackend = ::SagaEngine::Render::Backend;

struct FrameToken
{
    std::uint64_t frameIndex = 0;
    std::uint32_t frameSlot = 0;
    std::uint32_t frameSlotGeneration = 0;
    std::uint64_t submissionSerial = 0;
    std::uint64_t completedSerial = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return submissionSerial != 0u;
    }
};

struct ConstantSlice
{
    FrameToken token{};
    std::uint32_t chunkIndex = 0;
    std::uint64_t byteOffset = 0;
    std::uint64_t byteSize = 0;
    std::uint64_t alignment = 0;
    std::byte* cpu = nullptr;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return token.IsValid() && cpu != nullptr && byteSize != 0u;
    }
};

struct UploadArenaDiagnostics
{
    std::uint32_t uniformOverflowCount = 0;
    std::uint32_t transientOverflowCount = 0;
    std::uint32_t secondaryChunkCount = 0;
    std::uint64_t largestUniformAllocation = 0;
    std::uint64_t largestTransientAllocation = 0;
};

class DiligentFrameUploadArena final
{
public:
    DiligentFrameUploadArena() = default;
    DiligentFrameUploadArena(
        std::uint32_t frameSlotCount,
        std::uint64_t uniformChunkBytes,
        std::uint64_t transientChunkBytes,
        std::uint64_t constantBufferAlignment);

    void Configure(
        std::uint32_t frameSlotCount,
        std::uint64_t uniformChunkBytes,
        std::uint64_t transientChunkBytes,
        std::uint64_t constantBufferAlignment);
    void BeginFrame(FrameToken token);
    [[nodiscard]] ConstantSlice AllocateUniform(std::uint64_t byteSize);
    [[nodiscard]] ConstantSlice AllocateTransient(std::uint64_t byteSize);

    [[nodiscard]] UploadArenaDiagnostics Diagnostics() const noexcept;
    [[nodiscard]] std::uint64_t ConstantBufferAlignment() const noexcept;

private:
    struct Chunk
    {
        std::vector<std::byte> bytes;
        std::uint64_t cursor = 0;
    };

    struct Slot
    {
        std::uint32_t generation = 0;
        std::vector<Chunk> uniformChunks;
        std::vector<Chunk> transientChunks;
    };

    [[nodiscard]] ConstantSlice Allocate(
        std::vector<Chunk>& chunks,
        std::uint64_t primaryChunkBytes,
        std::uint64_t byteSize,
        bool uniform);
    [[nodiscard]] static std::uint64_t AlignUp(
        std::uint64_t value,
        std::uint64_t alignment) noexcept;

    std::vector<Slot> m_slots;
    std::uint64_t m_uniformChunkBytes = 0;
    std::uint64_t m_transientChunkBytes = 0;
    std::uint64_t m_constantBufferAlignment = 256;
    FrameToken m_activeToken{};
    UploadArenaDiagnostics m_diagnostics{};
};

enum class GraphicsCommandError : std::uint8_t
{
    None,
    FrameNotOpen,
    RenderPassNotOpen,
    RenderPassAlreadyOpen,
    Submitted,
};

struct GraphicsCommandDiagnostics
{
    GraphicsCommandError lastError = GraphicsCommandError::None;
    std::uint32_t rejectedCommandCount = 0;
    std::uint32_t drawCount = 0;
    std::uint32_t indexedDrawCount = 0;
    std::uint32_t passCount = 0;
    std::uint32_t submitCount = 0;
    std::uint32_t presentCount = 0;
};

struct GraphicsViewport
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

struct GraphicsScissor
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct NativeShaderBindingHandle
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return index != 0u && generation != 0u;
    }
};

struct NativeShaderBindingView
{
    ::Diligent::IShaderResourceBinding* srb = nullptr;
    ::Diligent::IShaderResourceVariable* albedoVariable = nullptr;
    ::Diligent::IShaderResourceVariable* shadowVariable = nullptr;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return srb != nullptr;
    }
};

struct NativeOverlayBindingView
{
    ::Diligent::IShaderResourceBinding* srb = nullptr;
    ::Diligent::IShaderResourceVariable* textureVariable = nullptr;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return srb != nullptr && textureVariable != nullptr;
    }
};

class GraphicsCommandEncoder;

class GraphicsRenderPassEncoder final
{
public:
    explicit GraphicsRenderPassEncoder(GraphicsCommandEncoder& encoder);

    [[nodiscard]] bool ClearColor(TextureHandle target, const float rgba[4]);
    [[nodiscard]] bool ClearDepth(TextureHandle target, float depth);
    [[nodiscard]] bool SetPipeline(PipelineHandle pipeline);
    [[nodiscard]] bool SetBindingSet(BindingSetHandle bindingSet);
    [[nodiscard]] bool SetVertexBuffer(BufferHandle buffer);
    [[nodiscard]] bool SetIndexBuffer(BufferHandle buffer);
    [[nodiscard]] bool SetViewport(GraphicsViewport viewport);
    [[nodiscard]] bool SetScissor(GraphicsScissor scissor);
    [[nodiscard]] bool Draw(std::uint32_t vertexCount);
    [[nodiscard]] bool DrawIndexed(std::uint32_t indexCount);
    [[nodiscard]] bool EndRenderPass();

private:
    GraphicsCommandEncoder* m_encoder = nullptr;
};

class GraphicsCommandEncoder final
{
public:
    explicit GraphicsCommandEncoder(FrameToken token);

    [[nodiscard]] GraphicsRenderPassEncoder BeginRenderPass();
    [[nodiscard]] bool TransitionResource(GraphicsHandle resource);
    [[nodiscard]] bool ClearColor(TextureHandle target, const float rgba[4]);
    [[nodiscard]] bool ClearDepth(TextureHandle target, float depth);
    [[nodiscard]] bool SetPipeline(PipelineHandle pipeline);
    [[nodiscard]] bool SetBindingSet(BindingSetHandle bindingSet);
    [[nodiscard]] bool SetVertexBuffer(BufferHandle buffer);
    [[nodiscard]] bool SetIndexBuffer(BufferHandle buffer);
    [[nodiscard]] bool SetViewport(GraphicsViewport viewport);
    [[nodiscard]] bool SetScissor(GraphicsScissor scissor);
    [[nodiscard]] bool Draw(std::uint32_t vertexCount);
    [[nodiscard]] bool DrawIndexed(std::uint32_t indexCount);
    [[nodiscard]] bool EndRenderPass();
    [[nodiscard]] bool Submit();
    [[nodiscard]] bool Present();

    [[nodiscard]] const FrameToken& Token() const noexcept;
    [[nodiscard]] GraphicsCommandDiagnostics Diagnostics() const noexcept;

private:
    [[nodiscard]] bool Reject(GraphicsCommandError error) noexcept;
    [[nodiscard]] bool RequireFrameOpen() noexcept;
    [[nodiscard]] bool RequirePassOpen() noexcept;

    FrameToken m_token{};
    bool m_passOpen = false;
    bool m_submitted = false;
    GraphicsCommandDiagnostics m_diagnostics{};
};

struct GraphicsFrameContext
{
    FrameToken token{};
    DiligentFrameUploadArena* uploads = nullptr;
    std::unique_ptr<GraphicsCommandEncoder> commands;
};

class DiligentGraphicsRuntime final
{
public:
    explicit DiligentGraphicsRuntime(std::uint32_t frameSlotCount = 3);
    ~DiligentGraphicsRuntime();

    DiligentGraphicsRuntime(const DiligentGraphicsRuntime&) = delete;
    DiligentGraphicsRuntime& operator=(const DiligentGraphicsRuntime&) =
        delete;
    DiligentGraphicsRuntime(DiligentGraphicsRuntime&&) = delete;
    DiligentGraphicsRuntime& operator=(DiligentGraphicsRuntime&&) = delete;

    [[nodiscard]] bool Initialize(
        const RenderBackend::RenderBackendConfig& config,
        const RenderBackend::SwapchainDesc& swapchain);
    void Shutdown();
    void Resize(std::uint32_t width, std::uint32_t height);
    void PresentActiveFrame();

    [[nodiscard]] GraphicsFrameContext BeginFrame(
        std::uint64_t completedSerial);
    void Submit(GraphicsCommandEncoder& encoder);
    void Present(GraphicsCommandEncoder& encoder);
    [[nodiscard]] std::uint64_t DeferredReleaseSerial() const noexcept;
    void RetireCompleted(std::uint64_t completedSerial) noexcept;

    [[nodiscard]] const FrameToken& LastFrameToken() const noexcept;
    [[nodiscard]] RenderBackend::RenderBackendStatus Status() const noexcept;
    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] ::Diligent::IRenderDevice* Device() const noexcept;
    [[nodiscard]] ::Diligent::IDeviceContext* Context() const noexcept;
    [[nodiscard]] ::Diligent::ISwapChain* SwapChain() const noexcept;
    [[nodiscard]] RenderBackend::DiligentDeviceServices Services()
        const noexcept;
    [[nodiscard]] RenderBackend::DiligentNativeResourceOwner&
    NativeResources() noexcept;
    [[nodiscard]] const RenderBackend::DiligentNativeResourceOwner&
    NativeResources() const noexcept;
    [[nodiscard]] RenderBackend::DiligentGpuTimeline& Timeline() noexcept;
    [[nodiscard]] const RenderBackend::DiligentGpuTimeline& Timeline()
        const noexcept;
    [[nodiscard]] RenderBackend::DiligentFrameSlotTracker& FrameSlots()
        noexcept;
    [[nodiscard]] const RenderBackend::DiligentFrameSlotTracker& FrameSlots()
        const noexcept;
    [[nodiscard]] DiligentBindingCache& BindingCache() noexcept;
    [[nodiscard]] const DiligentBindingCache& BindingCache() const noexcept;
    [[nodiscard]] DiligentFallbackResources& FallbackResources() noexcept;
    [[nodiscard]] const DiligentFallbackResources& FallbackResources()
        const noexcept;
    [[nodiscard]] NativeShaderBindingHandle CreateMaterialBinding(
        ::Diligent::IPipelineState& pipeline);
    [[nodiscard]] NativeShaderBindingHandle CreateOverlayBinding(
        ::Diligent::IPipelineState& pipeline);
    void DestroyMaterialBinding(NativeShaderBindingHandle handle) noexcept;
    [[nodiscard]] NativeShaderBindingView ResolveMaterialBinding(
        NativeShaderBindingHandle handle) noexcept;
    [[nodiscard]] NativeOverlayBindingView ResolveOverlayBinding(
        NativeShaderBindingHandle handle) noexcept;
    void ReleaseMaterialBindings() noexcept;

    [[nodiscard]] std::uint32_t FrameSlotCount() const noexcept;
    [[nodiscard]] DiligentFrameUploadArena& Uploads() noexcept;
    [[nodiscard]] const DiligentFrameUploadArena& Uploads() const noexcept;

private:
    struct NativeState;
    std::unique_ptr<NativeState> m_native;
    std::vector<std::uint32_t> m_slotGenerations;
    FrameToken m_lastToken{};
    DiligentFrameUploadArena m_uploads;
};

} // namespace SagaEngine::Graphics::Backends::Diligent::Runtime
