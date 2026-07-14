#include <SagaEngine/Render/Shaders/ShaderVariantBudget.h>

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

[[nodiscard]] bool Contains(
    const std::vector<std::string>& values,
    const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

[[nodiscard]] bool MatchesFilter(
    const std::vector<std::string>& filter,
    const std::string& value)
{
    return filter.empty() || Contains(filter, value);
}

[[nodiscard]] std::unordered_set<std::string> FeatureNames(
    const ShaderVariantKey& variant)
{
    std::unordered_set<std::string> names;
    for (const ShaderVariantFeature& feature : variant.features)
    {
        names.insert(feature.name);
    }
    return names;
}

[[nodiscard]] bool HasInvalidPair(
    const std::unordered_set<std::string>& features,
    const std::vector<std::string>& pairs)
{
    for (const std::string& pair : pairs)
    {
        const std::size_t split = pair.find('+');
        if (split == std::string::npos)
        {
            continue;
        }
        const std::string lhs = pair.substr(0, split);
        const std::string rhs = pair.substr(split + 1);
        if (features.find(lhs) != features.end() &&
            features.find(rhs) != features.end())
        {
            return true;
        }
    }
    return false;
}

} // namespace

bool ShaderVariantBudgetReport::Succeeded() const noexcept
{
    return std::none_of(diagnostics.begin(), diagnostics.end(), IsError);
}

ShaderValidationResult ValidateMaterialFeatureMatrix(
    const MaterialFeatureMatrix& matrix)
{
    ShaderValidationResult result;
    std::unordered_set<std::string> allowed;
    for (const std::string& feature : matrix.allowedFeatures)
    {
        if (feature.empty())
        {
            AddDiagnostic(
                result.diagnostics,
                "ShaderVariantBudget.FeatureNameMissing",
                "allowedFeatures",
                "Material feature matrix contains an empty feature name.");
        }
        allowed.insert(feature);
    }

    for (const std::string& feature : matrix.alwaysOnFeatures)
    {
        if (!allowed.empty() && allowed.find(feature) == allowed.end())
        {
            AddDiagnostic(
                result.diagnostics,
                "ShaderVariantBudget.AlwaysOnFeatureUnknown",
                "alwaysOnFeatures",
                "Always-on feature is not listed as an allowed feature.");
        }
    }

    for (const std::string& pair : matrix.invalidPairs)
    {
        if (pair.find('+') == std::string::npos)
        {
            AddDiagnostic(
                result.diagnostics,
                "ShaderVariantBudget.InvalidPairMalformed",
                "invalidPairs",
                "Invalid feature pair must use the featureA+featureB form.");
        }
    }

    return result;
}

ShaderVariantBudgetReport EvaluateShaderVariantBudget(
    const ShaderVariantBudgetConfig& config,
    const std::vector<ShaderVariantBudgetCandidate>& candidates,
    const MaterialFeatureMatrix& matrix)
{
    ShaderVariantBudgetReport report;
    report.inputVariantCount = static_cast<std::uint32_t>(candidates.size());

    ShaderValidationResult matrixValidation = ValidateMaterialFeatureMatrix(matrix);
    report.diagnostics.insert(
        report.diagnostics.end(),
        matrixValidation.diagnostics.begin(),
        matrixValidation.diagnostics.end());

    std::vector<ShaderVariantBudgetCandidate> accepted;
    accepted.reserve(candidates.size());

    for (const ShaderVariantBudgetCandidate& candidate : candidates)
    {
        if (!MatchesFilter(candidate.targetPlatforms, config.targetPlatform) ||
            !MatchesFilter(candidate.backends, config.backend))
        {
            ++report.prunedVariantCount;
            continue;
        }

        ShaderValidationResult keyValidation =
            ValidateShaderVariantKey(candidate.variant);
        report.diagnostics.insert(
            report.diagnostics.end(),
            keyValidation.diagnostics.begin(),
            keyValidation.diagnostics.end());
        bool rejectCandidate = !keyValidation.Succeeded();

        const std::unordered_set<std::string> featureNames =
            FeatureNames(candidate.variant);
        if (featureNames.size() > config.maxFeatureFlagsPerMaterial)
        {
            AddDiagnostic(
                report.diagnostics,
                "ShaderVariantBudget.FeatureCountExceeded",
                "features",
                "Material feature count exceeds the configured budget.");
            rejectCandidate = true;
        }
        for (const std::string& feature : featureNames)
        {
            if (!matrix.allowedFeatures.empty() &&
                !Contains(matrix.allowedFeatures, feature))
            {
                AddDiagnostic(
                    report.diagnostics,
                    "ShaderVariantBudget.FeatureUnknown",
                    "features",
                    "Shader variant uses a material feature outside the matrix.");
                rejectCandidate = true;
            }
        }
        if (HasInvalidPair(featureNames, matrix.invalidPairs))
        {
            AddDiagnostic(
                report.diagnostics,
                "ShaderVariantBudget.InvalidFeatureCombination",
                "features",
                "Shader variant contains an invalid material feature combination.");
            rejectCandidate = true;
        }

        for (const std::string& feature : matrix.alwaysOnFeatures)
        {
            if (featureNames.find(feature) == featureNames.end())
            {
                AddDiagnostic(
                    report.diagnostics,
                    "ShaderVariantBudget.RequiredFeatureMissing",
                    "features",
                    "Shader variant is missing an always-on material feature.");
                rejectCandidate = true;
            }
        }

        if (rejectCandidate)
        {
            ++report.prunedVariantCount;
            continue;
        }

        accepted.push_back(candidate);
    }

    std::sort(
        accepted.begin(),
        accepted.end(),
        [](const ShaderVariantBudgetCandidate& lhs,
           const ShaderVariantBudgetCandidate& rhs)
        {
            return BuildShaderVariantStableString(lhs.variant) <
                   BuildShaderVariantStableString(rhs.variant);
        });

    const std::uint32_t emitCount = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(accepted.size()),
        config.maxVariantCount);
    report.prunedVariantCount +=
        static_cast<std::uint32_t>(accepted.size()) - emitCount;
    report.emittedVariantCount = emitCount;

    if (accepted.size() > config.maxVariantCount)
    {
        AddDiagnostic(
            report.diagnostics,
            "ShaderVariantBudget.VariantCountExceeded",
            "maxVariantCount",
            "Shader variant count exceeds the configured budget.");
    }

    for (std::uint32_t index = 0; index < emitCount; ++index)
    {
        const ShaderVariantBudgetCandidate& candidate = accepted[index];
        report.emittedVariants.push_back(candidate.variant);
        report.estimatedCacheBytes += candidate.estimatedCacheBytes;
        report.estimatedCompileMilliseconds +=
            candidate.estimatedCompileMilliseconds;
    }

    if (report.estimatedCacheBytes > config.maxEstimatedCacheBytes)
    {
        AddDiagnostic(
            report.diagnostics,
            "ShaderVariantBudget.CacheBytesExceeded",
            "maxEstimatedCacheBytes",
            "Estimated shader cache size exceeds the configured budget.");
    }
    if (report.estimatedCompileMilliseconds >
        config.maxEstimatedCompileMilliseconds)
    {
        AddDiagnostic(
            report.diagnostics,
            "ShaderVariantBudget.CompileTimeExceeded",
            "maxEstimatedCompileMilliseconds",
            "Estimated shader compile time exceeds the configured budget.");
    }

    return report;
}

} // namespace SagaEngine::Render
