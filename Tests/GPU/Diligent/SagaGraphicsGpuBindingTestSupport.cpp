#include "SagaGraphicsGpuTestFixture.h"

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

SagaEngine::Graphics::GraphicsBindingLayoutDesc
SagaGraphicsGPU::MakeNativeAlbedoBindingLayout()
{
    SagaEngine::Graphics::GraphicsBindingLayoutDesc layout{};
    layout.debugName = "SagaGraphicsGPUNativeAlbedoLayout";

    SagaEngine::Graphics::GraphicsBindingLayoutSlot texture{};
    texture.slot = 0u;
    texture.stableId = 100u;
    texture.type = SagaEngine::Graphics::GraphicsBindingType::SampledTexture;
    texture.stages = SagaEngine::Graphics::kGraphicsShaderStageFragment;
    texture.required = true;
    texture.pairedSamplerSlot = 1u;
    layout.slots.push_back(texture);

    SagaEngine::Graphics::GraphicsBindingLayoutSlot sampler{};
    sampler.slot = 1u;
    sampler.stableId = 101u;
    sampler.type = SagaEngine::Graphics::GraphicsBindingType::Sampler;
    sampler.stages = SagaEngine::Graphics::kGraphicsShaderStageFragment;
    sampler.required = true;
    layout.slots.push_back(sampler);

    return layout;
}

SagaEngine::Graphics::GraphicsBindingLayoutDesc
SagaGraphicsGPU::MakeNativeOptionalFallbackAlbedoBindingLayout(
    bool textureRequired,
    bool samplerRequired)
{
    auto layout = MakeNativeAlbedoBindingLayout();
    layout.debugName = "SagaGraphicsGPUNativeFallbackAlbedoLayout";
    layout.slots[0].required = textureRequired;
    layout.slots[0].fallbackPolicy =
        textureRequired
            ? SagaEngine::Graphics::GraphicsBindingFallbackPolicy::None
            : SagaEngine::Graphics::GraphicsBindingFallbackPolicy::
                  OptionalSampledTexture;
    layout.slots[1].required = samplerRequired;
    layout.slots[1].fallbackPolicy =
        samplerRequired
            ? SagaEngine::Graphics::GraphicsBindingFallbackPolicy::None
            : SagaEngine::Graphics::GraphicsBindingFallbackPolicy::
                  OptionalSampler;
    return layout;
}

SagaEngine::Graphics::GraphicsBindingSetDesc
SagaGraphicsGPU::MakeNativeAlbedoBindingSet(
    SagaEngine::Graphics::BindingLayoutHandle layout,
    SagaEngine::Graphics::TextureHandle texture,
    SagaEngine::Graphics::SamplerHandle sampler)
{
    SagaEngine::Graphics::GraphicsBindingSetDesc desc{};
    desc.debugName = "SagaGraphicsGPUNativeAlbedoSet";
    desc.layout = layout;

    SagaEngine::Graphics::GraphicsBindingResourceRef textureRef{};
    textureRef.slot = 0u;
    textureRef.stableId = 100u;
    textureRef.kind = SagaEngine::Graphics::GraphicsResourceKind::Texture;
    textureRef.handle = texture;
    desc.resources.push_back(textureRef);

    SagaEngine::Graphics::GraphicsBindingResourceRef samplerRef{};
    samplerRef.slot = 1u;
    samplerRef.stableId = 101u;
    samplerRef.kind = SagaEngine::Graphics::GraphicsResourceKind::Sampler;
    samplerRef.handle = sampler;
    desc.resources.push_back(samplerRef);

    return desc;
}

SagaEngine::Graphics::GraphicsBindingSetDesc
SagaGraphicsGPU::MakeNativeAlbedoSamplerOnlyBindingSet(
    SagaEngine::Graphics::BindingLayoutHandle layout,
    SagaEngine::Graphics::SamplerHandle sampler)
{
    SagaEngine::Graphics::GraphicsBindingSetDesc desc{};
    desc.debugName = "SagaGraphicsGPUNativeFallbackSamplerOnlySet";
    desc.layout = layout;

    SagaEngine::Graphics::GraphicsBindingResourceRef samplerRef{};
    samplerRef.slot = 1u;
    samplerRef.stableId = 101u;
    samplerRef.kind = SagaEngine::Graphics::GraphicsResourceKind::Sampler;
    samplerRef.handle = sampler;
    desc.resources.push_back(samplerRef);
    return desc;
}

SagaEngine::Graphics::GraphicsBindingSetDesc
SagaGraphicsGPU::MakeNativeAlbedoEmptyBindingSet(
    SagaEngine::Graphics::BindingLayoutHandle layout)
{
    SagaEngine::Graphics::GraphicsBindingSetDesc desc{};
    desc.debugName = "SagaGraphicsGPUNativeFallbackEmptySet";
    desc.layout = layout;
    return desc;
}

SagaEngine::Graphics::ShaderHandle SagaGraphicsGPU::CreateNativeAlbedoShader(
    SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
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
    float2 Uv : TEXCOORD0;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.Pos = float4(input.Pos, 1.0);
    output.Uv = input.Uv;
    return output;
}
)";

    static constexpr const char* kPS = R"(
Texture2D g_Albedo;
SamplerState g_Albedo_sampler;

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 Uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return g_Albedo.Sample(g_Albedo_sampler, input.Uv);
}
)";

    SagaEngine::Graphics::ShaderDesc desc{};
    desc.debugName = stage == SagaEngine::Graphics::ShaderStage::Vertex
                         ? "SagaGraphicsGPUNativeBindingVS"
                         : "SagaGraphicsGPUNativeBindingPS";
    desc.stage = stage;
    desc.entryPoint = "main";
    desc.sourceIdentity = desc.debugName;
    desc.source = stage == SagaEngine::Graphics::ShaderStage::Vertex
                      ? kVS
                      : kPS;
    return gfx.CreateShader(desc);
}

SagaEngine::Graphics::PipelineHandle SagaGraphicsGPU::CreateNativeAlbedoPipeline(
    SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
    SagaEngine::Graphics::BindingLayoutHandle layout)
{
    auto vs = CreateNativeAlbedoShader(
        gfx,
        SagaEngine::Graphics::ShaderStage::Vertex);
    auto ps = CreateNativeAlbedoShader(
        gfx,
        SagaEngine::Graphics::ShaderStage::Fragment);
    if (!vs.IsValid() || !ps.IsValid())
    {
        return {};
    }

    const auto services = m_Backend.RuntimeForIntegrationTesting().Services();
    const auto& scDesc = services.SwapChain()->GetDesc();

    SagaEngine::Graphics::PipelineDesc desc{};
    desc.debugName = "SagaGraphicsGPUNativeBindingPipeline";
    desc.vertexShader = vs;
    desc.fragmentShader = ps;
    desc.bindingLayout = layout;
    desc.colorFormat = scDesc.ColorBufferFormat == Diligent::TEX_FORMAT_BGRA8_UNORM ||
        scDesc.ColorBufferFormat == Diligent::TEX_FORMAT_BGRA8_UNORM_SRGB
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
