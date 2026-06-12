/// @file MaterialGraphContract.h
/// @brief Renderer-facing material graph output schema.

#pragma once

#include "SagaEngine/Render/Materials/Material.h"
#include "SagaEngine/Render/Shaders/ShaderVariantBudget.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Render
{

inline constexpr std::uint32_t kMaterialGraphSchemaVersion = 1;

enum class MaterialGraphNodeKind : std::uint8_t
{
    ConstantScalar = 0,
    ConstantVector,
    TextureSample,
    NormalTransform,
    Add,
    Multiply,
    Fresnel,
    Unsupported,
};

struct MaterialGraphInputRef
{
    std::string sourceNodeId;
    std::string outputName;
};

struct MaterialGraphNode
{
    std::string nodeId;
    MaterialGraphNodeKind kind = MaterialGraphNodeKind::ConstantScalar;
    std::string outputName;
    std::vector<MaterialGraphInputRef> inputs;
    std::vector<ShaderVariantFeature> variantFeatures;
    MaterialScalarParam scalarParam;
    MaterialVectorParam vectorParam;
    MaterialTextureRef texture;
};

struct MaterialGraphOutputSchema
{
    std::uint32_t schemaVersion = kMaterialGraphSchemaVersion;
    std::uint64_t materialId = 0;
    std::string debugName;
    std::string shaderTemplateName;
    MaterialRenderQueue renderQueue = MaterialRenderQueue::Opaque;
    MaterialCullMode cullMode = MaterialCullMode::Back;
    bool writesDepth = true;
    bool receivesShadows = true;
    bool castsShadows = true;
    float alphaTestThreshold = 0.5f;
    std::vector<MaterialGraphNode> nodes;
    std::vector<ShaderVariantFeature> requestedFeatures;
};

struct CookedMaterialGraphOutput
{
    MaterialAsset material;
    ShaderVariantKey variant;
    std::vector<ShaderDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept;
};

[[nodiscard]] ShaderValidationResult ValidateMaterialGraphContract(
    const MaterialGraphOutputSchema& graph,
    const ShaderVariantBudgetConfig& budget = {});

[[nodiscard]] CookedMaterialGraphOutput CookMaterialGraphToMaterialAsset(
    const MaterialGraphOutputSchema& graph,
    const ShaderVariantBudgetConfig& budget = {});

} // namespace SagaEngine::Render
