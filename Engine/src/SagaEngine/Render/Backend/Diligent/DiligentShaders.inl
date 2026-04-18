/// @file DiligentShaders.inl
/// @brief Embedded HLSL shader source for the solid-color pipeline, included from DiligentRenderBackend.cpp.
///
/// MeshVertex layout: pos(3) normal(3) tangent(3) handedness(1) uv0(2) uv1(2)
/// = 14 floats = 56 bytes.  The VS reads pos + normal; PS outputs a flat
/// colour derived from the normal (simple directional lighting).

// ─── Solid-color vertex shader ───────────────────────────────────────

static constexpr char kSolidVS[] = R"(
cbuffer CameraCB
{
    float4x4 g_ViewProj;
    float4x4 g_Model;
};

struct VSInput
{
    float3 Pos        : ATTRIB0;
    float3 Normal     : ATTRIB1;
    float3 Tangent    : ATTRIB2;
    float  Handedness : ATTRIB3;
    float2 UV0        : ATTRIB4;
    float2 UV1        : ATTRIB5;
};

struct PSInput
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    // Column-major matrices: mul(matrix, vector) is the standard M * v.
    float4 worldPos = mul(g_Model, float4(VSIn.Pos, 1.0));
    PSIn.Pos    = mul(g_ViewProj, worldPos);
    PSIn.Normal = mul(g_Model, float4(VSIn.Normal, 0.0)).xyz;
}
)";

// ─── Solid-color pixel shader ────────────────────────────────────────

static constexpr char kSolidPS[] = R"(
struct PSInput
{
    float4 Pos    : SV_POSITION;
    float3 Normal : NORMAL;
};

float4 main(in PSInput PSIn) : SV_Target
{
    float3 lightDir = normalize(float3(0.3, 1.0, 0.5));
    float  ndl      = saturate(dot(normalize(PSIn.Normal), lightDir));
    float3 color    = float3(0.8, 0.8, 0.8) * (0.15 + 0.85 * ndl);
    return float4(color, 1.0);
}
)";
