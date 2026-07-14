/// @file ScriptPackageValidator.cpp
/// @brief Implements packaged SagaScript artifact validation.

#include <SagaEngine/Scripting/ScriptPackageValidator.hpp>

#include "SagaShared/Diagnostics/DiagnosticCategory.hpp"
#include "SagaShared/Diagnostics/DiagnosticSeverity.hpp"
#include "SagaShared/Diagnostics/DiagnosticSource.hpp"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SagaEngine::Scripting
{

namespace
{

constexpr std::uint32_t kSupportedSchemaVersion = 1;
constexpr std::string_view kSha256Prefix = "sha256:";
constexpr std::size_t kSha256HexLength = 64;
constexpr std::string_view kCapabilityManifestFileName =
    "script_capabilities.json";

struct CapabilityManifestEntry
{
    const nlohmann::json* json = nullptr;
};

/// Build a deterministic script host validation diagnostic.
[[nodiscard]] ScriptDiagnostic MakeDiagnostic(
    std::string code,
    std::string title,
    std::string message)
{
    ScriptDiagnostic diagnostic;
    diagnostic.diagnostic.severity =
        SagaShared::Diagnostics::DiagnosticSeverity::Error;
    diagnostic.diagnostic.category =
        SagaShared::Diagnostics::DiagnosticCategory::Script;
    diagnostic.diagnostic.source =
        SagaShared::Diagnostics::DiagnosticSource::Runtime;
    diagnostic.diagnostic.code.value = std::move(code);
    diagnostic.diagnostic.title = std::move(title);
    diagnostic.diagnostic.message = std::move(message);
    return diagnostic;
}

/// Return the absolute normalized package root for path policy checks.
[[nodiscard]] std::filesystem::path NormalizeRoot(
    const std::filesystem::path& packageRoot)
{
    return std::filesystem::absolute(packageRoot).lexically_normal();
}

/// Return whether a path is outside the normalized package root.
[[nodiscard]] bool PathEscapesRoot(
    const std::filesystem::path& packageRoot,
    const std::filesystem::path& candidate)
{
    const auto absoluteRoot = NormalizeRoot(packageRoot);
    const auto absolutePath = std::filesystem::absolute(candidate).lexically_normal();
    const auto relative = absolutePath.lexically_relative(absoluteRoot);
    if (relative.empty())
    {
        return false;
    }

    const auto first = *relative.begin();
    return first == ".." || first.empty();
}

/// Resolve a package-relative path and preserve absolute paths for validation.
[[nodiscard]] std::filesystem::path ResolvePackagePath(
    const std::filesystem::path& packageRoot,
    const std::filesystem::path& path)
{
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }

    return (packageRoot / path).lexically_normal();
}

/// Return true when a JSON field exists and is a string.
[[nodiscard]] bool HasStringField(
    const nlohmann::json& object,
    std::string_view field)
{
    const auto iterator = object.find(field);
    return iterator != object.end() && iterator->is_string();
}

/// Read a required string field after the caller has validated it.
[[nodiscard]] std::string ReadStringField(
    const nlohmann::json& object,
    std::string_view field)
{
    return object.at(std::string(field)).get<std::string>();
}

/// Return an array of strings when the field has the expected shape.
[[nodiscard]] std::optional<std::vector<std::string>> ReadStringArrayField(
    const nlohmann::json& object,
    std::string_view field)
{
    const auto iterator = object.find(field);
    if (iterator == object.end() || !iterator->is_array())
    {
        return std::nullopt;
    }

    std::vector<std::string> values;
    for (const auto& value : *iterator)
    {
        if (!value.is_string())
        {
            return std::nullopt;
        }

        values.push_back(value.get<std::string>());
    }

    std::sort(values.begin(), values.end());
    return values;
}

[[nodiscard]] std::vector<std::string> UniqueSortedStrings(
    std::vector<std::string> values)
{
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

/// Add artifact context metadata to a diagnostic.
void AddArtifactMetadata(
    ScriptDiagnostic& diagnostic,
    std::size_t artifactIndex,
    std::string_view field,
    const std::filesystem::path& path)
{
    diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
    diagnostic.metadata["field"] = std::string(field);
    diagnostic.metadata["path"] = path.generic_string();
}

/// Return true when the artifact destination is allowed for the expected package.
[[nodiscard]] bool DestinationAllowed(
    std::string_view expectedDestination,
    std::string_view artifactDestination)
{
    return expectedDestination.empty() ||
           artifactDestination == expectedDestination ||
           artifactDestination == "shared";
}

/// Return true when the authority token conflicts with the expected destination.
[[nodiscard]] bool AuthorityConflictsWithDestination(
    std::string_view expectedDestination,
    std::string_view authority)
{
    return (expectedDestination == "server" && authority == "ClientOnly") ||
           (expectedDestination == "client" && authority == "ServerOnly");
}

/// Return true when the manifest schema version is supported.
[[nodiscard]] bool HasSupportedSchemaVersion(const nlohmann::json& root)
{
    const auto iterator = root.find("schemaVersion");
    if (iterator == root.end() ||
        (!iterator->is_number_integer() && !iterator->is_number_unsigned()))
    {
        return false;
    }

    if (iterator->is_number_unsigned())
    {
        return iterator->get<std::uint64_t>() == kSupportedSchemaVersion;
    }

    return iterator->get<std::int64_t>() ==
           static_cast<std::int64_t>(kSupportedSchemaVersion);
}

/// Validate a path field that must stay package-relative and exist.
[[nodiscard]] std::optional<std::filesystem::path> ValidateExistingArtifactPath(
    const std::filesystem::path& packageRoot,
    const nlohmann::json& artifact,
    std::string_view field,
    std::size_t artifactIndex,
    ScriptPackageValidationResult& result)
{
    if (!HasStringField(artifact, field))
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ManifestInvalid,
            "Invalid script artifact manifest",
            "Script artifact entry is missing a required path field.");
        diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
        diagnostic.metadata["field"] = std::string(field);
        result.diagnostics.push_back(std::move(diagnostic));
        return std::nullopt;
    }

    const std::filesystem::path artifactPath(ReadStringField(artifact, field));
    if (artifactPath.empty() || artifactPath.is_absolute())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::InvalidArtifactPath,
            "Invalid script artifact path",
            "Script artifact paths must be package-relative.");
        AddArtifactMetadata(diagnostic, artifactIndex, field, artifactPath);
        result.diagnostics.push_back(std::move(diagnostic));
        return std::nullopt;
    }

    const auto resolvedPath = ResolvePackagePath(packageRoot, artifactPath);
    if (PathEscapesRoot(packageRoot, resolvedPath))
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::InvalidArtifactPath,
            "Invalid script artifact path",
            "Script artifact path must not escape the package root.");
        AddArtifactMetadata(diagnostic, artifactIndex, field, resolvedPath);
        result.diagnostics.push_back(std::move(diagnostic));
        return std::nullopt;
    }

    if (!std::filesystem::exists(resolvedPath))
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ArtifactMissing,
            "Script artifact file missing",
            "Script artifact manifest references a file that does not exist.");
        AddArtifactMetadata(diagnostic, artifactIndex, field, resolvedPath);
        result.diagnostics.push_back(std::move(diagnostic));
        return std::nullopt;
    }

    return resolvedPath;
}

