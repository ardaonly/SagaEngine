#include <SagaEngine/Render/Assets/RenderAssetManifest.h>
#include <SagaEngine/Render/Materials/MaterialGraphContract.h>
#include <SagaEngine/Render/Materials/MaterialRuntimeContract.h>
#include <SagaEngine/Render/Shaders/ShaderPipelineContract.h>
#include <SagaEngine/Render/Shaders/ShaderVariantBudget.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

namespace Render = SagaEngine::Render;

[[nodiscard]] bool HasDiagnostic(
    const std::vector<Render::ShaderDiagnostic>& diagnostics,
    std::string_view diagnosticId)
{
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [diagnosticId](const Render::ShaderDiagnostic& diagnostic)
        {
            return diagnostic.diagnosticId == diagnosticId;
        });
}

[[nodiscard]] Render::ShaderSourceDesc ValidShaderSource()
{
    Render::ShaderSourceDesc source;
    source.debugName = "terrain-main";
    source.sourceId = "terrain/main";
    source.sourceText = "float4 main() : SV_Target { return 1; }";
    source.entryPoint = "main";
    source.stage = Render::ShaderStage::Fragment;
    source.language = Render::ShaderSourceLanguage::Hlsl;
    return source;
}

[[nodiscard]] Render::ShaderVariantKey Variant(
    std::string shaderId,
    std::vector<Render::ShaderVariantFeature> features)
{
    Render::ShaderVariantKey key;
    key.shaderId = std::move(shaderId);
    key.features = std::move(features);
    return key;
}

[[nodiscard]] Render::RenderAssetManifest ValidManifest()
{
    Render::RenderAssetManifest manifest;
    manifest.textures.push_back(Render::RenderTextureCookMetadata{
        .assetId = "texture.b",
        .sourcePath = "Source/b.png",
        .cookedPath = "Cooked/b.ktx2",
        .format = "bc7",
        .residencyHint = "streamed",
        .width = 64,
        .height = 64,
        .mipCount = 7,
        .estimatedBytes = 128,
        .contentHash = "hash-b",
    });
    manifest.textures.push_back(Render::RenderTextureCookMetadata{
        .assetId = "texture.a",
        .sourcePath = "Source/a.png",
        .cookedPath = "Cooked/a.ktx2",
        .format = "bc7",
        .residencyHint = "streamed",
        .width = 32,
        .height = 32,
        .mipCount = 6,
        .estimatedBytes = 64,
        .contentHash = "hash-a",
    });
    manifest.meshes.push_back(Render::RenderMeshCookMetadata{
        .assetId = "mesh.hero",
        .sourcePath = "Source/hero.fbx",
        .cookedPath = "Cooked/hero.mesh",
        .vertexCount = 100,
        .indexCount = 300,
        .lodCount = 2,
        .hasTangents = true,
        .estimatedBytes = 4096,
        .contentHash = "hash-mesh",
    });
    manifest.materials.push_back(Render::RenderMaterialCookMetadata{
        .assetId = "material.hero",
        .cookedPath = "Cooked/hero.material",
        .shaderTemplateName = "pbr",
        .textureAssetIds = {"texture.b", "texture.a"},
        .shaderVariantIds = {"shader.pbr.base"},
        .compatibilityVersion = "material-v1",
    });
    manifest.shaderVariants.push_back(Render::RenderShaderVariantCookMetadata{
        .assetId = "shader.pbr.base",
        .stage = Render::ShaderStage::Fragment,
        .variant = Variant("pbr", {{std::string(Render::kMaterialFeatureNormalMap), "1"}}),
        .cachePath = "Cooked/pbr.shader",
        .reflectionPath = "Cooked/pbr.reflect",
        .estimatedCacheBytes = 2048,
        .estimatedCompileMilliseconds = 5,
    });
    return manifest;
}

} // namespace

