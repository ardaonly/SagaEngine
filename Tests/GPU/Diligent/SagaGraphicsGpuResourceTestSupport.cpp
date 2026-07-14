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

std::unique_ptr<
SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend>
SagaGraphicsGPU::CreateNativeGraphicsBackend()
{
    auto backend = std::make_unique<
        SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend>(
            m_Backend.RuntimeForIntegrationTesting());

    SagaEngine::Graphics::RenderBackendDesc desc{};
#if defined(__linux__)
    desc.preferredBackend =
        SagaEngine::Graphics::BackendPreference::NativePortable;
#endif

    SagaEngine::Graphics::SwapchainDesc swapchain{};
    swapchain.nativeWindow = GetNativeHandle(m_Window);
    swapchain.width = kWidth;
    swapchain.height = kHeight;
    swapchain.vsync = false;
    EXPECT_TRUE(backend->Initialize(desc, swapchain));
    EXPECT_TRUE(backend->HasNativeDeviceServicesForTesting());
    return backend;
}

SagaEngine::Graphics::GraphicsDataView SagaGraphicsGPU::DataView(
    const void* data,
    std::uint64_t sizeBytes) noexcept
{
    return {data, sizeBytes, 0u, 0u};
}

SagaEngine::Graphics::BufferHandle SagaGraphicsGPU::CreateNativeVertexBuffer(
    SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
    const std::array<NativeVertex, 3>& vertices)
{
    SagaEngine::Graphics::BufferDesc desc{};
    desc.debugName = "SagaGraphicsGPUVertexBuffer";
    desc.sizeBytes = sizeof(vertices);
    desc.usage = SagaEngine::Graphics::BufferUsage::Vertex;
    return gfx.CreateBuffer(desc, DataView(vertices.data(), sizeof(vertices)));
}

SagaEngine::Graphics::BufferHandle SagaGraphicsGPU::CreateNativeIndexBuffer(
    SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
    const std::array<std::uint32_t, 3>& indices)
{
    SagaEngine::Graphics::BufferDesc desc{};
    desc.debugName = "SagaGraphicsGPUIndexBuffer";
    desc.sizeBytes = sizeof(indices);
    desc.usage = SagaEngine::Graphics::BufferUsage::Index;
    return gfx.CreateBuffer(desc, DataView(indices.data(), sizeof(indices)));
}

SagaEngine::Graphics::BufferHandle SagaGraphicsGPU::CreateNativeTexturedVertexBuffer(
    SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
    const std::array<NativeTexturedVertex, 4>& vertices)
{
    SagaEngine::Graphics::BufferDesc desc{};
    desc.debugName = "SagaGraphicsGPUTexturedVertexBuffer";
    desc.sizeBytes = sizeof(vertices);
    desc.usage = SagaEngine::Graphics::BufferUsage::Vertex;
    return gfx.CreateBuffer(desc, DataView(vertices.data(), sizeof(vertices)));
}

SagaEngine::Graphics::BufferHandle SagaGraphicsGPU::CreateNativeQuadIndexBuffer(
    SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx)
{
    static constexpr std::array<std::uint32_t, 6> kIndices{
        0u, 1u, 2u, 0u, 2u, 3u};
    SagaEngine::Graphics::BufferDesc desc{};
    desc.debugName = "SagaGraphicsGPUQuadIndexBuffer";
    desc.sizeBytes = sizeof(kIndices);
    desc.usage = SagaEngine::Graphics::BufferUsage::Index;
    return gfx.CreateBuffer(desc, DataView(kIndices.data(), sizeof(kIndices)));
}

SagaEngine::Graphics::TextureHandle SagaGraphicsGPU::CreateNativeTexture(
    SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
    const std::array<std::uint8_t, 16>& rgba,
    SagaEngine::Graphics::ResourceFormat format)
{
    SagaEngine::Graphics::TextureDesc desc{};
    desc.debugName = "SagaGraphicsGPUTexture";
    desc.width = 2u;
    desc.height = 2u;
    desc.format = format;
    desc.usage = SagaEngine::Graphics::TextureUsageFlags::Sampled;
    SagaEngine::Graphics::GraphicsDataView data{
        rgba.data(),
        static_cast<std::uint64_t>(rgba.size()),
        8u,
        0u};
    return gfx.CreateTexture(desc, data);
}

