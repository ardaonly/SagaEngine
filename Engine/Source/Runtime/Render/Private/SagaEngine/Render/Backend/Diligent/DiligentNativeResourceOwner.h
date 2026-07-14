/// @file DiligentNativeResourceOwner.h
/// @brief Private Saga-owned Diligent resource factory and quarantine owner.

#pragma once

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentDeviceServices.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceToken.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Diligent
{
struct IBuffer;
struct ITexture;
struct ITextureView;
struct ISampler;
struct IShader;
struct IPipelineState;
} // namespace Diligent

namespace SagaEngine::Render::Backend
{

class DiligentNativeResourceOwner final
{
public:
    DiligentNativeResourceOwner();
    ~DiligentNativeResourceOwner();

    DiligentNativeResourceOwner(const DiligentNativeResourceOwner&) = delete;
    DiligentNativeResourceOwner& operator=(
        const DiligentNativeResourceOwner&) = delete;

    void Bind(DiligentDeviceServices services) noexcept;
    void ReleaseAll() noexcept;

    [[nodiscard]] bool CanCreateNative() const noexcept;

    [[nodiscard]] static bool IsBufferUsageSupported(
        Graphics::BufferUsage usage) noexcept;
    [[nodiscard]] static bool IsBufferSizeSupported(
        std::uint64_t sizeBytes) noexcept;

    [[nodiscard]] DiligentNativeResourceToken AllocateToken(
        DiligentNativeResourceKind kind) noexcept;

    [[nodiscard]] std::uint64_t CreateBufferForToken(
        DiligentNativeBufferToken token,
        const Graphics::BufferDesc& desc,
        Graphics::GraphicsDataView initialData,
        std::string* diagnostic = nullptr);
    [[nodiscard]] std::uint64_t CreateTextureForToken(
        DiligentNativeTextureToken token,
        const Graphics::TextureDesc& desc,
        Graphics::GraphicsDataView initialData,
        std::string* diagnostic = nullptr);
    [[nodiscard]] std::uint64_t CreateSamplerForToken(
        DiligentNativeSamplerToken token,
        const Graphics::SamplerDesc& desc,
        std::string* diagnostic = nullptr);
    [[nodiscard]] std::uint64_t CreateShaderForToken(
        DiligentNativeShaderToken token,
        const Graphics::ShaderDesc& desc,
        std::string* diagnostic = nullptr);
    [[nodiscard]] std::uint64_t CreatePipelineForToken(
        DiligentNativePipelineToken token,
        const Graphics::PipelineDesc& desc,
        std::string* diagnostic = nullptr);
    [[nodiscard]] std::uint64_t CreatePipelineForToken(
        DiligentNativePipelineToken token,
        DiligentNativeShaderToken vertexShader,
        DiligentNativeShaderToken fragmentShader,
        const Graphics::PipelineDesc& desc,
        std::string* diagnostic = nullptr);

    [[nodiscard]] std::uint64_t CreateBufferForHandle(
        Graphics::BufferHandle handle,
        const Graphics::BufferDesc& desc,
        Graphics::GraphicsDataView initialData,
        std::string* diagnostic = nullptr);
    [[nodiscard]] std::uint64_t CreateTextureForHandle(
        Graphics::TextureHandle handle,
        const Graphics::TextureDesc& desc,
        Graphics::GraphicsDataView initialData,
        std::string* diagnostic = nullptr);
    [[nodiscard]] std::uint64_t CreateSamplerForHandle(
        Graphics::SamplerHandle handle,
        const Graphics::SamplerDesc& desc,
        std::string* diagnostic = nullptr);
    [[nodiscard]] std::uint64_t CreateShaderForHandle(
        Graphics::ShaderHandle handle,
        const Graphics::ShaderDesc& desc,
        std::string* diagnostic = nullptr);
    [[nodiscard]] std::uint64_t CreatePipelineForHandle(
        Graphics::PipelineHandle handle,
        const Graphics::PipelineDesc& desc,
        std::string* diagnostic = nullptr);