/// Validate the root target framework without changing the manifest schema.
void ValidateTargetFramework(
    const nlohmann::json& root,
    const ScriptPackageValidationOptions& options,
    const std::filesystem::path& manifestPath,
    ScriptPackageValidationResult& result)
{
    if (options.expectedTargetFramework.empty())
    {
        return;
    }

    if (!HasStringField(root, "targetFramework") ||
        ReadStringField(root, "targetFramework") !=
            options.expectedTargetFramework)
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::UnsupportedTargetFramework,
            "Unsupported script target framework",
            "Script artifact manifest targetFramework does not match the runtime policy.");
        diagnostic.metadata["manifestPath"] = manifestPath.generic_string();
        diagnostic.metadata["expectedTargetFramework"] =
            options.expectedTargetFramework;
        if (HasStringField(root, "targetFramework"))
        {
            diagnostic.metadata["targetFramework"] =
                ReadStringField(root, "targetFramework");
        }
        result.diagnostics.push_back(std::move(diagnostic));
    }
}

[[nodiscard]] bool IsLowerAsciiHexDigit(const char value)
{
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f');
}

[[nodiscard]] bool IsAsciiHexDigit(const char value)
{
    return IsLowerAsciiHexDigit(value) || (value >= 'A' && value <= 'F');
}

