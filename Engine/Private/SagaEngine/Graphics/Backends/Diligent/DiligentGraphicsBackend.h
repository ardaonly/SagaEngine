/// @file DiligentGraphicsBackend.h
/// @brief Private SagaGraphics lifecycle adapter over the render backend.

#pragma once

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingRecords.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsResourceRegistry.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentDeviceServices.h"
#include "SagaEngine/Render/Backend/RenderBackendFactory.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace SagaEngine::Render::Backend
{
class DiligentNativeResourceOwner;
}

namespace Diligent
{
struct IBuffer;
struct ITextureView;
struct ISampler;
struct IShader;
struct IShaderResourceBinding;
struct IPipelineState;
}

namespace SagaEngine::Graphics::Backends::Diligent
{

namespace RenderBackend = ::SagaEngine::Render::Backend;

class DiligentBindingCache;
class DiligentFallbackResources;
struct DiligentBindingCacheResolveResult;
struct DiligentResolvedBindingSet;

class DiligentGraphicsBackend final : public IGraphicsBackend
{
private:
    using NativeResourceOwnership = Detail::NativeResourceOwnership;

    template <typename HandleT, typename DescT>
    using ResourceRegistry = Detail::ResourceRegistry<HandleT, DescT>;

public:
    using StatusReader = RenderBackend::RenderBackendStatus (*)(
        const RenderBackend::IRenderBackend&) noexcept;
    using BackendFactory = std::unique_ptr<RenderBackend::IRenderBackend> (*)(
        const RenderBackendDesc&);

    DiligentGraphicsBackend();
    explicit DiligentGraphicsBackend(
        std::unique_ptr<RenderBackend::IRenderBackend> backend,
        StatusReader statusReader = RenderBackend::GetRenderBackendStatus);
    explicit DiligentGraphicsBackend(
        BackendFactory backendFactory,
        StatusReader statusReader = RenderBackend::GetRenderBackendStatus);
    ~DiligentGraphicsBackend() override;

    [[nodiscard]] bool Initialize(
        const RenderBackendDesc& backend,
        const SwapchainDesc& swapchain) override;
    void Shutdown() override;
    void Resize(std::uint32_t width, std::uint32_t height) override;

    [[nodiscard]] TextureHandle CreateTexture(const TextureDesc& desc) override;
    [[nodiscard]] TextureHandle CreateTexture(
        const TextureDesc& desc,
        GraphicsDataView initialData) override;
    [[nodiscard]] BufferHandle CreateBuffer(const BufferDesc& desc) override;
    [[nodiscard]] BufferHandle CreateBuffer(
        const BufferDesc& desc,
        GraphicsDataView initialData) override;
    [[nodiscard]] ShaderHandle CreateShader(const ShaderDesc& desc) override;
    [[nodiscard]] PipelineHandle CreatePipeline(const PipelineDesc& desc) override;
    [[nodiscard]] SamplerHandle CreateSampler(const SamplerDesc& desc) override;
    [[nodiscard]] BindingLayoutHandle CreateBindingLayout(
        const GraphicsBindingLayoutDesc& desc) override;
    [[nodiscard]] BindingSetHandle CreateBindingSet(
        const GraphicsBindingSetDesc& desc) override;

    void DestroyTexture(TextureHandle handle) override;
    void DestroyBuffer(BufferHandle handle) override;
    void DestroyShader(ShaderHandle handle) override;
    void DestroyPipeline(PipelineHandle handle) override;
    void DestroySampler(SamplerHandle handle) override;
    void DestroyBindingLayout(BindingLayoutHandle handle) override;
    void DestroyBindingSet(BindingSetHandle handle) override;

    void BeginFrame() override;
    void EndFrame() override;

