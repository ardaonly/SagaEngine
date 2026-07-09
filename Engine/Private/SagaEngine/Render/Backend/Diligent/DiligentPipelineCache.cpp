/// @file DiligentPipelineCache.cpp
/// @brief Private Diligent graphics PSO cache helpers.

#include "SagaEngine/Render/Backend/Diligent/DiligentPipelineCache.h"

#include "SagaEngine/Core/Log/LogCategories.h"

#include "Buffer.h"
#include "PipelineState.h"
#include "RenderDevice.h"
#include "Shader.h"
#include "SwapChain.h"

namespace SagaEngine::Render::Backend
{

/// Creates or retrieves a cached PSO for the given key.
/// Takes raw Diligent pointers to avoid referencing the private Impl type
/// from the anonymous namespace.
Diligent::RefCntAutoPtr<Diligent::IPipelineState> FindOrCreatePSO(
    std::unordered_map<PSOCacheKey, Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PSOCacheKeyHash>& cache,
    Diligent::IRenderDevice&  device,
    Diligent::ISwapChain&     swapChain,
    Diligent::IShader*        solidVS,
    Diligent::IShader*        solidPS,
    Diligent::IBuffer*        cameraCB,
    const PSOCacheKey&        key)
{
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    using namespace Diligent;

    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name         = "SolidPSO";
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    psoCI.pVS                  = solidVS;
    psoCI.pPS                  = solidPS;

    const auto& scDesc = swapChain.GetDesc();
    psoCI.GraphicsPipeline.NumRenderTargets  = 1;
    psoCI.GraphicsPipeline.RTVFormats[0]     = scDesc.ColorBufferFormat;
    psoCI.GraphicsPipeline.DSVFormat         = scDesc.DepthBufferFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Depth
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable      = True;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable  = key.writesDepth ? True : False;

    // Cull — CCW front face (standard 3D convention; matches glTF, OBJ, FBX).
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
    switch (static_cast<MaterialCullMode>(key.cullMode))
    {
        case MaterialCullMode::Back:  psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;  break;
        case MaterialCullMode::Front: psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_FRONT; break;
        case MaterialCullMode::None:  psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;  break;
    }

    // Input layout matching MeshVertex (76 bytes):
    //   pos(3f) normal(3f) tangent(3f) handedness(1f) uv0(2f) uv1(2f)
    //   boneIndices(4u8) boneWeights(4f)
    LayoutElement layoutElems[] =
    {
        {0, 0, 3, VT_FLOAT32, False},  // Pos
        {1, 0, 3, VT_FLOAT32, False},  // Normal
        {2, 0, 3, VT_FLOAT32, False},  // Tangent
        {3, 0, 1, VT_FLOAT32, False},  // Handedness
        {4, 0, 2, VT_FLOAT32, False},  // UV0
        {5, 0, 2, VT_FLOAT32, False},  // UV1
        {6, 0, 4, VT_UINT8,   False},  // BoneIndices
        {7, 0, 4, VT_FLOAT32, False},  // BoneWeights
    };
    psoCI.GraphicsPipeline.InputLayout.NumElements    = 8;
    psoCI.GraphicsPipeline.InputLayout.LayoutElements  = layoutElems;

    // Texture/sampler resource layout: dynamic textures change per material/frame.
    ShaderResourceVariableDesc vars[] =
    {
        { SHADER_TYPE_PIXEL, "g_Albedo", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };
    psoCI.PSODesc.ResourceLayout.Variables    = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = 2;

    SamplerDesc sceneSamplerDesc;
    sceneSamplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.AddressU  = TEXTURE_ADDRESS_WRAP;
    sceneSamplerDesc.AddressV  = TEXTURE_ADDRESS_WRAP;
    sceneSamplerDesc.AddressW  = TEXTURE_ADDRESS_WRAP;

    SamplerDesc shadowSamplerDesc;
    shadowSamplerDesc.MinFilter = FILTER_TYPE_POINT;
    shadowSamplerDesc.MagFilter = FILTER_TYPE_POINT;
    shadowSamplerDesc.MipFilter = FILTER_TYPE_POINT;
    shadowSamplerDesc.AddressU  = TEXTURE_ADDRESS_CLAMP;
    shadowSamplerDesc.AddressV  = TEXTURE_ADDRESS_CLAMP;
    shadowSamplerDesc.AddressW  = TEXTURE_ADDRESS_CLAMP;

    ImmutableSamplerDesc samplers[] =
    {
        { SHADER_TYPE_PIXEL, "g_Albedo", sceneSamplerDesc },
        { SHADER_TYPE_PIXEL, "g_ShadowMap", shadowSamplerDesc },
    };
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers    = samplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 2;

    RefCntAutoPtr<IPipelineState> pso;
    device.CreateGraphicsPipelineState(psoCI, &pso);
    if (!pso)
    {
        LOG_CAT_ERROR(Render, "Failed to create PSO");
        return {};
    }

    // Bind CameraCB as a static variable on VS and PS.
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "CameraCB"))
        var->Set(cameraCB);
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "CameraCB"))
        var->Set(cameraCB);

    cache[key] = pso;
    return pso;
}

