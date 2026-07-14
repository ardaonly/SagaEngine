#include <SagaEngine/Render/Shaders/ShaderPipelineContract.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
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

[[nodiscard]] std::vector<ShaderVariantFeature> SortedFeatures(
    const ShaderVariantKey& variant)
{
    std::vector<ShaderVariantFeature> features = variant.features;
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
    return features;
}

[[nodiscard]] std::string SanitizeToken(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char ch : value)
    {
        const bool valid =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-' || ch == '.';
        result.push_back(valid ? ch : '_');
    }
    return result.empty() ? std::string("unnamed") : result;
}

void AppendHash(std::uint64_t& hash, std::string_view value) noexcept
{
    constexpr std::uint64_t kPrime = 1099511628211ull;
    for (const unsigned char byte : value)
    {
        hash ^= byte;
        hash *= kPrime;
    }
}

[[nodiscard]] std::string Hex(std::uint64_t value)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << value;
    return output.str();
}

[[nodiscard]] bool HasDeclaredInclude(
    const ShaderSourceDesc& source,
    std::string_view includeName)
{
    return std::any_of(
        source.includes.begin(),
        source.includes.end(),
        [includeName](const ShaderIncludeRef& include)
        {
            return include.logicalName == includeName ||
                   include.path == includeName;
        });
}

void ValidateIncludes(
    const ShaderSourceDesc& source,
    std::vector<ShaderDiagnostic>& diagnostics)
{
    std::size_t cursor = 0;
    while ((cursor = source.sourceText.find("#include", cursor)) !=
           std::string::npos)
    {
        const std::size_t quoteStart =
            source.sourceText.find_first_of("\"<", cursor);
        if (quoteStart == std::string::npos)
        {
            AddDiagnostic(
                diagnostics,
                "Shader.IncludeMalformed",
                "sourceText",
                "Shader include directive is missing a quoted path.");
            cursor += 8;
            continue;
        }

        const char closeChar =
            source.sourceText[quoteStart] == '"' ? '"' : '>';
        const std::size_t quoteEnd =
            source.sourceText.find(closeChar, quoteStart + 1);
        if (quoteEnd == std::string::npos)
        {
            AddDiagnostic(
                diagnostics,
                "Shader.IncludeMalformed",
                "sourceText",
                "Shader include directive is missing its closing delimiter.");
            cursor = quoteStart + 1;
            continue;
        }

        const std::string includeName =
            source.sourceText.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

        if (source.includePolicy == ShaderIncludePolicy::Disallow)
        {
            AddDiagnostic(
                diagnostics,
                "Shader.IncludeDisallowed",
                "includePolicy",
                "Shader source contains an include while includes are disallowed.");
        }
        else if (source.includePolicy == ShaderIncludePolicy::DeclaredOnly &&
                 !HasDeclaredInclude(source, includeName))
        {
            AddDiagnostic(
                diagnostics,
                "Shader.IncludeMissingDeclaration",
                "includes",
                "Shader include is not present in the declared include set.");
        }

        cursor = quoteEnd + 1;
    }
}

} // namespace

bool ShaderValidationResult::Succeeded() const noexcept
{
    return std::none_of(diagnostics.begin(), diagnostics.end(), IsError);
}

bool IsValidShaderStage(ShaderStage stage) noexcept
{
    return stage == ShaderStage::Vertex ||
           stage == ShaderStage::Fragment ||
           stage == ShaderStage::Compute;
}

bool IsValidShaderSourceLanguage(ShaderSourceLanguage language) noexcept
{
    return language == ShaderSourceLanguage::Hlsl ||
           language == ShaderSourceLanguage::Glsl ||
           language == ShaderSourceLanguage::Slang ||
           language == ShaderSourceLanguage::Binary;
}

bool IsValidShaderIncludePolicy(ShaderIncludePolicy policy) noexcept
{
    return policy == ShaderIncludePolicy::Disallow ||
           policy == ShaderIncludePolicy::DeclaredOnly ||
           policy == ShaderIncludePolicy::ProjectRelative;
}

std::string_view ToString(ShaderStage stage) noexcept
{
    switch (stage)
    {
    case ShaderStage::Vertex:
        return "vertex";
    case ShaderStage::Fragment:
        return "fragment";
    case ShaderStage::Compute:
        return "compute";
    }
    return "invalid";
}

std::string_view ToString(ShaderSourceLanguage language) noexcept
{
    switch (language)
    {
    case ShaderSourceLanguage::Hlsl:
        return "hlsl";
    case ShaderSourceLanguage::Glsl:
        return "glsl";
    case ShaderSourceLanguage::Slang:
        return "slang";
    case ShaderSourceLanguage::Binary:
        return "binary";
    }
    return "invalid";
}

std::string_view ToString(ShaderIncludePolicy policy) noexcept
{
    switch (policy)
    {
    case ShaderIncludePolicy::Disallow:
        return "disallow";
    case ShaderIncludePolicy::DeclaredOnly:
        return "declaredOnly";
    case ShaderIncludePolicy::ProjectRelative:
        return "projectRelative";
    }
    return "invalid";
}

std::string_view ToString(ShaderOptimizationProfile profile) noexcept
{
    switch (profile)
    {
    case ShaderOptimizationProfile::Debug:
        return "debug";
    case ShaderOptimizationProfile::Development:
        return "development";
    case ShaderOptimizationProfile::Shipping:
        return "shipping";
    }
    return "invalid";
}

