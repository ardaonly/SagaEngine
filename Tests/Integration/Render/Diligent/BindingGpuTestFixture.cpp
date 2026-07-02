#include "BindingGpuTestFixture.h"

#if defined(None)
#   undef None
#endif
#if defined(Bool)
#   undef Bool
#endif
#if defined(True)
#   undef True
#endif
#if defined(False)
#   undef False
#endif

#include "DeviceContext.h"
#include "Shader.h"

#include <utility>

using namespace SagaEngine::Render::Backend;

SagaTests::Render::Rgba8 BindingGPU::Red() noexcept
{
    return {255u, 0u, 0u, 255u};
}

SagaTests::Render::Rgba8 BindingGPU::Green() noexcept
{
    return {0u, 255u, 0u, 255u};
}

SagaTests::Render::Rgba8 BindingGPU::Blue() noexcept
{
    return {0u, 0u, 255u, 255u};
}

SagaTests::Render::Rgba8 BindingGPU::White() noexcept
{
    return {255u, 255u, 255u, 255u};
}

std::uint32_t BindingGPU::CountCenterColor(
    const RenderFrameCapture& capture,
    SagaTests::Render::Rgba8 expected,
    std::uint8_t tolerance)
{
    return SagaTests::Render::CountRegionMatching(
        capture,
        capture.width / 2u,
        capture.height / 2u,
        24u,
        [expected, tolerance](SagaTests::Render::Rgba8 c)
        {
            return SagaTests::Render::ColorNear(c, expected, tolerance);
        });
}

void BindingGPU::ExpectCenterColor(
    const RenderFrameCapture& capture,
    SagaTests::Render::Rgba8 expected,
    std::uint8_t tolerance)
{
    EXPECT_GT(CountCenterColor(capture, expected, tolerance), 100u);
}

SagaEngine::Graphics::BufferHandle BindingGPU::CreateNativeConstantBuffer(
    DiligentBackend& gfx,
    std::array<float, 4> color)
{
    SagaEngine::Graphics::BufferDesc desc{};
    desc.debugName = "BindingGPUConstantBuffer";
    desc.sizeBytes = sizeof(color);
    desc.usage = SagaEngine::Graphics::BufferUsage::Uniform;
    return gfx.CreateBuffer(desc, DataView(color.data(), sizeof(color)));
}

SagaEngine::Graphics::BufferHandle BindingGPU::CreateNativeWrongUsageBuffer(
    DiligentBackend& gfx)
{
    std::array<float, 4> payload{1.0f, 0.0f, 0.0f, 1.0f};
    SagaEngine::Graphics::BufferDesc desc{};
    desc.debugName = "BindingGPUWrongUsageBuffer";
    desc.sizeBytes = sizeof(payload);
    desc.usage = SagaEngine::Graphics::BufferUsage::Vertex;
    return gfx.CreateBuffer(desc, DataView(payload.data(), sizeof(payload)));
}

SagaEngine::Graphics::GraphicsBindingLayoutDesc
BindingGPU::MakeNativeConstantBindingLayout()
{
    SagaEngine::Graphics::GraphicsBindingLayoutDesc layout{};
    layout.debugName = "BindingGPUConstantLayout";
    SagaEngine::Graphics::GraphicsBindingLayoutSlot cb{};
    cb.slot = 0u;
    cb.stableId = 300u;
    cb.type = SagaEngine::Graphics::GraphicsBindingType::ConstantBuffer;
    cb.stages = SagaEngine::Graphics::kGraphicsShaderStageFragment;
    cb.frequency = SagaEngine::Graphics::GraphicsBindingFrequency::Material;
    cb.required = true;
    layout.slots.push_back(cb);
    return layout;
}

SagaEngine::Graphics::GraphicsBindingSetDesc
BindingGPU::MakeNativeConstantBindingSet(
    SagaEngine::Graphics::BindingLayoutHandle layout,
    SagaEngine::Graphics::BufferHandle buffer,
    std::uint64_t offset,
    std::uint64_t range)
{
    SagaEngine::Graphics::GraphicsBindingSetDesc desc{};
    desc.debugName = "BindingGPUConstantSet";
    desc.layout = layout;
    SagaEngine::Graphics::GraphicsBindingResourceRef ref{};
    ref.slot = 0u;
    ref.stableId = 300u;
    ref.kind = SagaEngine::Graphics::GraphicsResourceKind::Buffer;
    ref.handle = buffer;
    ref.bufferOffsetBytes = offset;
    ref.bufferRangeBytes = range;
    desc.resources.push_back(ref);
    return desc;
}