[[nodiscard]] std::string ToLowerAscii(std::string value)
{
    for (char& character : value)
    {
        character = static_cast<char>(std::tolower(
            static_cast<unsigned char>(character)));
    }
    return value;
}

[[nodiscard]] std::optional<std::string> ParseSha256Hex(std::string_view hash)
{
    if (hash.size() != kSha256Prefix.size() + kSha256HexLength ||
        hash.substr(0, kSha256Prefix.size()) != kSha256Prefix)
    {
        return std::nullopt;
    }

    const auto hex = hash.substr(kSha256Prefix.size());
    for (const char character : hex)
    {
        if (!IsAsciiHexDigit(character))
        {
            return std::nullopt;
        }
    }

    return ToLowerAscii(std::string(hex));
}

[[nodiscard]] std::optional<std::string> ComputeSha256Hex(
    const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        return std::nullopt;
    }

    using EvpContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    EvpContext context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!context ||
        EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
    {
        return std::nullopt;
    }

    std::array<char, 8192> buffer{};
    while (input.good())
    {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto readCount = input.gcount();
        if (readCount > 0 &&
            EVP_DigestUpdate(
                context.get(),
                buffer.data(),
                static_cast<std::size_t>(readCount)) != 1)
        {
            return std::nullopt;
        }
    }

    if (input.bad())
    {
        return std::nullopt;
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &digestLength) != 1 ||
        digestLength != 32)
    {
        return std::nullopt;
    }

    constexpr char kHex[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(kSha256HexLength);
    for (unsigned int index = 0; index < digestLength; ++index)
    {
        const auto byte = digest[index];
        hex.push_back(kHex[(byte >> 4) & 0x0F]);
        hex.push_back(kHex[byte & 0x0F]);
    }
    return hex;
}

/// Validate artifact hash metadata against the referenced script assembly.
void ValidateArtifactHash(
    const nlohmann::json& artifact,
    const std::optional<std::filesystem::path>& assemblyPath,
    std::size_t artifactIndex,
    ScriptPackageValidationResult& result)
{
    if (!HasStringField(artifact, "hash"))
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::InvalidArtifactHash,
            "Invalid script artifact hash",
            "Script artifact entry must include a hash string.");
        diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
        diagnostic.metadata["field"] = "hash";
        result.diagnostics.push_back(std::move(diagnostic));
        return;
    }

    const auto hash = ReadStringField(artifact, "hash");
    const auto expectedHash = ParseSha256Hex(hash);
    if (!expectedHash.has_value())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::InvalidArtifactHash,
            "Invalid script artifact hash",
            "Script artifact hash must use sha256: followed by 64 hexadecimal characters.");
        diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
        diagnostic.metadata["field"] = "hash";
        diagnostic.metadata["hash"] = hash;
        result.diagnostics.push_back(std::move(diagnostic));
        return;
    }

    if (!assemblyPath.has_value())
    {
        return;
    }

    const auto actualHash = ComputeSha256Hex(*assemblyPath);
    if (!actualHash.has_value())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ArtifactMissing,
            "Script artifact file unreadable",
            "Script artifact manifest references a file that could not be read.");
        AddArtifactMetadata(diagnostic, artifactIndex, "assemblyPath", *assemblyPath);
        result.diagnostics.push_back(std::move(diagnostic));
        return;
    }

    if (*actualHash != *expectedHash)
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ArtifactHashMismatch,
            "Script artifact hash mismatch",
            "Script artifact manifest hash does not match the referenced assembly file.");
        AddArtifactMetadata(diagnostic, artifactIndex, "hash", *assemblyPath);
        diagnostic.metadata["expectedHash"] =
            std::string(kSha256Prefix) + *expectedHash;
        diagnostic.metadata["actualHash"] =
            std::string(kSha256Prefix) + *actualHash;
        result.diagnostics.push_back(std::move(diagnostic));
    }
}

