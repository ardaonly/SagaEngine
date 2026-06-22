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
    float4   g_NormalMatrixRow0;
    float4   g_NormalMatrixRow1;
    float4   g_NormalMatrixRow2;
    float4   g_LightDirection; // xyz = direction light rays travel, w = directional enabled
    float4   g_LightColor;     // xyz = light colour, w = intensity
    float4   g_AmbientColor;   // xyz = ambient colour, w = intensity
    float4x4 g_LightViewProj;
    float4   g_ShadowParams;   // x = constant bias, y = normal bias, z = inv resolution, w = shadows enabled
    float4   g_ShadingFlags;   // x = shading model (0 unlit, 1 lit diffuse)
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
    float3 WorldPos : TEXCOORD1;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    // Column-major matrices: mul(matrix, vector) is the standard M * v.
    float4 worldPos = mul(g_Model, float4(VSIn.Pos, 1.0));
    PSIn.Pos    = mul(g_ViewProj, worldPos);
    PSIn.WorldPos = worldPos.xyz;
    PSIn.Normal = float3(
        dot(g_NormalMatrixRow0.xyz, VSIn.Normal),
        dot(g_NormalMatrixRow1.xyz, VSIn.Normal),
        dot(g_NormalMatrixRow2.xyz, VSIn.Normal));
    PSIn.UV     = VSIn.UV0;
}
)";

// ─── Skinned vertex shader ──────────────────────────────────────────

static constexpr char kSkinnedVS[] = R"(
cbuffer CameraCB
{
    float4x4 g_ViewProj;
    float4x4 g_Model;
    float4   g_NormalMatrixRow0;
    float4   g_NormalMatrixRow1;
    float4   g_NormalMatrixRow2;
    float4   g_LightDirection;
    float4   g_LightColor;
    float4   g_AmbientColor;
    float4x4 g_LightViewProj;
    float4   g_ShadowParams;
    float4   g_ShadingFlags;
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
    float3 WorldPos : TEXCOORD1;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    // Weighted bone skinning (up to 4 influences).
    float4x4 skin = g_BonePalette[VSIn.BoneIdx.x] * VSIn.BoneWgt.x;
    skin += g_BonePalette[VSIn.BoneIdx.y] * VSIn.BoneWgt.y;
    skin += g_BonePalette[VSIn.BoneIdx.z] * VSIn.BoneWgt.z;
    skin += g_BonePalette[VSIn.BoneIdx.w] * VSIn.BoneWgt.w;

    float4 skinnedPos    = mul(skin, float4(VSIn.Pos, 1.0));
    float3 skinnedNormal = mul(skin, float4(VSIn.Normal, 0.0)).xyz;

    float4 worldPos = mul(g_Model, skinnedPos);
    PSIn.Pos    = mul(g_ViewProj, worldPos);
    PSIn.WorldPos = worldPos.xyz;
    PSIn.Normal = float3(
        dot(g_NormalMatrixRow0.xyz, skinnedNormal),
        dot(g_NormalMatrixRow1.xyz, skinnedNormal),
        dot(g_NormalMatrixRow2.xyz, skinnedNormal));
    PSIn.UV     = VSIn.UV0;
}
)";

// ─── Shadow depth vertex shader ─────────────────────────────────────

static constexpr char kShadowDepthVS[] = R"(
cbuffer CameraCB
{
    float4x4 g_ViewProj;
    float4x4 g_Model;
    float4   g_NormalMatrixRow0;
    float4   g_NormalMatrixRow1;
    float4   g_NormalMatrixRow2;
    float4   g_LightDirection;
    float4   g_LightColor;
    float4   g_AmbientColor;
    float4x4 g_LightViewProj;
    float4   g_ShadowParams;
    float4   g_ShadingFlags;
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

struct VSOutput
{
    float4 Pos : SV_POSITION;
};

void main(in VSInput VSIn, out VSOutput VSOut)
{
    float4 worldPos = mul(g_Model, float4(VSIn.Pos, 1.0));
    VSOut.Pos = mul(g_LightViewProj, worldPos);
}
)";

// ─── Pixel shader ───────────────────────────────────────────────────

static constexpr char kSolidPS[] = R"(
cbuffer CameraCB
{
    float4x4 g_ViewProj;
    float4x4 g_Model;
    float4   g_NormalMatrixRow0;
    float4   g_NormalMatrixRow1;
    float4   g_NormalMatrixRow2;
    float4   g_LightDirection;
    float4   g_LightColor;
    float4   g_AmbientColor;
    float4x4 g_LightViewProj;
    float4   g_ShadowParams;
    float4   g_ShadingFlags;
};

Texture2D    g_Albedo  : register(t0);
SamplerState g_Albedo_sampler : register(s0);
Texture2D    g_ShadowMap : register(t1);
SamplerState g_ShadowMap_sampler : register(s1);

struct PSInput
{
    float4 Pos    : SV_POSITION;
    float3 WorldPos : TEXCOORD1;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
};

float ComputeShadowVisibility(float3 worldPos, float3 normal, float3 lightToSurface)
{
    if (g_ShadowParams.w < 0.5)
        return 1.0;

    float4 lightClip = mul(g_LightViewProj, float4(worldPos, 1.0));
    if (abs(lightClip.w) < 0.00001)
        return 1.0;

    float3 ndc = lightClip.xyz / lightClip.w;
    float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    float receiverDepth = ndc.z;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 ||
        receiverDepth < 0.0 || receiverDepth > 1.0)
    {
        return 1.0;
    }

    float ndl = saturate(dot(normal, lightToSurface));
    float bias = g_ShadowParams.x + g_ShadowParams.y * (1.0 - ndl);
    float texel = g_ShadowParams.z;

    float visibility = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleUv = uv + float2((float)x, (float)y) * texel;
            if (sampleUv.x < 0.0 || sampleUv.x > 1.0 ||
                sampleUv.y < 0.0 || sampleUv.y > 1.0)
            {
                visibility += 1.0;
            }
            else
            {
                float shadowDepth = g_ShadowMap.SampleLevel(g_ShadowMap_sampler, sampleUv, 0).r;
                visibility += ((receiverDepth - bias) > shadowDepth) ? 0.0 : 1.0;
            }
        }
    }

    return visibility / 9.0;
}

float4 main(in PSInput PSIn) : SV_Target
{
    float4 texColor = g_Albedo.Sample(g_Albedo_sampler, PSIn.UV);

    if (g_ShadingFlags.x < 0.5)
        return texColor;

    float3 N = normalize(PSIn.Normal);
    float3 lightToSurface = normalize(-g_LightDirection.xyz);
    float  ndl = saturate(dot(N, lightToSurface)) * g_LightDirection.w;

    float3 ambient = texColor.rgb * g_AmbientColor.xyz * g_AmbientColor.w;
    float shadowVisibility = ComputeShadowVisibility(PSIn.WorldPos, N, lightToSurface);
    float3 diffuse = texColor.rgb * g_LightColor.xyz * g_LightColor.w * ndl * shadowVisibility;

    float3 color = ambient + diffuse;
    return float4(color, texColor.a);
}
)";
