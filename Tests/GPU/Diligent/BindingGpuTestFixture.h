#pragma once

#include "SagaGraphicsGpuTestFixture.h"

class BindingGPU : public SagaGraphicsGPU
{
protected:
    using DiligentBackend =
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend;

    [[nodiscard]] static constexpr std::array<std::uint8_t, 16> SolidTexture(
        std::uint8_t r,
        std::uint8_t g,
        std::uint8_t b,
        std::uint8_t a = 255u) noexcept
    {
        return {
            r, g, b, a,
            r, g, b, a,
            r, g, b, a,
            r, g, b, a};
    }

    [[nodiscard]] static SagaTests::Render::Rgba8 Red() noexcept;
    [[nodiscard]] static SagaTests::Render::Rgba8 Green() noexcept;
    [[nodiscard]] static SagaTests::Render::Rgba8 Blue() noexcept;
    [[nodiscard]] static SagaTests::Render::Rgba8 White() noexcept;
    [[nodiscard]] static std::uint32_t CountCenterColor(
        const SagaEngine::Render::Backend::RenderFrameCapture& capture,
        SagaTests::Render::Rgba8 expected,
        std::uint8_t tolerance = 12u);
    void ExpectCenterColor(
        const SagaEngine::Render::Backend::RenderFrameCapture& capture,
        SagaTests::Render::Rgba8 expected,
        std::uint8_t tolerance = 12u);
    [[nodiscard]] SagaEngine::Graphics::BufferHandle CreateNativeConstantBuffer(
        DiligentBackend& gfx,
        std::array<float, 4> color);
    [[nodiscard]] SagaEngine::Graphics::BufferHandle CreateNativeWrongUsageBuffer(
        DiligentBackend& gfx);
    [[nodiscard]] SagaEngine::Graphics::GraphicsBindingLayoutDesc MakeNativeConstantBindingLayout();
    [[nodiscard]] SagaEngine::Graphics::GraphicsBindingSetDesc MakeNativeConstantBindingSet(
        SagaEngine::Graphics::BindingLayoutHandle layout,
        SagaEngine::Graphics::BufferHandle buffer,
        std::uint64_t offset = 0u,
        std::uint64_t range = 0u);
    [[nodiscard]] SagaEngine::Graphics::ShaderHandle CreateNativeConstantShader(
        DiligentBackend& gfx,
        SagaEngine::Graphics::ShaderStage stage);
    [[nodiscard]] SagaEngine::Graphics::PipelineHandle CreateNativeConstantPipeline(
        DiligentBackend& gfx,
        SagaEngine::Graphics::BindingLayoutHandle layout);
    [[nodiscard]] SagaEngine::Render::Backend::RenderFrameCapture DrawResolvedTextured(
        DiligentBackend& gfx,
        SagaEngine::Graphics::PipelineHandle pipeline,
        SagaEngine::Graphics::BindingSetHandle bindingSet,
        bool* submitted = nullptr);
};