TEST(ShaderPipelineContractTests, ValidatesSourceDiagnostics)
{
    Render::ShaderSourceDesc source = ValidShaderSource();
    EXPECT_TRUE(Render::ValidateShaderSource(source).Succeeded());

    source.sourceText.clear();
    source.entryPoint.clear();
    source.stage = static_cast<Render::ShaderStage>(255);
    source.language = static_cast<Render::ShaderSourceLanguage>(255);
    source.includePolicy = static_cast<Render::ShaderIncludePolicy>(255);

    const Render::ShaderValidationResult result =
        Render::ValidateShaderSource(source);
    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(HasDiagnostic(result.diagnostics, "Shader.SourceEmpty"));
    EXPECT_TRUE(HasDiagnostic(result.diagnostics, "Shader.EntryPointMissing"));
    EXPECT_TRUE(HasDiagnostic(result.diagnostics, "Shader.StageInvalid"));
    EXPECT_TRUE(HasDiagnostic(result.diagnostics, "Shader.LanguageInvalid"));
    EXPECT_TRUE(HasDiagnostic(result.diagnostics, "Shader.IncludePolicyInvalid"));
}

TEST(ShaderPipelineContractTests, MissingIncludeProducesDiagnostic)
{
    Render::ShaderSourceDesc source = ValidShaderSource();
    source.sourceText = "#include \"lighting/common.h\"\nfloat4 main() : SV_Target { return 1; }";
    source.includePolicy = Render::ShaderIncludePolicy::DeclaredOnly;

    const Render::ShaderValidationResult missing =
        Render::ValidateShaderSource(source);
    EXPECT_FALSE(missing.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        missing.diagnostics,
        "Shader.IncludeMissingDeclaration"));

    source.includes.push_back(Render::ShaderIncludeRef{
        .logicalName = "lighting/common.h",
        .path = "Shaders/lighting/common.h",
    });
    EXPECT_TRUE(Render::ValidateShaderSource(source).Succeeded());
}

TEST(ShaderPipelineContractTests, VariantKeyAndCachePathAreDeterministic)
{
    const Render::ShaderVariantKey first =
        Variant("pbr", {{"normalMap", "1"}, {"alphaTest", "1"}});
    const Render::ShaderVariantKey second =
        Variant("pbr", {{"alphaTest", "1"}, {"normalMap", "1"}});
    const Render::ShaderVariantKey different =
        Variant("pbr", {{"alphaTest", "0"}, {"normalMap", "1"}});

    EXPECT_EQ(
        Render::BuildShaderVariantStableString(first),
        Render::BuildShaderVariantStableString(second));
    EXPECT_EQ(
        Render::BuildShaderVariantStableHash(first),
        Render::BuildShaderVariantStableHash(second));
    EXPECT_NE(
        Render::BuildShaderVariantStableHash(first),
        Render::BuildShaderVariantStableHash(different));

    const Render::ShaderCompileProfile profile{
        .profileName = "dev-client",
        .targetPlatform = "linux",
        .backend = "portable",
        .optimization = Render::ShaderOptimizationProfile::Development,
        .generateDebugInfo = true,
        .cacheNamespace = "render-cache",
    };
    EXPECT_EQ(
        Render::BuildShaderCachePath(ValidShaderSource(), profile, first),
        Render::BuildShaderCachePath(ValidShaderSource(), profile, second));
}

