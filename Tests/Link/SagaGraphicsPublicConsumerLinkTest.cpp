/// @file SagaGraphicsPublicConsumerLinkTest.cpp
/// @brief Standalone public SagaGraphics link consumer.

#include "SagaEngine/Graphics/Graphics.h"

#include <cstdint>

namespace
{

namespace Graphics = SagaEngine::Graphics;

int Fail(int code) noexcept
{
    return code;
}

Graphics::GraphicsHandle ToGraphicsHandle(
    const Graphics::GraphicsHandle& handle) noexcept
{
    return handle;
}

} // namespace

int main()
{
    Graphics::NullGraphicsBackend backend;

    Graphics::RenderBackendDesc backendDesc{};
    backendDesc.headless = true;
    if (!backend.Initialize(backendDesc, {}))
    {
        return Fail(1);
    }

    Graphics::BufferDesc bufferDesc{};
    bufferDesc.debugName = "public-link-buffer";
    bufferDesc.sizeBytes = 64u;
    bufferDesc.usage = Graphics::BufferUsage::Uniform;
    const auto buffer = backend.CreateBuffer(bufferDesc);
    if (!buffer.IsValid())
    {
        return Fail(2);
    }

    const auto bufferQuery = backend.QueryResource(
        Graphics::GraphicsResourceKind::Buffer,
        ToGraphicsHandle(buffer));
    if (!bufferQuery.live || bufferQuery.debugName != bufferDesc.debugName)
    {
        return Fail(3);
    }

    Graphics::GraphicsBindingLayoutDesc layout{};
    layout.debugName = "public-link-layout";
    layout.slots.push_back({
        0u,
        10u,
        Graphics::GraphicsBindingType::ConstantBuffer,
        Graphics::kGraphicsShaderStageVertex |
            Graphics::kGraphicsShaderStageFragment,
        1u,
        Graphics::GraphicsBindingFrequency::Frame,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
        Graphics::GraphicsBindingFallbackPolicy::None,
    });

    const auto key = Graphics::ComputeGraphicsBindingLayoutKey(layout);
    if (key == 0u)
    {
        return Fail(4);
    }

    const auto bindingLayout = backend.CreateBindingLayout(layout);
    if (!bindingLayout.IsValid())
    {
        return Fail(5);
    }

    const auto bindingQuery = backend.QueryBindingLayout(bindingLayout);
    if (!bindingQuery.live || bindingQuery.compatibilityKey != key)
    {
        return Fail(6);
    }

    backend.DestroyBindingLayout(bindingLayout);
    backend.DestroyBuffer(buffer);
    if (backend.QueryResource(
            Graphics::GraphicsResourceKind::Buffer,
            ToGraphicsHandle(buffer))
            .live)
    {
        return Fail(7);
    }

    backend.Shutdown();
    if (backend.GetStatus().initialized)
    {
        return Fail(8);
    }

    return 0;
}