SagaEngine::Graphics::SamplerHandle SagaGraphicsGPU::CreateNativeSampler(
    SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx,
    bool point,
    bool wrap)
{
    SagaEngine::Graphics::SamplerDesc desc{};
    desc.debugName = point ? "SagaGraphicsGPUPointSampler"
                           : "SagaGraphicsGPULinearSampler";
    desc.minFilter = point ? SagaEngine::Graphics::FilterMode::Nearest
                           : SagaEngine::Graphics::FilterMode::Linear;
    desc.magFilter = desc.minFilter;
    desc.addressU = wrap ? SagaEngine::Graphics::AddressMode::Repeat
                         : SagaEngine::Graphics::AddressMode::ClampToEdge;
    desc.addressV = desc.addressU;
    desc.addressW = SagaEngine::Graphics::AddressMode::ClampToEdge;
    return gfx.CreateSampler(desc);
}

SagaEngine::Graphics::ShaderHandle SagaGraphicsGPU::CreateNativeTexturedShader(
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
Texture2D g_Tex;
SamplerState g_Tex_sampler;

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 Uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return g_Tex.Sample(g_Tex_sampler, input.Uv);
}
)";

    SagaEngine::Graphics::ShaderDesc desc{};
    desc.debugName = stage == SagaEngine::Graphics::ShaderStage::Vertex
                         ? "SagaGraphicsGPUTexturedVS"
                         : "SagaGraphicsGPUTexturedPS";
    desc.stage = stage;
    desc.entryPoint = "main";
    desc.sourceIdentity = desc.debugName;
    desc.source = stage == SagaEngine::Graphics::ShaderStage::Vertex
                      ? kVS
                      : kPS;
    return gfx.CreateShader(desc);
}

SagaEngine::Graphics::PipelineHandle SagaGraphicsGPU::CreateNativeTexturedPipeline(
    SagaEngine::Graphics::Backends::Diligent::DiligentGraphicsBackend& gfx)
{
    auto vs = CreateNativeTexturedShader(
        gfx,
        SagaEngine::Graphics::ShaderStage::Vertex);
    auto ps = CreateNativeTexturedShader(
        gfx,
        SagaEngine::Graphics::ShaderStage::Fragment);
    if (!vs.IsValid() || !ps.IsValid())
    {
        return {};
    }

    const auto services = m_Backend.RuntimeForIntegrationTesting().Services();
    const auto& scDesc = services.SwapChain()->GetDesc();

    SagaEngine::Graphics::PipelineDesc desc{};
    desc.debugName = "SagaGraphicsGPUTexturedPipeline";
    desc.vertexShader = vs;
    desc.fragmentShader = ps;
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

Diligent::RefCntAutoPtr<Diligent::IPipelineState>
SagaGraphicsGPU::CreateTestPipeline(Diligent::IRenderDevice& device,
                   Diligent::ISwapChain& swapChain)
{
    using namespace Diligent;

    static constexpr const char* kVS = R"(
struct VSInput
{
    float3 Pos : ATTRIB0;
    float4 Color : ATTRIB1;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.Pos = float4(input.Pos, 1.0);
    output.Color = input.Color;
    return output;
}
)";

    static constexpr const char* kPS = R"(
struct PSInput
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR0;
};