TEST(ShaderPipelineContractTests, DuplicateVariantAndReflectionErrorsAreReported)
{
    const Render::ShaderValidationResult variantResult =
        Render::ValidateShaderVariantKey(
            Variant("pbr", {{"normalMap", "1"}, {"normalMap", "0"}}));
    EXPECT_FALSE(variantResult.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        variantResult.diagnostics,
        "ShaderVariant.FeatureDuplicate"));

    Render::ShaderReflectionSummary reflection;
    reflection.entryPoint = "main";
    reflection.resources.push_back(Render::ShaderResourceBindingSummary{
        .name = "Material",
        .resourceClass = "uniform",
        .slot = 0,
        .count = 1,
    });
    EXPECT_TRUE(Render::ValidateShaderReflectionSummary(reflection).Succeeded());

    reflection.resources.push_back(Render::ShaderResourceBindingSummary{
        .name = "Material",
        .resourceClass = "uniform",
        .slot = 1,
        .count = 1,
    });
    const Render::ShaderValidationResult reflectionResult =
        Render::ValidateShaderReflectionSummary(reflection);
    EXPECT_FALSE(reflectionResult.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        reflectionResult.diagnostics,
        "ShaderReflection.ResourceDuplicate"));
}

TEST(ShaderVariantBudgetTests, EnforcesThresholdsInvalidPairsAndFilters)
{
    Render::ShaderVariantBudgetConfig config;
    config.maxVariantCount = 1;
    config.maxFeatureFlagsPerMaterial = 2;
    config.maxEstimatedCacheBytes = 100;
    config.maxEstimatedCompileMilliseconds = 5;
    config.targetPlatform = "linux";
    config.backend = "portable";

    Render::MaterialFeatureMatrix matrix;
    matrix.allowedFeatures = {"alphaTest", "transparent", "normalMap"};
    matrix.invalidPairs = {"alphaTest+transparent"};

    const std::vector<Render::ShaderVariantBudgetCandidate> candidates = {
        Render::ShaderVariantBudgetCandidate{
            .variant = Variant("pbr.a", {{"alphaTest", "1"}}),
            .targetPlatforms = {"linux"},
            .backends = {"portable"},
            .estimatedCacheBytes = 200,
            .estimatedCompileMilliseconds = 6,
        },
        Render::ShaderVariantBudgetCandidate{
            .variant = Variant("pbr.b", {{"normalMap", "1"}}),
            .targetPlatforms = {"linux"},
            .backends = {"portable"},
            .estimatedCacheBytes = 200,
            .estimatedCompileMilliseconds = 6,
        },
        Render::ShaderVariantBudgetCandidate{
            .variant = Variant("pbr.a", {{"alphaTest", "1"}, {"transparent", "1"}}),
            .targetPlatforms = {"linux"},
            .backends = {"portable"},
            .estimatedCacheBytes = 200,
            .estimatedCompileMilliseconds = 6,
        },
        Render::ShaderVariantBudgetCandidate{
            .variant = Variant("pbr.filtered", {{"normalMap", "1"}}),
            .targetPlatforms = {"windows"},
            .backends = {"portable"},
            .estimatedCacheBytes = 200,
            .estimatedCompileMilliseconds = 6,
        },
    };

    const Render::ShaderVariantBudgetReport report =
        Render::EvaluateShaderVariantBudget(config, candidates, matrix);
    EXPECT_FALSE(report.Succeeded());
    EXPECT_EQ(report.inputVariantCount, 4u);
    EXPECT_EQ(report.emittedVariantCount, 1u);
    EXPECT_EQ(report.prunedVariantCount, 3u);
    ASSERT_EQ(report.emittedVariants.size(), 1u);
    EXPECT_EQ(report.emittedVariants[0].shaderId, "pbr.a");
    EXPECT_TRUE(HasDiagnostic(
        report.diagnostics,
        "ShaderVariantBudget.InvalidFeatureCombination"));
    EXPECT_TRUE(HasDiagnostic(
        report.diagnostics,
        "ShaderVariantBudget.VariantCountExceeded"));
    EXPECT_TRUE(HasDiagnostic(
        report.diagnostics,
        "ShaderVariantBudget.CacheBytesExceeded"));
    EXPECT_TRUE(HasDiagnostic(
        report.diagnostics,
        "ShaderVariantBudget.CompileTimeExceeded"));
}