/// Validate requested and granted capability arrays.
void ValidateCapabilityMetadata(
    const nlohmann::json& artifact,
    std::size_t artifactIndex,
    ScriptPackageValidationResult& result)
{
    for (const auto field : {"requestedCapabilities", "grantedCapabilities"})
    {
        if (ReadStringArrayField(artifact, field).has_value())
        {
            continue;
        }

        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::InvalidCapabilityMetadata,
            "Invalid script capability metadata",
            "Script artifact capability metadata must be arrays of strings.");
        diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
        diagnostic.metadata["field"] = field;
        result.diagnostics.push_back(std::move(diagnostic));
    }
}

void AddCapabilityGrantDiagnostic(
    ScriptPackageValidationResult& result,
    std::size_t artifactIndex,
    std::string_view scriptId,
    std::string capability,
    const char* code,
    std::string title,
    std::string message)
{
    auto diagnostic = MakeDiagnostic(
        code,
        std::move(title),
        std::move(message));
    diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
    diagnostic.metadata["scriptId"] = std::string(scriptId);
    diagnostic.metadata["capability"] = std::move(capability);
    diagnostic.metadata["field"] = "grantedCapabilities";
    result.diagnostics.push_back(std::move(diagnostic));
}

void ValidateCapabilityGrantPolicy(
    const nlohmann::json& artifacts,
    ScriptPackageValidationResult& result)
{
    for (std::size_t artifactIndex = 0; artifactIndex < artifacts.size();
         ++artifactIndex)
    {
        const auto& artifact = artifacts[artifactIndex];
        if (!artifact.is_object())
        {
            continue;
        }

        const auto requested =
            ReadStringArrayField(artifact, "requestedCapabilities");
        const auto granted =
            ReadStringArrayField(artifact, "grantedCapabilities");
        if (!requested.has_value() || !granted.has_value())
        {
            continue;
        }

        const auto requestedSet = UniqueSortedStrings(*requested);
        const auto grantedSet = UniqueSortedStrings(*granted);
        const auto scriptId = HasStringField(artifact, "scriptId")
            ? ReadStringField(artifact, "scriptId")
            : std::string{};

        for (const auto& capability : requestedSet)
        {
            if (std::binary_search(
                    grantedSet.begin(),
                    grantedSet.end(),
                    capability))
            {
                continue;
            }

            AddCapabilityGrantDiagnostic(
                result,
                artifactIndex,
                scriptId,
                capability,
                ScriptHostDiagnostics::CapabilityGrantMissing,
                "Script capability grant missing",
                "Script artifact requests a capability that was not granted.");
        }

        for (const auto& capability : grantedSet)
        {
            if (std::binary_search(
                    requestedSet.begin(),
                    requestedSet.end(),
                    capability))
            {
                continue;
            }

            AddCapabilityGrantDiagnostic(
                result,
                artifactIndex,
                scriptId,
                capability,
                ScriptHostDiagnostics::CapabilityGrantUnexpected,
                "Unexpected script capability grant",
                "Script artifact grants a capability that was not requested.");
        }
    }
}

void AddCapabilityManifestMetadata(
    ScriptDiagnostic& diagnostic,
    const std::filesystem::path& manifestPath,
    std::string_view field = {})
{
    diagnostic.metadata["manifestPath"] = manifestPath.generic_string();
    if (!field.empty())
    {
        diagnostic.metadata["field"] = std::string(field);
    }
}

[[nodiscard]] std::optional<nlohmann::json> LoadCapabilityManifest(
    const std::filesystem::path& manifestPath,
    ScriptPackageValidationResult& result)
{
    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::CapabilityManifestMissing,
            "Script capability manifest missing",
            "Script capability manifest could not be opened.");
        AddCapabilityManifestMetadata(diagnostic, manifestPath);
        result.diagnostics.push_back(std::move(diagnostic));
        return std::nullopt;
    }

    nlohmann::json root;
    try
    {
        input >> root;
    }
    catch (const nlohmann::json::exception& exception)
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::CapabilityManifestInvalid,
            "Invalid script capability manifest",
            std::string("Script capability manifest JSON parse failed: ") +
                exception.what());
        AddCapabilityManifestMetadata(diagnostic, manifestPath);
        result.diagnostics.push_back(std::move(diagnostic));
        return std::nullopt;
    }

    return root;
}