SagaEngine::Graphics::ShaderHandle BindingGPU::CreateNativeConstantShader(
    DiligentBackend& gfx,
    SagaEngine::Graphics::ShaderStage stage)
{
    static constexpr const char* kVS = R"(
struct VSInput
{
    float3 Pos : ATTRIB0;
    float2 Uv : ATTRIB1;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.Pos = float4(input.Pos, 1.0);
    return output;
}
)";

    static constexpr const char* kPS = R"(
cbuffer CameraCB
{
    float4 g_BindingGpuColor;
};

float4 main() : SV_Target
{
    return g_BindingGpuColor;
}
)";

    SagaEngine::Graphics::ShaderDesc desc{};
    desc.debugName = stage == SagaEngine::Graphics::ShaderStage::Vertex
                         ? "BindingGPUConstantVS"
                         : "BindingGPUConstantPS";
    desc.stage = stage;
    desc.entryPoint = "main";
    desc.sourceIdentity = desc.debugName;
    desc.source = stage == SagaEngine::Graphics::ShaderStage::Vertex
                      ? kVS
                      : kPS;
    return gfx.CreateShader(desc);
}

SagaEngine::Graphics::PipelineHandle
BindingGPU::CreateNativeConstantPipeline(
    DiligentBackend& gfx,
    SagaEngine::Graphics::BindingLayoutHandle layout)
{
    auto vs = CreateNativeConstantShader(
        gfx,
        SagaEngine::Graphics::ShaderStage::Vertex);
    auto ps = CreateNativeConstantShader(
        gfx,
        SagaEngine::Graphics::ShaderStage::Fragment);
    if (!vs.IsValid() || !ps.IsValid())
    {
        return {};
    }

    const auto services = m_Backend.GetDiligentDeviceServices();
    const auto& scDesc = services.SwapChain()->GetDesc();

    SagaEngine::Graphics::PipelineDesc desc{};
    desc.debugName = "BindingGPUConstantPipeline";
    desc.vertexShader = vs;
    desc.fragmentShader = ps;
    desc.bindingLayout = layout;
    desc.colorFormat = scDesc.ColorBufferFormat == Diligent::TEX_FORMAT_BGRA8_UNORM
        ? SagaEngine::Graphics::ResourceFormat::Bgra8Unorm
        : SagaEngine::Graphics::ResourceFormat::Rgba8Unorm;
    desc.depthFormat = scDesc.DepthBufferFormat == Diligent::TEX_FORMAT_D32_FLOAT
        ? SagaEngine::Graphics::ResourceFormat::Depth32Float
        : SagaEngine::Graphics::ResourceFormat::Depth24Stencil8;
    desc.depthTest = false;
    desc.depthWrite = false;
    desc.cullBackFaces = false;
    desc.vertexLayout.push_back({
        0u,
        0u,
        static_cast<std::uint32_t>(offsetof(NativeTexturedVertex, position)),
        SagaEngine::Graphics::VertexElementFormat::Float32x3});
    desc.vertexLayout.push_back({
        1u,
        0u,
        static_cast<std::uint32_t>(offsetof(NativeTexturedVertex, uv)),
        SagaEngine::Graphics::VertexElementFormat::Float32x2});
    return gfx.CreatePipeline(desc);
}

RenderFrameCapture BindingGPU::DrawResolvedTextured(
    DiligentBackend& gfx,
    SagaEngine::Graphics::PipelineHandle pipeline,
    SagaEngine::Graphics::BindingSetHandle bindingSet,
    bool* submitted)
{
    const auto vertices = TexturedQuadVertices();
    const auto vertexBuffer = CreateNativeTexturedVertexBuffer(gfx, vertices);
    const auto indexBuffer = CreateNativeQuadIndexBuffer(gfx);
    auto* srb = gfx.ResolveNativeBindingSrbForTesting(pipeline, bindingSet);
    return DrawNativeBoundTexturedQuad(
        gfx.ResolveNativeBufferForTesting(vertexBuffer),
        gfx.ResolveNativeBufferForTesting(indexBuffer),
        gfx.ResolveNativePipelineForTesting(pipeline),
        srb,
        submitted);
}