ShaderValidationResult ValidateShaderSource(const ShaderSourceDesc& source)
{
    ShaderValidationResult result;
    if (source.sourceId.empty())
    {
        AddDiagnostic(
            result.diagnostics,
            "Shader.SourceIdMissing",
            "sourceId",
            "Shader source must provide a stable source id.");
    }
    if (source.sourceText.empty())
    {
        AddDiagnostic(
            result.diagnostics,
            "Shader.SourceEmpty",
            "sourceText",
            "Shader source text must not be empty.");
    }
    if (source.entryPoint.empty())
    {
        AddDiagnostic(
            result.diagnostics,
            "Shader.EntryPointMissing",
            "entryPoint",
            "Shader source must declare an entry point.");
    }
    if (!IsValidShaderStage(source.stage))
    {
        AddDiagnostic(
            result.diagnostics,
            "Shader.StageInvalid",
            "stage",
            "Shader stage is not part of the public shader contract.");
    }
    if (!IsValidShaderSourceLanguage(source.language))
    {
        AddDiagnostic(
            result.diagnostics,
            "Shader.LanguageInvalid",
            "language",
            "Shader language is not part of the public shader contract.");
    }
    if (!IsValidShaderIncludePolicy(source.includePolicy))
    {
        AddDiagnostic(
            result.diagnostics,
            "Shader.IncludePolicyInvalid",
            "includePolicy",
            "Shader include policy is not supported.");
    }
    else
    {
        ValidateIncludes(source, result.diagnostics);
    }

    return result;
}

ShaderValidationResult ValidateShaderVariantKey(
    const ShaderVariantKey& variant)
{
    ShaderValidationResult result;
    if (variant.shaderId.empty())
    {
        AddDiagnostic(
            result.diagnostics,
            "ShaderVariant.ShaderIdMissing",
            "shaderId",
            "Shader variant key must identify its shader.");
    }

    std::unordered_set<std::string> names;
    for (const ShaderVariantFeature& feature : variant.features)
    {
        if (feature.name.empty())
        {
            AddDiagnostic(
                result.diagnostics,
                "ShaderVariant.FeatureNameMissing",
                "features",
                "Shader variant feature names must not be empty.");
            continue;
        }
        if (!names.insert(feature.name).second)
        {
            AddDiagnostic(
                result.diagnostics,
                "ShaderVariant.FeatureDuplicate",
                "features",
                "Shader variant key contains a duplicate feature.");
        }
    }

    return result;
}

ShaderValidationResult ValidateShaderReflectionSummary(
    const ShaderReflectionSummary& reflection)
{
    ShaderValidationResult result;
    if (reflection.entryPoint.empty())
    {
        AddDiagnostic(
            result.diagnostics,
            "ShaderReflection.EntryPointMissing",
            "entryPoint",
            "Shader reflection summary must include an entry point.");
    }
    if (!IsValidShaderStage(reflection.stage))
    {
        AddDiagnostic(
            result.diagnostics,
            "ShaderReflection.StageInvalid",
            "stage",
            "Shader reflection stage is invalid.");
    }

    std::unordered_set<std::string> resourceNames;
    for (const ShaderResourceBindingSummary& resource : reflection.resources)
    {
        if (resource.name.empty())
        {
            AddDiagnostic(
                result.diagnostics,
                "ShaderReflection.ResourceNameMissing",
                "resources",
                "Shader reflection resources must have stable names.");
        }
        else if (!resourceNames.insert(resource.name).second)
        {
            AddDiagnostic(
                result.diagnostics,
                "ShaderReflection.ResourceDuplicate",
                "resources",
                "Shader reflection contains duplicate resource names.");
        }
        if (resource.count == 0)
        {
            AddDiagnostic(
                result.diagnostics,
                "ShaderReflection.ResourceCountInvalid",
                "resources",
                "Shader reflection resource count must be non-zero.");
        }
    }

    return result;
}

std::string BuildShaderVariantStableString(const ShaderVariantKey& variant)
{
    std::ostringstream output;
    output << "shader=" << variant.shaderId;
    for (const ShaderVariantFeature& feature : SortedFeatures(variant))
    {
        output << ";feature=" << feature.name << ':' << feature.value;
    }
    return output.str();
}

std::uint64_t BuildShaderVariantStableHash(const ShaderVariantKey& variant)
{
    std::uint64_t hash = 14695981039346656037ull;
    AppendHash(hash, BuildShaderVariantStableString(variant));
    return hash;
}

std::string BuildShaderCachePath(
    const ShaderSourceDesc& source,
    const ShaderCompileProfile& profile,
    const ShaderVariantKey& variant)
{
    std::ostringstream output;
    output << SanitizeToken(profile.cacheNamespace) << '/'
           << SanitizeToken(profile.targetPlatform) << '/'
           << SanitizeToken(profile.backend) << '/'
           << ToString(profile.optimization) << '/'
           << SanitizeToken(source.sourceId) << '/'
           << ToString(source.stage) << '/'
           << ToString(source.language) << '/'
           << SanitizeToken(source.entryPoint) << '/'
           << Hex(BuildShaderVariantStableHash(variant))
           << ".sagashader";
    return output.str();
}

} // namespace SagaEngine::Render