    [[nodiscard]] RenderBackendStatus GetStatus() const noexcept override;
    [[nodiscard]] RenderBackendCapabilities
    GetCapabilities() const noexcept override;
    [[nodiscard]] GraphicsResourceMemoryReport GetResourceMemoryReport()
        const noexcept override;
    [[nodiscard]] GraphicsResourceFailure GetLastResourceFailure()
        const noexcept override;
    [[nodiscard]] GraphicsResourceLeakSummary
    GetLastShutdownResourceLeakSummary() const noexcept override;
    [[nodiscard]] GraphicsResourceQueryResult QueryResource(
        GraphicsResourceKind kind,
        GraphicsHandle handle) const override;
    [[nodiscard]] GraphicsBindingLayoutQueryResult QueryBindingLayout(
        BindingLayoutHandle handle) const override;
    [[nodiscard]] GraphicsBindingSetQueryResult QueryBindingSet(
        BindingSetHandle handle) const override;
    [[nodiscard]] std::uint64_t GetTextureShadowBytesForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t GetBufferShadowBytesForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t GetTextureNativeSerialForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t GetBufferNativeSerialForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] std::uint64_t GetSamplerNativeSerialForTesting(
        SamplerHandle handle) const noexcept;
    [[nodiscard]] bool GetTextureNativeUploadDeferredForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] bool GetBufferNativeUploadDeferredForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceBacking GetTextureBackingForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceBacking GetBufferBackingForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceQueryResult QueryTextureForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceQueryResult QueryBufferForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceQueryResult QueryShaderForTesting(
        ShaderHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceQueryResult QueryPipelineForTesting(
        PipelineHandle handle) const noexcept;
    [[nodiscard]] GraphicsResourceQueryResult QuerySamplerForTesting(
        SamplerHandle handle) const noexcept;
    [[nodiscard]] bool HasNativeDeviceServicesForTesting() const noexcept;
    void BindNativeDeviceServicesForTesting(
        RenderBackend::DiligentDeviceServices services,
        bool enableNativeCreation = false) noexcept;
    [[nodiscard]] bool MarkBufferNativeForTesting(
        BufferHandle handle,
        std::uint64_t nativeSerial) noexcept;
    [[nodiscard]] bool MarkTextureNativeForTesting(
        TextureHandle handle,
        std::uint64_t nativeSerial) noexcept;
    [[nodiscard]] bool MarkSamplerNativeForTesting(
        SamplerHandle handle,
        std::uint64_t nativeSerial) noexcept;
    [[nodiscard]] bool MarkShaderNativeForTesting(
        ShaderHandle handle,
        std::uint64_t nativeSerial) noexcept;
    [[nodiscard]] bool MarkPipelineNativeForTesting(
        PipelineHandle handle,
        std::uint64_t nativeSerial) noexcept;
    [[nodiscard]] ::Diligent::IBuffer* ResolveNativeBufferForTesting(
        BufferHandle handle) const noexcept;
    [[nodiscard]] ::Diligent::ITextureView* ResolveNativeTextureSrvForTesting(
        TextureHandle handle) const noexcept;
    [[nodiscard]] ::Diligent::ISampler* ResolveNativeSamplerForTesting(
        SamplerHandle handle) const noexcept;
    [[nodiscard]] ::Diligent::IShader* ResolveNativeShaderForTesting(
        ShaderHandle handle) const noexcept;
    [[nodiscard]] ::Diligent::IPipelineState* ResolveNativePipelineForTesting(
        PipelineHandle handle) const noexcept;
    [[nodiscard]] std::uint32_t GetCompiledBindingLayoutCountForTesting()
        const noexcept;
    [[nodiscard]] const DiligentCompiledBindingLayout*
    ResolveCompiledBindingLayoutForTesting(
        BindingLayoutHandle handle) const noexcept;
    [[nodiscard]] std::uint32_t GetNativeBindingSetRecordCountForTesting()
        const noexcept;
    [[nodiscard]] const DiligentNativeBindingSetRecord*
    ResolveNativeBindingSetRecordForTesting(
        BindingSetHandle handle) const noexcept;
    [[nodiscard]] bool CorruptNativeBindingSetCanonicalLayoutForTesting(
        BindingSetHandle handle) noexcept;
    [[nodiscard]] ::Diligent::IShaderResourceBinding*
    ResolveNativeBindingSrbForTesting(
        PipelineHandle pipeline,
        BindingSetHandle bindingSet) noexcept;
    [[nodiscard]] DiligentNativeBindingDiagnostics
    GetNativeBindingDiagnosticsForTesting() const noexcept;
    [[nodiscard]] std::uint64_t GetNativeBindingCacheEntryCountForTesting()
        const noexcept;
    [[nodiscard]] std::uint64_t GetNativeBindingQuarantinedSrbCountForTesting()
        const noexcept;
    [[nodiscard]] TextureHandle GetFallbackWhiteTextureForTesting()
        const noexcept;
    [[nodiscard]] SamplerHandle GetFallbackMaterialSamplerForTesting()
        const noexcept;
    [[nodiscard]] std::uint64_t GetFallbackGenerationForTesting()
        const noexcept;
    [[nodiscard]] bool InitializeFallbackResourcesForTesting() noexcept;
    void ReleaseFallbackResourcesForTesting() noexcept;
    void ForceNextFallbackSamplerFailureForTesting(bool enabled) noexcept;

private:
    [[nodiscard]] RenderBackendCapabilities MakeConservativeCapabilities()
        const noexcept;
    [[nodiscard]] RenderBackendCapabilities MakeReadyCapabilities(
        BackendPreference backend) const noexcept;
    [[nodiscard]] bool CanRenderFrame() const noexcept;
    [[nodiscard]] bool CanCreateResources() const noexcept;
    [[nodiscard]] GraphicsResourceBacking ResourceBackingForNativeResource()
        const noexcept;
    [[nodiscard]] GraphicsResourceLifecycle ResourceLifecycleForBacking(
        GraphicsResourceBacking backing) const noexcept;
    [[nodiscard]] NativeResourceOwnership MakeNativeResourceOwnership(
        bool uploadDeferred) noexcept;
    template <typename HandleT>
    [[nodiscard]] HandleT RecordCreateFailure(
        GraphicsResourceFailure failure) noexcept;
    template <typename HandleT>
    [[nodiscard]] HandleT RecordSuccessfulCreate(HandleT handle) noexcept;
    [[nodiscard]] GraphicsResourceMemoryReport BuildResourceMemoryReport()
        const noexcept;
    [[nodiscard]] static GraphicsResourceLeakSummary
    BuildLeakSummary(const GraphicsResourceMemoryReport& report) noexcept;
    [[nodiscard]] static std::uint64_t PackHandleKey(
        std::uint32_t index,
        std::uint32_t generation) noexcept;
    void SetFailure(RenderBackendFailure failure) noexcept;
    void SetFrameSkipped(RenderBackendFailure failure) noexcept;
    void ReleaseResources() noexcept;
    [[nodiscard]] DiligentResolvedBindingSet ResolveNativeBindingSet(
        PipelineHandle pipeline,
        BindingSetHandle bindingSet) noexcept;
    [[nodiscard]] DiligentBindingCacheResolveResult ResolveNativeBindingSrb(
        PipelineHandle pipeline,
        BindingSetHandle bindingSet) noexcept;

    std::unique_ptr<RenderBackend::IRenderBackend> m_RenderBackend;
    BackendFactory m_BackendFactory = nullptr;
    StatusReader m_StatusReader = RenderBackend::GetRenderBackendStatus;
    RenderBackendStatus m_HeadlessStatus{};
    RenderBackendStatus m_LastStatus{};
    RenderBackendCapabilities m_LastCapabilities{};
    RenderBackend::DiligentDeviceServices m_DeviceServices{};
    std::unique_ptr<RenderBackend::DiligentNativeResourceOwner> m_NativeOwner;
    bool m_NativeCreationEnabled = false;
    bool m_Headless = false;
    bool m_SurfaceMinimized = false;
    GraphicsResourceFailure m_LastResourceFailure =
        GraphicsResourceFailure::None;
    GraphicsResourceLeakSummary m_LastShutdownLeakSummary{};
    std::uint64_t m_PeakLiveBytes = 0;
    std::uint64_t m_FailedCreateCount = 0;
    std::uint64_t m_NextNativeResourceSerial = 1;
    std::uint64_t m_NextResourceCreationSerial = 1;
    ResourceRegistry<TextureHandle, TextureDesc> m_Textures{1u};
    ResourceRegistry<BufferHandle, BufferDesc> m_Buffers{1001u};
    ResourceRegistry<ShaderHandle, ShaderDesc> m_Shaders{2001u};
    ResourceRegistry<PipelineHandle, PipelineDesc> m_Pipelines{3001u};
    ResourceRegistry<SamplerHandle, SamplerDesc> m_Samplers{4001u};
    ResourceRegistry<BindingLayoutHandle, GraphicsBindingLayoutDesc>
        m_BindingLayouts{5001u};
    ResourceRegistry<BindingSetHandle, GraphicsBindingSetDesc>
        m_BindingSets{6001u};
    std::unordered_map<std::uint64_t, DiligentCompiledBindingLayout>
        m_CompiledBindingLayouts;
    std::unordered_map<std::uint64_t, DiligentNativeBindingSetRecord>
        m_NativeBindingSets;
    std::unique_ptr<DiligentBindingCache> m_NativeBindingCache;
    std::unique_ptr<DiligentFallbackResources> m_FallbackResources;
    DiligentNativeBindingDiagnostics m_NativeBindingDiagnostics{};
};

[[nodiscard]] std::unique_ptr<IGraphicsBackend> CreateDiligentGraphicsBackend();
[[nodiscard]] std::unique_ptr<IGraphicsBackend>
CreateDiligentGraphicsBackendForTesting(
    std::unique_ptr<RenderBackend::IRenderBackend> backend,
    DiligentGraphicsBackend::StatusReader statusReader =
        RenderBackend::GetRenderBackendStatus);
[[nodiscard]] std::unique_ptr<IGraphicsBackend>
CreateDiligentGraphicsBackendForTesting(
    DiligentGraphicsBackend::BackendFactory backendFactory,
    DiligentGraphicsBackend::StatusReader statusReader =
        RenderBackend::GetRenderBackendStatus);

} // namespace SagaEngine::Graphics::Backends::Diligent