float4 main(PSInput input) : SV_Target
{
    return input.Color;
}
)";

    ShaderCreateInfo shaderInfo;
    shaderInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderInfo.Desc.UseCombinedTextureSamplers = True;
    shaderInfo.EntryPoint = "main";

    RefCntAutoPtr<IShader> vs;
    shaderInfo.Desc.Name = "SagaGraphicsBufferProofVS";
    shaderInfo.Desc.ShaderType = SHADER_TYPE_VERTEX;
    shaderInfo.Source = kVS;
    device.CreateShader(shaderInfo, &vs);
    if (!vs)
    {
        return {};
    }

    RefCntAutoPtr<IShader> ps;
    shaderInfo.Desc.Name = "SagaGraphicsBufferProofPS";
    shaderInfo.Desc.ShaderType = SHADER_TYPE_PIXEL;
    shaderInfo.Source = kPS;
    device.CreateShader(shaderInfo, &ps);
    if (!ps)
    {
        return {};
    }

    const auto& scDesc = swapChain.GetDesc();
    GraphicsPipelineStateCreateInfo psoInfo;
    psoInfo.PSODesc.Name = "SagaGraphicsBufferProofPSO";
    psoInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    psoInfo.pVS = vs;
    psoInfo.pPS = ps;
    psoInfo.GraphicsPipeline.NumRenderTargets = 1u;
    psoInfo.GraphicsPipeline.RTVFormats[0] = scDesc.ColorBufferFormat;
    psoInfo.GraphicsPipeline.DSVFormat = scDesc.DepthBufferFormat;
    psoInfo.GraphicsPipeline.PrimitiveTopology =
        PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    psoInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    psoInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;

    LayoutElement layout[] = {
        {0, 0, 3, VT_FLOAT32, False,
         static_cast<Uint32>(offsetof(NativeVertex, position))},
        {1, 0, 4, VT_FLOAT32, False,
         static_cast<Uint32>(offsetof(NativeVertex, color))},
    };
    psoInfo.GraphicsPipeline.InputLayout.LayoutElements = layout;
    psoInfo.GraphicsPipeline.InputLayout.NumElements = 2u;

    RefCntAutoPtr<IPipelineState> pso;
    device.CreateGraphicsPipelineState(psoInfo, &pso);
    return pso;
}

