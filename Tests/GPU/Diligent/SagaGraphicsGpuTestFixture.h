#pragma once

#include "DiligentGpuTestFixture.h"

#include "RefCntAutoPtr.hpp"
#include "Buffer.h"
#include "PipelineState.h"
#include "RenderDevice.h"
#include "ShaderResourceBinding.h"
#include "SwapChain.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

class SagaGraphicsGPU : public DiligentGPU
{
protected:
    struct NativeVertex
    {
        float position[3];
        float color[4];
    };

    struct NativeTexturedVertex
    {
        float position[3];
        float uv[2];
    };

    [[nodiscard]] std::unique_ptr<
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend>
    CreateNativeGraphicsBackend();

    [[nodiscard]] static SagaEngine::Graphics::GraphicsDataView DataView(
        const void* data,
        std::uint64_t sizeBytes) noexcept;

    [[nodiscard]] SagaEngine::Graphics::BufferHandle CreateNativeVertexBuffer(
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
        const std::array<NativeVertex, 3>& vertices);
    [[nodiscard]] SagaEngine::Graphics::BufferHandle CreateNativeIndexBuffer(
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
        const std::array<std::uint32_t, 3>& indices);
    [[nodiscard]] SagaEngine::Graphics::BufferHandle CreateNativeTexturedVertexBuffer(
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
        const std::array<NativeTexturedVertex, 4>& vertices);
    [[nodiscard]] SagaEngine::Graphics::BufferHandle CreateNativeQuadIndexBuffer(
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx);
    [[nodiscard]] SagaEngine::Graphics::TextureHandle CreateNativeTexture(
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
        const std::array<std::uint8_t, 16>& rgba,
        SagaEngine::Graphics::ResourceFormat format =
            SagaEngine::Graphics::ResourceFormat::Rgba8Unorm);
    [[nodiscard]] SagaEngine::Graphics::SamplerHandle CreateNativeSampler(
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
        bool point,
        bool wrap);
    [[nodiscard]] SagaEngine::Graphics::ShaderHandle CreateNativeTexturedShader(
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
        SagaEngine::Graphics::ShaderStage stage);
    [[nodiscard]] SagaEngine::Graphics::PipelineHandle CreateNativeTexturedPipeline(
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx);

    [[nodiscard]] SagaEngine::Graphics::GraphicsBindingLayoutDesc MakeNativeAlbedoBindingLayout();
    [[nodiscard]] SagaEngine::Graphics::GraphicsBindingLayoutDesc
    MakeNativeOptionalFallbackAlbedoBindingLayout(bool textureRequired, bool samplerRequired);
    [[nodiscard]] SagaEngine::Graphics::GraphicsBindingSetDesc MakeNativeAlbedoBindingSet(
        SagaEngine::Graphics::BindingLayoutHandle layout,
        SagaEngine::Graphics::TextureHandle texture,
        SagaEngine::Graphics::SamplerHandle sampler);
    [[nodiscard]] SagaEngine::Graphics::GraphicsBindingSetDesc MakeNativeAlbedoSamplerOnlyBindingSet(
        SagaEngine::Graphics::BindingLayoutHandle layout,
        SagaEngine::Graphics::SamplerHandle sampler);
    [[nodiscard]] SagaEngine::Graphics::GraphicsBindingSetDesc MakeNativeAlbedoEmptyBindingSet(
        SagaEngine::Graphics::BindingLayoutHandle layout);
    [[nodiscard]] SagaEngine::Graphics::ShaderHandle CreateNativeAlbedoShader(
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
        SagaEngine::Graphics::ShaderStage stage);
    [[nodiscard]] SagaEngine::Graphics::PipelineHandle CreateNativeAlbedoPipeline(
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
        SagaEngine::Graphics::BindingLayoutHandle layout);

    [[nodiscard]] Diligent::RefCntAutoPtr<Diligent::IPipelineState> CreateTestPipeline(
        Diligent::IRenderDevice& device,
        Diligent::ISwapChain& swapChain);
    [[nodiscard]] SagaEngine::Render::Backend::RenderFrameCapture DrawNativeIndexedTriangle(
        Diligent::IBuffer* vertexBuffer,
        Diligent::IBuffer* indexBuffer,
        bool* submitted);
    [[nodiscard]] static std::array<NativeVertex, 3> TriangleVertices();
    [[nodiscard]] static std::array<std::uint32_t, 3> TriangleIndices();
    [[nodiscard]] static std::array<NativeTexturedVertex, 4> TexturedQuadVertices(float maxUv = 1.0f);
    [[nodiscard]] SagaEngine::Render::Backend::RenderFrameCapture DrawNativeTexturedQuad(
        Diligent::IBuffer* vertexBuffer,
        Diligent::IBuffer* indexBuffer,
        Diligent::IPipelineState* pipeline,
        Diligent::ITextureView* textureSrv,
        Diligent::ISampler* sampler,
        bool* submitted);
    [[nodiscard]] SagaEngine::Render::Backend::RenderFrameCapture DrawNativeBoundTexturedQuad(
        Diligent::IBuffer* vertexBuffer,
        Diligent::IBuffer* indexBuffer,
        Diligent::IPipelineState* pipeline,
        Diligent::IShaderResourceBinding* srb,
        bool* submitted);
};
