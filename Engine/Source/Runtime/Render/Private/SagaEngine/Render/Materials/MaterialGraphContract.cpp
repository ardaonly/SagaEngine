#include <SagaEngine/Render/Materials/MaterialGraphContract.h>

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace SagaEngine::Render
{

namespace
{

[[nodiscard]] bool IsError(const ShaderDiagnostic& diagnostic) noexcept
{
    return diagnostic.severity == ShaderDiagnosticSeverity::Error;
}

void AddDiagnostic(
    std::vector<ShaderDiagnostic>& diagnostics,
    std::string diagnosticId,
    std::string field,
    std::string message,
    ShaderDiagnosticSeverity severity = ShaderDiagnosticSeverity::Error)
{
    diagnostics.push_back(ShaderDiagnostic{
        severity,
        std::move(diagnosticId),
        std::move(field),
        std::move(message),
    });
}

[[nodiscard]] bool IsSupportedNode(MaterialGraphNodeKind kind) noexcept
{
    return kind == MaterialGraphNodeKind::ConstantScalar ||
           kind == MaterialGraphNodeKind::ConstantVector ||
           kind == MaterialGraphNodeKind::TextureSample ||
           kind == MaterialGraphNodeKind::NormalTransform ||
           kind == MaterialGraphNodeKind::Add ||
           kind == MaterialGraphNodeKind::Multiply ||
           kind == MaterialGraphNodeKind::Fresnel;
}

[[nodiscard]] std::vector<ShaderVariantFeature> CollectFeatures(
    const MaterialGraphOutputSchema& graph)
{
    std::vector<ShaderVariantFeature> features = graph.requestedFeatures;
    for (const MaterialGraphNode& node : graph.nodes)
    {
        features.insert(
            features.end(),
            node.variantFeatures.begin(),
            node.variantFeatures.end());
    }
    std::sort(
        features.begin(),
        features.end(),
        [](const ShaderVariantFeature& lhs,
           const ShaderVariantFeature& rhs)
        {
            if (lhs.name != rhs.name)
            {
                return lhs.name < rhs.name;
            }
            return lhs.value < rhs.value;
        });
    features.erase(
        std::unique(
            features.begin(),
            features.end(),
            [](const ShaderVariantFeature& lhs,
               const ShaderVariantFeature& rhs)
            {
                return lhs.name == rhs.name && lhs.value == rhs.value;
            }),
        features.end());
    return features;
}

} // namespace

bool CookedMaterialGraphOutput::Succeeded() const noexcept
{
    return std::none_of(diagnostics.begin(), diagnostics.end(), IsError);
}

ShaderValidationResult ValidateMaterialGraphContract(
    const MaterialGraphOutputSchema& graph,
    const ShaderVariantBudgetConfig& budget)
{
    ShaderValidationResult result;
    if (graph.schemaVersion != kMaterialGraphSchemaVersion)
    {
        AddDiagnostic(
            result.diagnostics,
            "MaterialGraph.SchemaVersionUnsupported",
            "schemaVersion",
            "Material graph schema version is not supported.");
    }
    if (graph.shaderTemplateName.empty())
    {
        AddDiagnostic(
            result.diagnostics,
            "MaterialGraph.ShaderTemplateMissing",
            "shaderTemplateName",
            "Material graph output must name its shader template.");
    }
    if (graph.nodes.empty())
    {
        AddDiagnostic(
            result.diagnostics,
            "MaterialGraph.NodesMissing",
            "nodes",
            "Material graph output must contain at least one node.");
    }

    std::unordered_set<std::string> nodeIds;
    for (const MaterialGraphNode& node : graph.nodes)
    {
        if (node.nodeId.empty())
        {
            AddDiagnostic(
                result.diagnostics,
                "MaterialGraph.NodeIdMissing",
                "nodes",
                "Material graph node id must not be empty.");
        }
        else if (!nodeIds.insert(node.nodeId).second)
        {
            AddDiagnostic(
                result.diagnostics,
                "MaterialGraph.NodeIdDuplicate",
                "nodes",
                "Material graph output contains duplicate node ids.");
        }
        if (!IsSupportedNode(node.kind))
        {
            AddDiagnostic(
                result.diagnostics,
                "MaterialGraph.NodeUnsupported",
                "nodes",
                "Material graph output contains an unsupported node kind.");
        }
    }

    const std::vector<ShaderVariantFeature> features = CollectFeatures(graph);
    if (features.size() > budget.maxFeatureFlagsPerMaterial)
    {
        AddDiagnostic(
            result.diagnostics,
            "MaterialGraph.FeatureBudgetExceeded",
            "requestedFeatures",
            "Material graph output requests more shader features than the budget allows.");
    }

    ShaderVariantKey variant;
    variant.shaderId = graph.shaderTemplateName;
    variant.features = features;
    ShaderValidationResult variantValidation = ValidateShaderVariantKey(variant);
    result.diagnostics.insert(
        result.diagnostics.end(),
        variantValidation.diagnostics.begin(),
        variantValidation.diagnostics.end());

    return result;
}

CookedMaterialGraphOutput CookMaterialGraphToMaterialAsset(
    const MaterialGraphOutputSchema& graph,
    const ShaderVariantBudgetConfig& budget)
{
    CookedMaterialGraphOutput output;
    output.material.materialId = graph.materialId;
    output.material.debugName = graph.debugName;
    output.material.shaderTemplateName = graph.shaderTemplateName;
    output.material.renderQueue = graph.renderQueue;
    output.material.cullMode = graph.cullMode;
    output.material.writesDepth = graph.writesDepth;
    output.material.receivesShadows = graph.receivesShadows;
    output.material.castsShadows = graph.castsShadows;
    output.material.alphaTestThreshold = graph.alphaTestThreshold;

    output.variant.shaderId = graph.shaderTemplateName;
    output.variant.features = CollectFeatures(graph);

    ShaderValidationResult validation =
        ValidateMaterialGraphContract(graph, budget);
    output.diagnostics.insert(
        output.diagnostics.end(),
        validation.diagnostics.begin(),
        validation.diagnostics.end());

    for (const MaterialGraphNode& node : graph.nodes)
    {
        switch (node.kind)
        {
        case MaterialGraphNodeKind::ConstantScalar:
            if (output.material.scalarParams.size() < kMaxScalarParams)
            {
                output.material.scalarParams.push_back(node.scalarParam);
            }
            break;
        case MaterialGraphNodeKind::ConstantVector:
            if (output.material.vectorParams.size() < kMaxVectorParams)
            {
                output.material.vectorParams.push_back(node.vectorParam);
            }
            break;
        case MaterialGraphNodeKind::TextureSample:
        case MaterialGraphNodeKind::NormalTransform:
            if (output.material.textures.size() < kMaxTextureSlots)
            {
                output.material.textures.push_back(node.texture);
            }
            break;
        case MaterialGraphNodeKind::Add:
        case MaterialGraphNodeKind::Multiply:
        case MaterialGraphNodeKind::Fresnel:
        case MaterialGraphNodeKind::Unsupported:
            break;
        }
    }

    return output;
}

} // namespace SagaEngine::Render