[[nodiscard]] bool ValidateCapabilityManifestShape(
    const nlohmann::json& root,
    const std::filesystem::path& manifestPath,
    ScriptPackageValidationResult& result)
{
    if (!root.is_object() ||
        !HasSupportedSchemaVersion(root) ||
        !root.contains("scripts") ||
        !root["scripts"].is_array() ||
        root["scripts"].empty())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::CapabilityManifestInvalid,
            "Invalid script capability manifest",
            "Script capability manifest must be schemaVersion 1 with a non-empty scripts array.");
        AddCapabilityManifestMetadata(diagnostic, manifestPath);
        result.diagnostics.push_back(std::move(diagnostic));
        return false;
    }

    bool valid = true;
    const auto& scripts = root["scripts"];
    for (std::size_t index = 0; index < scripts.size(); ++index)
    {
        const auto& script = scripts[index];
        if (!script.is_object())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::CapabilityManifestInvalid,
                "Invalid script capability manifest",
                "Script capability entries must be JSON objects.");
            AddCapabilityManifestMetadata(diagnostic, manifestPath, "scripts");
            diagnostic.metadata["scriptIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
            valid = false;
            continue;
        }

        for (const auto field :
             {"scriptId", "authority", "packageDestinationIntent"})
        {
            if (HasStringField(script, field) &&
                !ReadStringField(script, field).empty())
            {
                continue;
            }

            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::CapabilityManifestInvalid,
                "Invalid script capability manifest",
                "Script capability entry is missing required metadata.");
            AddCapabilityManifestMetadata(diagnostic, manifestPath, field);
            diagnostic.metadata["scriptIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
            valid = false;
        }

        for (const auto field : {"requestedCapabilities", "grantedCapabilities"})
        {
            if (ReadStringArrayField(script, field).has_value())
            {
                continue;
            }

            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::CapabilityManifestInvalid,
                "Invalid script capability manifest",
                "Script capability metadata must be arrays of strings.");
            AddCapabilityManifestMetadata(diagnostic, manifestPath, field);
            diagnostic.metadata["scriptIndex"] = std::to_string(index);
            result.diagnostics.push_back(std::move(diagnostic));
            valid = false;
        }
    }

    return valid;
}

[[nodiscard]] std::unordered_map<std::string, CapabilityManifestEntry>
BuildCapabilityEntryIndex(
    const nlohmann::json& root,
    const std::filesystem::path& manifestPath,
    ScriptPackageValidationResult& result)
{
    std::unordered_map<std::string, CapabilityManifestEntry> entries;
    const auto& scripts = root["scripts"];
    for (std::size_t index = 0; index < scripts.size(); ++index)
    {
        const auto& script = scripts[index];
        const auto scriptId = ReadStringField(script, "scriptId");
        if (entries.find(scriptId) != entries.end())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::CapabilityManifestMismatch,
                "Script capability manifest mismatch",
                "Script capability manifest contains duplicate entries for a script.");
            AddCapabilityManifestMetadata(diagnostic, manifestPath, "scriptId");
            diagnostic.metadata["scriptIndex"] = std::to_string(index);
            diagnostic.metadata["scriptId"] = scriptId;
            result.diagnostics.push_back(std::move(diagnostic));
            continue;
        }

        entries.emplace(scriptId, CapabilityManifestEntry{&script});
    }

    return entries;
}

void AddCapabilityMismatchDiagnostic(
    ScriptPackageValidationResult& result,
    const std::filesystem::path& manifestPath,
    std::size_t artifactIndex,
    std::string_view scriptId,
    std::string_view field,
    std::string_view message)
{
    auto diagnostic = MakeDiagnostic(
        ScriptHostDiagnostics::CapabilityManifestMismatch,
        "Script capability manifest mismatch",
        std::string(message));
    AddCapabilityManifestMetadata(diagnostic, manifestPath, field);
    diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
    diagnostic.metadata["scriptId"] = std::string(scriptId);
    result.diagnostics.push_back(std::move(diagnostic));
}