/// Same as FindOrCreatePSO but uses the skinned VS and binds BoneCB.
Diligent::RefCntAutoPtr<Diligent::IPipelineState> FindOrCreateSkinnedPSO(
    std::unordered_map<PSOCacheKey, Diligent::RefCntAutoPtr<Diligent::IPipelineState>, PSOCacheKeyHash>& cache,
    Diligent::IRenderDevice&  device,
    Diligent::ISwapChain&     swapChain,
    Diligent::IShader*        skinnedVS,
    Diligent::IShader*        solidPS,
    Diligent::IBuffer*        cameraCB,
    Diligent::IBuffer*        boneCB,
    const PSOCacheKey&        key)
{
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    using namespace Diligent;

    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name         = "SkinnedPSO";
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    psoCI.pVS                  = skinnedVS;
    psoCI.pPS                  = solidPS;

    const auto& scDesc = swapChain.GetDesc();
    psoCI.GraphicsPipeline.NumRenderTargets  = 1;
    psoCI.GraphicsPipeline.RTVFormats[0]     = scDesc.ColorBufferFormat;
    psoCI.GraphicsPipeline.DSVFormat         = scDesc.DepthBufferFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable      = True;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable  = key.writesDepth ? True : False;

    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
    switch (static_cast<MaterialCullMode>(key.cullMode))
    {
        case MaterialCullMode::Back:  psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;  break;
        case MaterialCullMode::Front: psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_FRONT; break;
        case MaterialCullMode::None:  psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;  break;
    }

    LayoutElement layoutElems[] =
    {
        {0, 0, 3, VT_FLOAT32, False},  // Pos
        {1, 0, 3, VT_FLOAT32, False},  // Normal
        {2, 0, 3, VT_FLOAT32, False},  // Tangent
        {3, 0, 1, VT_FLOAT32, False},  // Handedness
        {4, 0, 2, VT_FLOAT32, False},  // UV0
        {5, 0, 2, VT_FLOAT32, False},  // UV1
        {6, 0, 4, VT_UINT8,   False},  // BoneIndices
        {7, 0, 4, VT_FLOAT32, False},  // BoneWeights
    };
    psoCI.GraphicsPipeline.InputLayout.NumElements    = 8;
    psoCI.GraphicsPipeline.InputLayout.LayoutElements  = layoutElems;

    ShaderResourceVariableDesc vars[] =
    {
        { SHADER_TYPE_PIXEL, "g_Albedo", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };
    psoCI.PSODesc.ResourceLayout.Variables    = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = 2;

    SamplerDesc sceneSamplerDesc;
    sceneSamplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    sceneSamplerDesc.AddressU  = TEXTURE_ADDRESS_WRAP;
    sceneSamplerDesc.AddressV  = TEXTURE_ADDRESS_WRAP;
    sceneSamplerDesc.AddressW  = TEXTURE_ADDRESS_WRAP;

    SamplerDesc shadowSamplerDesc;
    shadowSamplerDesc.MinFilter = FILTER_TYPE_POINT;
    shadowSamplerDesc.MagFilter = FILTER_TYPE_POINT;
    shadowSamplerDesc.MipFilter = FILTER_TYPE_POINT;
    shadowSamplerDesc.AddressU  = TEXTURE_ADDRESS_CLAMP;
    shadowSamplerDesc.AddressV  = TEXTURE_ADDRESS_CLAMP;
    shadowSamplerDesc.AddressW  = TEXTURE_ADDRESS_CLAMP;

    ImmutableSamplerDesc samplers[] =
    {
        { SHADER_TYPE_PIXEL, "g_Albedo", sceneSamplerDesc },
        { SHADER_TYPE_PIXEL, "g_ShadowMap", shadowSamplerDesc },
    };
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers    = samplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 2;

    RefCntAutoPtr<IPipelineState> pso;
    device.CreateGraphicsPipelineState(psoCI, &pso);
    if (!pso)
    {
        LOG_CAT_ERROR(Render, "Failed to create skinned PSO");
        return {};
    }

    // Bind CameraCB and BoneCB as static variables.
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "CameraCB"))
        var->Set(cameraCB);
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "CameraCB"))
        var->Set(cameraCB);
    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "BoneCB"))
        var->Set(boneCB);

    cache[key] = pso;
    return pso;
}

Diligent::RefCntAutoPtr<Diligent::IPipelineState> CreateShadowDepthPSO(
    Diligent::IRenderDevice&  device,
    Diligent::IShader*        shadowVS,
    Diligent::IBuffer*        cameraCB,
    Diligent::TEXTURE_FORMAT  depthFormat)
{
    using namespace Diligent;

    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name         = "DirectionalShadowDepthPSO";
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    psoCI.pVS                  = shadowVS;
    psoCI.pPS                  = nullptr;

    psoCI.GraphicsPipeline.NumRenderTargets  = 0;
    psoCI.GraphicsPipeline.DSVFormat         = depthFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable      = True;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = True;
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;

    LayoutElement layoutElems[] =
    {
        {0, 0, 3, VT_FLOAT32, False},
        {1, 0, 3, VT_FLOAT32, False},
        {2, 0, 3, VT_FLOAT32, False},
        {3, 0, 1, VT_FLOAT32, False},
        {4, 0, 2, VT_FLOAT32, False},
        {5, 0, 2, VT_FLOAT32, False},
        {6, 0, 4, VT_UINT8,   False},
        {7, 0, 4, VT_FLOAT32, False},
    };
    psoCI.GraphicsPipeline.InputLayout.NumElements   = 8;
    psoCI.GraphicsPipeline.InputLayout.LayoutElements = layoutElems;

    RefCntAutoPtr<IPipelineState> pso;
    device.CreateGraphicsPipelineState(psoCI, &pso);
    if (!pso)
    {
        LOG_CAT_ERROR(Render, "Failed to create directional shadow depth PSO");
        return {};
    }

    if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "CameraCB"))
        var->Set(cameraCB);

    return pso;
}

} // namespace SagaEngine::Render::Backend
