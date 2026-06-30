#include <SagaEngine/Graphics/Graphics.h>

#include <cstdint>

namespace Graphics = SagaEngine::Graphics;

Graphics::GraphicsHandle ToGraphicsHandle(
    const Graphics::GraphicsHandle& handle) noexcept
{
    return handle;
}

int main()
{
    Graphics::NullGraphicsBackend backend;

    Graphics::RenderBackendDesc backendDesc{};
    backendDesc.headless = true;
    if (!backend.Initialize(backendDesc, {}))
    {
        return 1;
    }

    Graphics::BufferDesc bufferDesc{};
    bufferDesc.sizeBytes = 128u;
    bufferDesc.usage = Graphics::BufferUsage::Uniform;
    const auto buffer = backend.CreateBuffer(bufferDesc);
    if (!buffer.IsValid())
    {
        return 2;
    }

    const auto bufferQuery = backend.QueryResource(
        Graphics::GraphicsResourceKind::Buffer,
        ToGraphicsHandle(buffer));
    if (!bufferQuery.live ||
        bufferQuery.kind != Graphics::GraphicsResourceKind::Buffer)
    {
        return 3;
    }

    Graphics::GraphicsBindingLayoutDesc layoutDesc{};
    layoutDesc.slots.push_back({
        0u,
        100u,
        Graphics::GraphicsBindingType::ConstantBuffer,
        Graphics::kGraphicsShaderStageVertex,
        1u,
        Graphics::GraphicsBindingFrequency::Frame,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
        Graphics::GraphicsBindingFallbackPolicy::None,
    });

    const auto key = Graphics::ComputeGraphicsBindingLayoutKey(layoutDesc);
    if (key == 0u)
    {
        return 4;
    }

    const auto layout = backend.CreateBindingLayout(layoutDesc);
    if (!layout.IsValid())
    {
        return 5;
    }

    const auto layoutQuery = backend.QueryBindingLayout(layout);
    if (!layoutQuery.live || layoutQuery.compatibilityKey != key ||
        !Graphics::AreGraphicsBindingLayoutsCompatible(
            layoutDesc,
            layoutQuery.canonicalLayout))
    {
        return 6;
    }

    backend.DestroyBindingLayout(layout);
    backend.DestroyBuffer(buffer);
    if (backend.QueryResource(
            Graphics::GraphicsResourceKind::Buffer,
            ToGraphicsHandle(buffer))
            .live)
    {
        return 7;
    }

    backend.Shutdown();
    if (backend.GetStatus().initialized)
    {
        return 8;
    }

    return 0;
}