void ValidateCapabilityManifestConsistency(
    const nlohmann::json& artifacts,
    const std::filesystem::path& manifestPath,
    const std::unordered_map<std::string, CapabilityManifestEntry>& entries,
    ScriptPackageValidationResult& result)
{
    for (std::size_t artifactIndex = 0; artifactIndex < artifacts.size();
         ++artifactIndex)
    {
        const auto& artifact = artifacts[artifactIndex];
        if (!artifact.is_object() || !HasStringField(artifact, "scriptId") ||
            ReadStringField(artifact, "scriptId").empty())
        {
            continue;
        }

        const auto scriptId = ReadStringField(artifact, "scriptId");
        const auto entryIterator = entries.find(scriptId);
        if (entryIterator == entries.end())
        {
            AddCapabilityMismatchDiagnostic(
                result,
                manifestPath,
                artifactIndex,
                scriptId,
                "scriptId",
                "Script artifact has no matching capability manifest entry.");
            continue;
        }

        const auto& capability = *entryIterator->second.json;
        for (const auto field : {"authority", "packageDestinationIntent"})
        {
            if (!HasStringField(artifact, field) ||
                ReadStringField(artifact, field).empty())
            {
                continue;
            }

            if (ReadStringField(artifact, field) == ReadStringField(capability, field))
            {
                continue;
            }

            AddCapabilityMismatchDiagnostic(
                result,
                manifestPath,
                artifactIndex,
                scriptId,
                field,
                "Script artifact metadata does not match the capability manifest.");
        }

        for (const auto field : {"requestedCapabilities", "grantedCapabilities"})
        {
            const auto artifactValues = ReadStringArrayField(artifact, field);
            if (!artifactValues.has_value())
            {
                continue;
            }

            const auto capabilityValues = ReadStringArrayField(capability, field);
            if (capabilityValues.has_value() &&
                *artifactValues == *capabilityValues)
            {
                continue;
            }

            AddCapabilityMismatchDiagnostic(
                result,
                manifestPath,
                artifactIndex,
                scriptId,
                field,
                "Script artifact capabilities do not match the capability manifest.");
        }
    }
}

/// Validate authority metadata against package policy.
void ValidateAuthorityMetadata(
    const nlohmann::json& artifact,
    const ScriptPackageValidationOptions& options,
    std::size_t artifactIndex,
    ScriptPackageValidationResult& result)
{
    if (!HasStringField(artifact, "authority") ||
        ReadStringField(artifact, "authority").empty())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::InvalidAuthority,
            "Invalid script authority",
            "Script artifact entry must include non-empty authority metadata.");
        diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
        diagnostic.metadata["field"] = "authority";
        result.diagnostics.push_back(std::move(diagnostic));
        return;
    }

    if (!options.enforceAuthorityDestination ||
        options.expectedPackageDestination.empty())
    {
        return;
    }

    const auto authority = ReadStringField(artifact, "authority");
    if (AuthorityConflictsWithDestination(
            options.expectedPackageDestination,
            authority))
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::InvalidAuthority,
            "Script authority does not match package destination",
            "Script artifact authority conflicts with the expected package destination.");
        diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
        diagnostic.metadata["expectedPackageDestination"] =
            options.expectedPackageDestination;
        diagnostic.metadata["authority"] = authority;
        result.diagnostics.push_back(std::move(diagnostic));
    }
}

/// Validate package destination metadata.
bool ValidatePackageDestination(
    const nlohmann::json& artifact,
    const ScriptPackageValidationOptions& options,
    std::size_t artifactIndex,
    ScriptPackageValidationResult& result)
{
    if (!HasStringField(artifact, "packageDestinationIntent") ||
        ReadStringField(artifact, "packageDestinationIntent").empty())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::PackageDestinationMismatch,
            "Script package destination missing",
            "Script artifact entry must include packageDestinationIntent.");
        diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
        diagnostic.metadata["field"] = "packageDestinationIntent";
        result.diagnostics.push_back(std::move(diagnostic));
        return false;
    }

    const auto destination =
        ReadStringField(artifact, "packageDestinationIntent");
    if (!DestinationAllowed(options.expectedPackageDestination, destination))
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::PackageDestinationMismatch,
            "Script package destination mismatch",
            "Script artifact package destination does not match the expected runtime domain.");
        diagnostic.metadata["artifactIndex"] = std::to_string(artifactIndex);
        diagnostic.metadata["expectedPackageDestination"] =
            options.expectedPackageDestination;
        diagnostic.metadata["packageDestinationIntent"] = destination;
        result.diagnostics.push_back(std::move(diagnostic));
        return false;
    }

    return true;
}

} // namespace