RenderFrameCapture SagaGraphicsGPU::DrawNativeIndexedTriangle(
    Diligent::IBuffer* vertexBuffer,
    Diligent::IBuffer* indexBuffer,
    bool* submitted)
{
    if (submitted)
    {
        *submitted = false;
    }

    RenderFrameCapture capture{};
    const auto services = m_Backend.RuntimeForIntegrationTesting().Services();
    EXPECT_NE(services.Device(), nullptr);
    EXPECT_NE(services.ImmediateContext(), nullptr);
    EXPECT_NE(services.SwapChain(), nullptr);
    if (!services.Device() || !services.ImmediateContext() ||
        !services.SwapChain())
    {
        return capture;
    }

    auto pso = CreateTestPipeline(*services.Device(), *services.SwapChain());
    EXPECT_NE(pso.RawPtr(), nullptr);

    m_Backend.BeginFrame();
    if (vertexBuffer && indexBuffer && pso)
    {
        auto* ctx = services.ImmediateContext();
        ctx->SetPipelineState(pso);
        Diligent::IBuffer* vbs[] = {vertexBuffer};
        Diligent::Uint64 offsets[] = {0u};
        ctx->SetVertexBuffers(
            0u,
            1u,
            vbs,
            offsets,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        ctx->SetIndexBuffer(
            indexBuffer,
            0u,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::DrawIndexedAttribs drawAttribs;
        drawAttribs.NumIndices = 3u;
        drawAttribs.IndexType = Diligent::VT_UINT32;
        drawAttribs.Flags = Diligent::DRAW_FLAG_NONE;
        ctx->DrawIndexed(drawAttribs);
        if (submitted)
        {
            *submitted = true;
        }
    }

    const auto result = m_Backend.CaptureCurrentColorFrame(capture);
    m_Backend.EndFrame();
    EXPECT_EQ(result, RenderCaptureResult::kSuccess);
    return capture;
}

std::array<SagaGraphicsGPU::NativeVertex, 3> SagaGraphicsGPU::TriangleVertices()
{
    return {{
        {{-0.65f, -0.65f, 0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{0.65f, -0.65f, 0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{0.0f, 0.65f, 0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    }};
}

std::array<std::uint32_t, 3> SagaGraphicsGPU::TriangleIndices()
{
    return {{0u, 1u, 2u}};
}

std::array<SagaGraphicsGPU::NativeTexturedVertex, 4> SagaGraphicsGPU::TexturedQuadVertices(
    float maxUv)
{
    return {{
        {{-0.75f, -0.75f, 0.5f}, {0.0f, maxUv}},
        {{0.75f, -0.75f, 0.5f}, {maxUv, maxUv}},
        {{0.75f, 0.75f, 0.5f}, {maxUv, 0.0f}},
        {{-0.75f, 0.75f, 0.5f}, {0.0f, 0.0f}},
    }};
}

RenderFrameCapture SagaGraphicsGPU::DrawNativeTexturedQuad(
    Diligent::IBuffer* vertexBuffer,
    Diligent::IBuffer* indexBuffer,
    Diligent::IPipelineState* pipeline,
    Diligent::ITextureView* textureSrv,
    Diligent::ISampler* sampler,
    bool* submitted)
{
    if (submitted)
    {
        *submitted = false;
    }

    RenderFrameCapture capture{};
    const auto services = m_Backend.RuntimeForIntegrationTesting().Services();
    EXPECT_NE(services.ImmediateContext(), nullptr);
    if (!services.ImmediateContext())
    {
        return capture;
    }

    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
    if (pipeline)
    {
        pipeline->CreateShaderResourceBinding(&srb, true);
    }
    if (srb)
    {
        if (auto* var = srb->GetVariableByName(
                Diligent::SHADER_TYPE_PIXEL,
                "g_Tex"))
        {
            var->Set(textureSrv);
        }
        if (auto* var = srb->GetVariableByName(
                Diligent::SHADER_TYPE_PIXEL,
                "g_Tex_sampler"))
        {
            var->Set(sampler);
        }
    }

    m_Backend.BeginFrame();
    if (vertexBuffer && indexBuffer && pipeline && textureSrv && sampler && srb)
    {
        auto* ctx = services.ImmediateContext();
        ctx->SetPipelineState(pipeline);
        ctx->CommitShaderResources(
            srb,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::IBuffer* vbs[] = {vertexBuffer};
        Diligent::Uint64 offsets[] = {0u};
        ctx->SetVertexBuffers(
            0u,
            1u,
            vbs,
            offsets,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        ctx->SetIndexBuffer(
            indexBuffer,
            0u,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::DrawIndexedAttribs drawAttribs;
        drawAttribs.NumIndices = 6u;
        drawAttribs.IndexType = Diligent::VT_UINT32;
        drawAttribs.Flags = Diligent::DRAW_FLAG_NONE;
        ctx->DrawIndexed(drawAttribs);
        if (submitted)
        {
            *submitted = true;
        }
    }

    const auto result = m_Backend.CaptureCurrentColorFrame(capture);
    m_Backend.EndFrame();
    EXPECT_EQ(result, RenderCaptureResult::kSuccess);
    return capture;
}

RenderFrameCapture SagaGraphicsGPU::DrawNativeBoundTexturedQuad(
    Diligent::IBuffer* vertexBuffer,
    Diligent::IBuffer* indexBuffer,
    Diligent::IPipelineState* pipeline,
    Diligent::IShaderResourceBinding* srb,
    bool* submitted)
{
    if (submitted)
    {
        *submitted = false;
    }

    RenderFrameCapture capture{};
    const auto services = m_Backend.RuntimeForIntegrationTesting().Services();
    EXPECT_NE(services.ImmediateContext(), nullptr);
    if (!services.ImmediateContext())
    {
        return capture;
    }

    m_Backend.BeginFrame();
    if (vertexBuffer && indexBuffer && pipeline && srb)
    {
        auto* ctx = services.ImmediateContext();
        ctx->SetPipelineState(pipeline);
        ctx->CommitShaderResources(
            srb,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::IBuffer* vbs[] = {vertexBuffer};
        Diligent::Uint64 offsets[] = {0u};
        ctx->SetVertexBuffers(
            0u,
            1u,
            vbs,
            offsets,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        ctx->SetIndexBuffer(
            indexBuffer,
            0u,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::DrawIndexedAttribs drawAttribs;
        drawAttribs.NumIndices = 6u;
        drawAttribs.IndexType = Diligent::VT_UINT32;
        drawAttribs.Flags = Diligent::DRAW_FLAG_NONE;
        ctx->DrawIndexed(drawAttribs);
        if (submitted)
        {
            *submitted = true;
        }
    }

    const auto result = m_Backend.CaptureCurrentColorFrame(capture);
    m_Backend.EndFrame();
    EXPECT_EQ(result, RenderCaptureResult::kSuccess);
    return capture;
}