TEST(RenderAssetManifestContractTests, SerializesDeterministicallyAndValidates)
{
    const Render::RenderAssetManifest manifest = ValidManifest();
    const std::string json = Render::SerializeRenderAssetManifestJson(manifest);
    const std::string jsonAgain =
        Render::SerializeRenderAssetManifestJson(manifest);
    EXPECT_EQ(json, jsonAgain);
    EXPECT_LT(json.find("\"texture.a\""), json.find("\"texture.b\""));
    EXPECT_LT(json.find("\"assetId\""), json.find("\"contentHash\""));

    Render::RenderAssetManifestValidationOptions options;
    options.requireCookedArtifactsPresent = true;
    options.availableCookedPaths = {
        "Cooked/a.ktx2",
        "Cooked/b.ktx2",
        "Cooked/hero.mesh",
        "Cooked/hero.material",
        "Cooked/pbr.shader",
    };
    EXPECT_TRUE(
        Render::ValidateRenderAssetManifest(manifest, options).Succeeded());

    options.availableCookedPaths.pop_back();
    const Render::RenderAssetManifestValidationResult missing =
        Render::ValidateRenderAssetManifest(manifest, options);
    EXPECT_FALSE(missing.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        missing.diagnostics,
        "RenderAssetManifest.CookedAssetMissing"));
}

TEST(MaterialGraphContractTests, ReportsUnsupportedNodesAndFeatureBudget)
{
    Render::MaterialGraphOutputSchema graph;
    graph.schemaVersion = 999;
    graph.materialId = 42;
    graph.debugName = "hero";
    graph.shaderTemplateName = "pbr";
    graph.nodes.push_back(Render::MaterialGraphNode{
        .nodeId = "unsupported",
        .kind = Render::MaterialGraphNodeKind::Unsupported,
        .outputName = "baseColor",
    });
    graph.requestedFeatures = {
        {"baseColorTexture", "1"},
        {"normalMap", "1"},
    };

    Render::ShaderVariantBudgetConfig budget;
    budget.maxFeatureFlagsPerMaterial = 1;

    const Render::ShaderValidationResult result =
        Render::ValidateMaterialGraphContract(graph, budget);
    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        result.diagnostics,
        "MaterialGraph.SchemaVersionUnsupported"));
    EXPECT_TRUE(HasDiagnostic(result.diagnostics, "MaterialGraph.NodeUnsupported"));
    EXPECT_TRUE(HasDiagnostic(
        result.diagnostics,
        "MaterialGraph.FeatureBudgetExceeded"));
}

TEST(MaterialGraphContractTests, CookedGraphMapsToMaterialAssetAndVariant)
{
    Render::MaterialGraphOutputSchema graph;
    graph.materialId = 7;
    graph.debugName = "hero";
    graph.shaderTemplateName = "pbr";
    graph.nodes.push_back(Render::MaterialGraphNode{
        .nodeId = "roughness",
        .kind = Render::MaterialGraphNodeKind::ConstantScalar,
        .outputName = "roughness",
        .scalarParam = Render::MaterialScalarParam{
            .name = "roughness",
            .value = 0.45f,
        },
    });
    graph.nodes.push_back(Render::MaterialGraphNode{
        .nodeId = "albedo",
        .kind = Render::MaterialGraphNodeKind::TextureSample,
        .outputName = "baseColor",
        .variantFeatures = {{"baseColorTexture", "1"}},
        .texture = Render::MaterialTextureRef{
            .slot = Render::MaterialTextureSlot::Albedo,
            .assetId = 1001,
            .fallbackPath = "",
            .residencyHint = Render::MaterialTextureResidencyHint::Streamed,
        },
    });

    const Render::CookedMaterialGraphOutput cooked =
        Render::CookMaterialGraphToMaterialAsset(graph);
    EXPECT_TRUE(cooked.Succeeded());
    EXPECT_EQ(cooked.material.materialId, 7u);
    EXPECT_EQ(cooked.material.shaderTemplateName, "pbr");
    ASSERT_EQ(cooked.material.scalarParams.size(), 1u);
    EXPECT_EQ(cooked.material.scalarParams[0].name, "roughness");
    ASSERT_EQ(cooked.material.textures.size(), 1u);
    EXPECT_EQ(cooked.material.textures[0].assetId, 1001u);
    ASSERT_EQ(cooked.variant.features.size(), 1u);
    EXPECT_EQ(cooked.variant.features[0].name, "baseColorTexture");
}

