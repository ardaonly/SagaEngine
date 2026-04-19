/// @file DiligentShaders.inl
/// @brief Embedded HLSL shader source for the solid-color pipeline, included from DiligentRenderBackend.cpp.
///
/// MeshVertex layout (76 bytes):
///   pos(3f) normal(3f) tangent(3f) handedness(1f) uv0(2f) uv1(2f)
///   boneIndices(4u8) boneWeights(4f)
///
/// Two VS variants:
///   kSolidVS    — static geometry, ignores bone attributes.
///   kSkinnedVS  — applies up to 4-bone weighted skinning from g_BonePalette.
/// Both share the same PS (kSolidPS).

// ─── Vertex shader ──────────────────────────────────────────────────

static constexpr char kSolidVS[] = R"(
cbuffer CameraCB
{
    float4x4 g_ViewProj;
    float4x4 g_Model;
    float4   g_LightDir;    // xyz = world-space direction TO the light (normalised)
    float4   g_LightColor;  // xyz = light colour, w = ambient strength
};

struct VSInput
{
    float3 Pos        : ATTRIB0;
    float3 Normal     : ATTRIB1;
    float3 Tangent    : ATTRIB2;
    float  Handedness : ATTRIB3;
    float2 UV0        : ATTRIB4;
    float2 UV1        : ATTRIB5;
    uint4  BoneIdx    : ATTRIB6;   // Declared to match 76-byte vertex; unused.
    float4 BoneWgt    : ATTRIB7;
};

struct PSInput
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    // Column-major matrices: mul(matrix, vector) is the standard M * v.
    float4 worldPos = mul(g_Model, float4(VSIn.Pos, 1.0));
    PSIn.Pos    = mul(g_ViewProj, worldPos);
    PSIn.Normal = mul(g_Model, float4(VSIn.Normal, 0.0)).xyz;
    PSIn.UV     = VSIn.UV0;
}
)";

// ─── Skinned vertex shader ──────────────────────────────────────────

static constexpr char kSkinnedVS[] = R"(
cbuffer CameraCB
{
    float4x4 g_ViewProj;
    float4x4 g_Model;
    float4   g_LightDir;
    float4   g_LightColor;
};

cbuffer BoneCB
{
    float4x4 g_BonePalette[128];
};

struct VSInput
{
    float3 Pos        : ATTRIB0;
    float3 Normal     : ATTRIB1;
    float3 Tangent    : ATTRIB2;
    float  Handedness : ATTRIB3;
    float2 UV0        : ATTRIB4;
    float2 UV1        : ATTRIB5;
    uint4  BoneIdx    : ATTRIB6;
    float4 BoneWgt    : ATTRIB7;
};

struct PSInput
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    // Weighted bone skinning (up to 4 influences).
    float4x4 skin = (float4x4)0;
    skin += g_BonePalette[VSIn.BoneIdx.x] * VSIn.BoneWgt.x;
    skin += g_BonePalette[VSIn.BoneIdx.y] * VSIn.BoneWgt.y;
    skin += g_BonePalette[VSIn.BoneIdx.z] * VSIn.BoneWgt.z;
    skin += g_BonePalette[VSIn.BoneIdx.w] * VSIn.BoneWgt.w;

    float4 skinnedPos    = mul(skin, float4(VSIn.Pos, 1.0));
    float3 skinnedNormal = mul(skin, float4(VSIn.Normal, 0.0)).xyz;

    float4 worldPos = mul(g_Model, skinnedPos);
    PSIn.Pos    = mul(g_ViewProj, worldPos);
    PSIn.Normal = mul(g_Model, float4(skinnedNormal, 0.0)).xyz;
    PSIn.UV     = VSIn.UV0;
}
)";

// ─── Pixel shader ───────────────────────────────────────────────────

static constexpr char kSolidPS[] = R"(
cbuffer CameraCB
{
    float4x4 g_ViewProj;
    float4x4 g_Model;
    float4   g_LightDir;
    float4   g_LightColor;
};

Texture2D    g_Albedo  : register(t0);
SamplerState g_Sampler : register(s0);

struct PSInput
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
};

float4 main(in PSInput PSIn) : SV_Target
{
    float4 texColor = g_Albedo.Sample(g_Sampler, PSIn.UV);

    float3 N = normalize(PSIn.Normal);
    float3 L = normalize(g_LightDir.xyz);
    float  ndl = saturate(dot(N, L));

    float  ambient = g_LightColor.w;
    float3 lit = g_LightColor.xyz * ndl;

    float3 color = texColor.rgb * (ambient + lit);
    return float4(color, texColor.a);
}
)";