    [[nodiscard]] Graphics::BufferHandle CreateStandaloneBuffer(
        const Graphics::BufferDesc& desc,
        Graphics::GraphicsDataView initialData,
        std::string* diagnostic = nullptr);
    [[nodiscard]] Graphics::TextureHandle CreateStandaloneTexture(
        const Graphics::TextureDesc& desc,
        Graphics::GraphicsDataView initialData,
        std::string* diagnostic = nullptr);

    void DestroyBuffer(DiligentNativeBufferToken token) noexcept;
    void DestroyTexture(DiligentNativeTextureToken token) noexcept;
    void DestroySampler(DiligentNativeSamplerToken token) noexcept;
    void DestroyShader(DiligentNativeShaderToken token) noexcept;
    void DestroyPipeline(DiligentNativePipelineToken token) noexcept;

    void DestroyBuffer(Graphics::BufferHandle handle) noexcept;
    void DestroyTexture(Graphics::TextureHandle handle) noexcept;
    void DestroySampler(Graphics::SamplerHandle handle) noexcept;
    void DestroyShader(Graphics::ShaderHandle handle) noexcept;
    void DestroyPipeline(Graphics::PipelineHandle handle) noexcept;

    void MarkBufferUsed(DiligentNativeBufferToken token,
                        std::uint64_t serial) noexcept;
    void MarkTextureUsed(DiligentNativeTextureToken token,
                         std::uint64_t serial) noexcept;
    void MarkSamplerUsed(DiligentNativeSamplerToken token,
                         std::uint64_t serial) noexcept;
    void MarkShaderUsed(DiligentNativeShaderToken token,
                        std::uint64_t serial) noexcept;
    void MarkPipelineUsed(DiligentNativePipelineToken token,
                          std::uint64_t serial) noexcept;

    void MarkBufferUsed(Graphics::BufferHandle handle,
                        std::uint64_t serial) noexcept;
    void MarkTextureUsed(Graphics::TextureHandle handle,
                         std::uint64_t serial) noexcept;
    void MarkSamplerUsed(Graphics::SamplerHandle handle,
                         std::uint64_t serial) noexcept;
    void MarkShaderUsed(Graphics::ShaderHandle handle,
                        std::uint64_t serial) noexcept;
    void MarkPipelineUsed(Graphics::PipelineHandle handle,
                          std::uint64_t serial) noexcept;
    void RetireCompleted(std::uint64_t completedSerial) noexcept;

    [[nodiscard]] Diligent::IBuffer* ResolveBuffer(
        Graphics::BufferHandle handle) const noexcept;
    [[nodiscard]] Diligent::ITexture* ResolveTexture(
        Graphics::TextureHandle handle) const noexcept;
    [[nodiscard]] Diligent::ITextureView* ResolveTextureSrv(
        Graphics::TextureHandle handle) const noexcept;
    [[nodiscard]] Diligent::ISampler* ResolveSampler(
        Graphics::SamplerHandle handle) const noexcept;
    [[nodiscard]] Diligent::IShader* ResolveShader(
        Graphics::ShaderHandle handle) const noexcept;
    [[nodiscard]] Diligent::IPipelineState* ResolvePipeline(
        Graphics::PipelineHandle handle) const noexcept;

    [[nodiscard]] Diligent::IBuffer* ResolveBuffer(
        DiligentNativeBufferToken token) const noexcept;
    [[nodiscard]] Diligent::ITexture* ResolveTexture(
        DiligentNativeTextureToken token) const noexcept;
    [[nodiscard]] Diligent::ITextureView* ResolveTextureSrv(
        DiligentNativeTextureToken token) const noexcept;
    [[nodiscard]] Diligent::ISampler* ResolveSampler(
        DiligentNativeSamplerToken token) const noexcept;
    [[nodiscard]] Diligent::IShader* ResolveShader(
        DiligentNativeShaderToken token) const noexcept;
    [[nodiscard]] Diligent::IPipelineState* ResolvePipeline(
        DiligentNativePipelineToken token) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace SagaEngine::Render::Backend