TEST(MaterialRuntimeContractTests, BuildsDefaultMaterialAndFallbackPlan)
{
    const auto shader =
        static_cast<Render::ShaderTemplateHandle>(12u);
    const auto fallback =
        static_cast<Render::TextureHandle>(99u);

    const Render::MaterialRuntime defaultRuntime =
        Render::MakeDefaultMaterialRuntime(shader, fallback);
    EXPECT_EQ(defaultRuntime.shaderHandle, shader);
    EXPECT_EQ(defaultRuntime.textures[0], fallback);
    EXPECT_FLOAT_EQ(defaultRuntime.vectors[0].x, 1.0f);
    EXPECT_FLOAT_EQ(defaultRuntime.vectors[0].y, 0.0f);

    Render::MaterialAsset asset;
    asset.materialId = 88;
    asset.shaderTemplateName = "pbr";
    asset.scalarParams.push_back(Render::MaterialScalarParam{
        .name = "metallic",
        .value = 0.25f,
    });
    asset.textures.push_back(Render::MaterialTextureRef{
        .slot = Render::MaterialTextureSlot::Albedo,
        .assetId = 0,
        .fallbackPath = "debug/missing.png",
        .residencyHint = Render::MaterialTextureResidencyHint::PlaceholderAllowed,
    });

    const Render::MaterialRuntimeBuildResult build =
        Render::BuildMaterialRuntime(
            asset,
            Render::MaterialRuntimeBuildOptions{
                .shaderHandle = shader,
                .missingTextureFallback = fallback,
            });
    EXPECT_TRUE(build.Succeeded());
    EXPECT_EQ(build.runtime.materialId, 88u);
    EXPECT_FLOAT_EQ(build.runtime.scalars[0], 0.25f);
    EXPECT_EQ(build.runtime.textures[0], fallback);

    const Render::MaterialBindingPlan plan =
        Render::BuildMaterialBindingPlan(build.runtime, fallback);
    EXPECT_EQ(plan.shaderHandle, shader);
    EXPECT_TRUE(plan.usesMissingTextureFallback);
}

TEST(MaterialRuntimeContractTests, ParameterOverrideAndResidencyValidation)
{
    Render::MaterialRuntime runtime =
        Render::MakeDefaultMaterialRuntime(
            static_cast<Render::ShaderTemplateHandle>(1u),
            static_cast<Render::TextureHandle>(2u));

    const Render::ShaderValidationResult overrideResult =
        Render::ApplyMaterialParameterOverride(
            runtime,
            Render::MaterialParameterOverride{
                .kind = Render::MaterialParameterOverrideKind::Scalar,
                .index = 1,
                .scalarValue = 0.75f,
            });
    EXPECT_TRUE(overrideResult.Succeeded());
    EXPECT_FLOAT_EQ(runtime.scalars[1], 0.75f);

    Render::MaterialAsset asset;
    asset.shaderTemplateName = "pbr";
    asset.textures.push_back(Render::MaterialTextureRef{
        .slot = Render::MaterialTextureSlot::Albedo,
        .assetId = 123,
        .fallbackPath = "",
        .residencyHint =
            static_cast<Render::MaterialTextureResidencyHint>(255),
    });
    const Render::ShaderValidationResult validation =
        Render::ValidateMaterialAssetContract(asset);
    EXPECT_FALSE(validation.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        validation.diagnostics,
        "Material.TextureResidencyHintInvalid"));
}
