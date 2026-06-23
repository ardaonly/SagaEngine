/// @file DiligentNativeResourceOwner.h
/// @brief Private Saga-owned Diligent resource factory and quarantine owner.

#pragma once

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentDeviceServices.h"

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

    void DestroyBuffer(Graphics::BufferHandle handle) noexcept;
    void DestroyTexture(Graphics::TextureHandle handle) noexcept;
    void DestroySampler(Graphics::SamplerHandle handle) noexcept;
    void DestroyShader(Graphics::ShaderHandle handle) noexcept;
    void DestroyPipeline(Graphics::PipelineHandle handle) noexcept;

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

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace SagaEngine::Render::Backend
