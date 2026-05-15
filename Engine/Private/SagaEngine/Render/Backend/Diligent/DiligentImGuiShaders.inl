/// @file DiligentImGuiShaders.inl
/// @brief Embedded HLSL shader source for ImGui rendering through Diligent.
///
/// Included from DiligentRenderBackend.cpp inside an anonymous namespace.
/// Not a standalone translation unit.

static constexpr const char* kImGuiVS = R"(
cbuffer ImGuiCB : register(b0)
{
    float4x4 ProjectionMatrix;
};

struct VS_INPUT
{
    float2 pos : ATTRIB0;
    float2 uv  : ATTRIB1;
    float4 col : ATTRIB2;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

void main(in VS_INPUT input, out PS_INPUT output)
{
    output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.0, 1.0));
    output.col = input.col;
    output.uv  = input.uv;
}
)";

static constexpr const char* kImGuiPS = R"(
Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

float4 main(in PS_INPUT input) : SV_Target
{
    return input.col * g_Texture.Sample(g_Sampler, input.uv);
}
)";