ScriptPackageValidationResult ScriptPackageValidator::ValidateLoadRequest(
    const ScriptPackageLoadRequest& request,
    const ScriptPackageValidationOptions& options)
{
    ScriptPackageValidationResult result;

    if (request.packageRoot.empty() ||
        !std::filesystem::exists(request.packageRoot) ||
        !std::filesystem::is_directory(request.packageRoot))
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ManifestInvalid,
            "Invalid script package root",
            "Script package root must be an existing directory.");
        diagnostic.metadata["packageRoot"] = request.packageRoot.generic_string();
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    const auto packageRoot = NormalizeRoot(request.packageRoot);
    const auto manifestPath =
        ResolvePackagePath(packageRoot, request.scriptArtifactManifest);
    if (request.scriptArtifactManifest.empty() ||
        PathEscapesRoot(packageRoot, manifestPath))
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::InvalidArtifactPath,
            "Invalid script artifact manifest path",
            "Script artifact manifest path must stay inside the package root.");
        diagnostic.metadata["manifestPath"] = manifestPath.generic_string();
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    std::ifstream input(manifestPath);
    if (!input.is_open())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ManifestMissing,
            "Script artifact manifest missing",
            "Script artifact manifest could not be opened.");
        diagnostic.metadata["manifestPath"] = manifestPath.generic_string();
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    nlohmann::json root;
    try
    {
        input >> root;
    }
    catch (const nlohmann::json::exception& exception)
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ManifestInvalid,
            "Invalid script artifact manifest",
            std::string("Script artifact manifest JSON parse failed: ") +
                exception.what());
        diagnostic.metadata["manifestPath"] = manifestPath.generic_string();
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    if (!root.is_object() ||
        !HasSupportedSchemaVersion(root) ||
        !root.contains("artifacts") ||
        !root["artifacts"].is_array())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ManifestInvalid,
            "Invalid script artifact manifest",
            "Script artifact manifest must be schemaVersion 1 with an artifacts array.");
        diagnostic.metadata["manifestPath"] = manifestPath.generic_string();
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    ValidateTargetFramework(root, options, manifestPath, result);

    const auto& artifacts = root["artifacts"];
    if (artifacts.empty())
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ManifestInvalid,
            "Invalid script artifact manifest",
            "Script artifact manifest must contain at least one artifact.");
        diagnostic.metadata["manifestPath"] = manifestPath.generic_string();
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    const auto capabilityManifestPath =
        manifestPath.parent_path() / kCapabilityManifestFileName;
    const auto capabilityManifest =
        LoadCapabilityManifest(capabilityManifestPath, result);
    if (capabilityManifest.has_value() &&
        ValidateCapabilityManifestShape(
            *capabilityManifest,
            capabilityManifestPath,
            result))
    {
        const auto capabilityEntries = BuildCapabilityEntryIndex(
            *capabilityManifest,
            capabilityManifestPath,
            result);
        ValidateCapabilityManifestConsistency(
            artifacts,
            capabilityManifestPath,
            capabilityEntries,
            result);
    }

    for (std::size_t index = 0; index < artifacts.size(); ++index)
    {
        const auto& artifact = artifacts[index];
        if (!artifact.is_object())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::ManifestInvalid,
                "Invalid script artifact manifest",
                "Script artifact entries must be JSON objects.");
            diagnostic.metadata["artifactIndex"] = std::to_string(index);
                result.diagnostics.push_back(std::move(diagnostic));
                continue;
        }

        ValidateCapabilityMetadata(artifact, index, result);
        ValidateAuthorityMetadata(artifact, options, index, result);

        const auto assemblyPath = ValidateExistingArtifactPath(
            packageRoot,
            artifact,
            "assemblyPath",
            index,
            result);

        if (options.requireRuntimeConfig)
        {
            static_cast<void>(ValidateExistingArtifactPath(
                packageRoot,
                artifact,
                "runtimeConfigPath",
                index,
                result));
        }

        ValidateArtifactHash(artifact, assemblyPath, index, result);

        static_cast<void>(ValidatePackageDestination(
            artifact,
            options,
            index,
            result));
    }

    if (result.diagnostics.empty())
    {
        ValidateCapabilityGrantPolicy(artifacts, result);
    }

    result.artifactCount = artifacts.size();
    result.succeeded = result.diagnostics.empty();
    return result;
}

} // namespace SagaEngine::Scripting
