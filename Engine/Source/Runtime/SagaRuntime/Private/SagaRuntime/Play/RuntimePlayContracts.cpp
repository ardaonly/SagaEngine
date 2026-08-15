// SPDX-License-Identifier: MPL-2.0
#include "SagaRuntime/Play/RuntimePlayContracts.hpp"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <tuple>
namespace SagaRuntime::Play
{
namespace
{
using Json = nlohmann::json;
RuntimeDiagnostic Error(std::string code, std::string message, std::string subject = {})
{ return {std::move(code), std::move(message), std::move(subject)}; }

std::string ReadFile(const std::filesystem::path& path, std::error_code& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) { error = std::make_error_code(std::errc::no_such_file_or_directory); return {}; }
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (!input.good() && !input.eof())
    { error = std::make_error_code(std::errc::io_error); return {}; }
    error.clear();
    return bytes.str();
}

std::string Canonical(const Json& value) { return value.dump(2) + "\n"; }

bool ExactFields(const Json& value, std::initializer_list<std::string_view> allowed)
{
    if (!value.is_object()) return false;
    const std::set<std::string_view> names(allowed);
    for (auto iterator = value.cbegin(); iterator != value.cend(); ++iterator)
        if (!names.contains(iterator.key())) return false;
    return true;
}

bool NonEmptyString(const Json& value, std::string_view name)
{
    const auto iterator = value.find(std::string(name));
    return iterator != value.end() && iterator->is_string() && !iterator->get_ref<const std::string&>().empty();
}

template <std::size_t N>
bool NumberArray(const Json& value, std::string_view name, std::array<double, N>& output)
{
    const auto iterator = value.find(std::string(name));
    if (iterator == value.end() || !iterator->is_array() || iterator->size() != N) return false;
    for (std::size_t index = 0; index < N; ++index)
    {
        if (!(*iterator)[index].is_number()) return false;
        output[index] = (*iterator)[index].get<double>();
        if (!std::isfinite(output[index])) return false;
    }
    return true;
}

bool Contained(const std::filesystem::path& root, const std::filesystem::path& path)
{
    std::error_code error;
    const auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
    if (error) return false;
    const auto canonicalPath = std::filesystem::weakly_canonical(path, error);
    if (error) return false;
    const auto relative = canonicalPath.lexically_relative(canonicalRoot);
    return !relative.empty() && !relative.is_absolute() &&
        *relative.begin() != std::filesystem::path("..");
}

bool ArtifactValid(const ArtifactIdentity& value)
{ return !value.logicalId.empty() && IsSha256(value.hash); }

Json ArtifactJson(const ArtifactIdentity& value)
{ return {{"hash", value.hash}, {"logicalId", value.logicalId}}; }

ArtifactIdentity ParseArtifact(const Json& parent, std::string_view name,
                               std::vector<RuntimeDiagnostic>& diagnostics)
{
    const auto iterator = parent.find(std::string(name));
    if (iterator == parent.end() || !ExactFields(*iterator, {"hash", "logicalId"}) ||
        !NonEmptyString(*iterator, "logicalId") || !NonEmptyString(*iterator, "hash"))
    {
        diagnostics.push_back(Error("Runtime.Launch.ArtifactInvalid",
            "Artifact identity requires only logicalId and SHA-256 hash.", std::string(name)));
        return {};
    }
    ArtifactIdentity result{iterator->at("logicalId").get<std::string>(),
                            iterator->at("hash").get<std::string>()};
    if (!ArtifactValid(result)) diagnostics.push_back(Error("Runtime.Launch.ArtifactInvalid",
        "Artifact identity or hash is malformed.", std::string(name)));
    return result;
}

template <typename T>
ArtifactLoadResult<T> ArtifactFailure(std::string code, std::string message,
                                      const std::filesystem::path& path)
{
    ArtifactLoadResult<T> result;
    result.diagnostics.push_back(Error(std::move(code), std::move(message), path.generic_string()));
    return result;
}

bool ValidateWrapper(const Json& root, std::string_view type,
                     std::vector<RuntimeDiagnostic>& diagnostics)
{
    if (!ExactFields(root, {"artifactId", "artifactType", "contentHash", "cooker",
                            "dependencies", "payload", "provenance", "schemaVersion", "sourceHash"}) ||
        (root.value("schemaVersion", -1) != 1 &&
         root.value("schemaVersion", -1) != 2) ||
        root.value("artifactType", std::string{}) != type ||
        !NonEmptyString(root, "artifactId") || !root.contains("payload") || !root["payload"].is_object() ||
        !IsSha256(root.value("contentHash", std::string{})) ||
        !IsSha256(root.value("sourceHash", std::string{})))
    {
        diagnostics.push_back(Error("Runtime.Artifact.SchemaInvalid",
            "Runtime artifact wrapper schema, type, identity, or hash token is invalid.", std::string(type)));
        return false;
    }
    if (Sha256Bytes(Canonical(root["payload"])) != root["contentHash"].get<std::string>())
    {
        diagnostics.push_back(Error("Runtime.Artifact.ContentHashMismatch",
            "Runtime artifact payload does not match contentHash.", root["artifactId"].get<std::string>()));
        return false;
    }
    return true;
}
} // namespace

bool IsSha256(std::string_view value) noexcept
{
    return value.size() == 71 && value.starts_with("sha256:") &&
        std::all_of(value.begin() + 7, value.end(), [](unsigned char character)
        { return (character >= '0' && character <= '9') ||
                 (character >= 'a' && character <= 'f'); });
}

std::string Sha256Bytes(std::string_view bytes)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), digest.data(), &length, EVP_sha256(), nullptr) != 1)
        return {};
    std::ostringstream output;
    output << "sha256:" << std::hex << std::setfill('0');
    for (unsigned int index = 0; index < length; ++index)
        output << std::setw(2) << static_cast<unsigned int>(digest[index]);
    return output.str();
}

std::string SerializeCanonical(const CanonicalRuntimeLaunchManifest& manifest)
{
    Json root = {
        {"artifacts", {
            {"behaviors", ArtifactJson(manifest.behaviors)},
            {"capabilities", ArtifactJson(manifest.capabilities)},
            {"input", ArtifactJson(manifest.input)},
            {"namedActions", ArtifactJson(manifest.namedActions)},
            {"packageGeneration", ArtifactJson(manifest.packageGeneration)},
            {"scene", ArtifactJson(manifest.scene)},
            {"scriptAssembly", ArtifactJson(manifest.scriptAssembly)},
            {"scriptAssemblyManifest", ArtifactJson(manifest.scriptAssemblyManifest)},
            {"scriptReferenceManifest", ArtifactJson(manifest.scriptReferenceManifest)},
            {"scriptRuntimeConfig", ArtifactJson(manifest.scriptRuntimeConfig)},
        }},
        {"playSessionId", manifest.playSessionId},
        {"packageGraphHash", manifest.packageGraphHash},
        {"projectHash", manifest.projectHash},
        {"projectId", manifest.projectId},
        {"requiredRuntimeCapabilities", manifest.requiredRuntimeCapabilities},
        {"runtimeBridge", {
            {"abi", manifest.runtimeBridge.abi},
            {"assemblyHash", manifest.runtimeBridge.assemblyHash},
            {"logicalReferenceId", manifest.runtimeBridge.logicalReferenceId},
            {"runtimeCompatibility", manifest.runtimeBridge.runtimeCompatibility},
            {"targetFramework", manifest.runtimeBridge.targetFramework},
            {"trust", manifest.runtimeBridge.trust},
        }},
        {"runtimeProfile", manifest.runtimeProfile},
        {"schemaVersion", manifest.schemaVersion},
        {"scriptSourceRevision", manifest.scriptSourceRevision},
        {"scriptSourceSetHash", manifest.scriptSourceSetHash},
        {"toolVersions", manifest.toolVersions},
    };
    auto capabilities = manifest.requiredRuntimeCapabilities;
    std::sort(capabilities.begin(), capabilities.end());
    capabilities.erase(std::unique(capabilities.begin(), capabilities.end()), capabilities.end());
    root["requiredRuntimeCapabilities"] = capabilities;
    if (manifest.schemaVersion >= 2)
        root["artifacts"]["uiSet"] = ArtifactJson(manifest.uiSet);
    if (manifest.schemaVersion == 3)
    {
        root["artifacts"]["behaviorModules"] = ArtifactJson(manifest.behaviorModules);
        root["artifacts"]["behaviorArtifacts"] = ArtifactJson(manifest.behaviorArtifacts);
    }
    return Canonical(root);
}

CanonicalManifestResult ParseCanonicalManifest(std::string_view bytes,
                                                std::string_view expectedHash)
{
    CanonicalManifestResult result;
    result.hash = Sha256Bytes(bytes);
    if (!expectedHash.empty() && (!IsSha256(expectedHash) || result.hash != expectedHash))
        result.diagnostics.push_back(Error("Runtime.Launch.ManifestHashMismatch",
            "Canonical launch manifest bytes do not match the expected hash."));
    Json root;
    try { root = Json::parse(bytes); }
    catch (const Json::exception& error)
    {
        result.diagnostics.push_back(Error("Runtime.Launch.ManifestJsonInvalid", error.what()));
        return result;
    }
    const auto schemaVersion = root.value("schemaVersion", -1);
    if (!ExactFields(root, {"artifacts", "packageGraphHash", "playSessionId", "projectHash", "projectId",
                            "requiredRuntimeCapabilities", "runtimeBridge", "runtimeProfile",
                            "schemaVersion", "scriptSourceRevision", "scriptSourceSetHash", "toolVersions"}) ||
        (schemaVersion != 1 && schemaVersion != 2 && schemaVersion != 3) ||
        !NonEmptyString(root, "projectId") || !NonEmptyString(root, "playSessionId") ||
        !IsSha256(root.value("projectHash", std::string{})) ||
        !IsSha256(root.value("packageGraphHash", std::string{})) ||
        !IsSha256(root.value("scriptSourceRevision", std::string{})) ||
        !IsSha256(root.value("scriptSourceSetHash", std::string{})) ||
        root.value("runtimeProfile", std::string{}) != "Client" ||
        !root.contains("artifacts") || !root["artifacts"].is_object() ||
        !root.contains("runtimeBridge") || !root["runtimeBridge"].is_object() ||
        !root.contains("requiredRuntimeCapabilities") || !root["requiredRuntimeCapabilities"].is_array() ||
        !root.contains("toolVersions") || !root["toolVersions"].is_object())
    {
        result.diagnostics.push_back(Error("Runtime.Launch.ManifestSchemaInvalid",
            "Canonical runtime launch manifest schema version 1, 2, or 3 is required."));
        return result;
    }
    CanonicalRuntimeLaunchManifest manifest;
    manifest.schemaVersion = static_cast<std::uint32_t>(schemaVersion);
    manifest.projectId = root["projectId"].get<std::string>();
    manifest.projectHash = root["projectHash"].get<std::string>();
    manifest.packageGraphHash = root["packageGraphHash"].get<std::string>();
    manifest.scriptSourceRevision = root["scriptSourceRevision"].get<std::string>();
    manifest.scriptSourceSetHash = root["scriptSourceSetHash"].get<std::string>();
    manifest.playSessionId = root["playSessionId"].get<std::string>();
    manifest.runtimeProfile = root["runtimeProfile"].get<std::string>();
    const auto& artifacts = root["artifacts"];
    const bool artifactFieldsValid = schemaVersion == 1
        ? ExactFields(artifacts, {"behaviors", "capabilities", "input", "namedActions",
                                 "packageGeneration", "scene", "scriptAssembly",
                                 "scriptAssemblyManifest", "scriptReferenceManifest",
                                 "scriptRuntimeConfig"})
        : schemaVersion == 2
        ? ExactFields(artifacts, {"behaviors", "capabilities", "input", "namedActions",
                                 "packageGeneration", "scene", "scriptAssembly",
                                 "scriptAssemblyManifest", "scriptReferenceManifest",
                                 "scriptRuntimeConfig", "uiSet"})
        : ExactFields(artifacts, {"behaviorArtifacts", "behaviorModules", "behaviors",
                                 "capabilities", "input", "namedActions", "packageGeneration",
                                 "scene", "scriptAssembly", "scriptAssemblyManifest",
                                 "scriptReferenceManifest", "scriptRuntimeConfig", "uiSet"});
    if (!artifactFieldsValid)
        result.diagnostics.push_back(Error("Runtime.Launch.ArtifactSetInvalid",
            "The canonical artifact set is incomplete or contains unknown fields."));
    manifest.behaviors = ParseArtifact(artifacts, "behaviors", result.diagnostics);
    if (schemaVersion == 3)
    {
        manifest.behaviorModules = ParseArtifact(
            artifacts, "behaviorModules", result.diagnostics);
        manifest.behaviorArtifacts = ParseArtifact(
            artifacts, "behaviorArtifacts", result.diagnostics);
    }
    manifest.capabilities = ParseArtifact(artifacts, "capabilities", result.diagnostics);
    manifest.input = ParseArtifact(artifacts, "input", result.diagnostics);
    if (schemaVersion >= 2)
        manifest.uiSet = ParseArtifact(artifacts, "uiSet", result.diagnostics);
    manifest.namedActions = ParseArtifact(artifacts, "namedActions", result.diagnostics);
    manifest.packageGeneration = ParseArtifact(artifacts, "packageGeneration", result.diagnostics);
    manifest.scene = ParseArtifact(artifacts, "scene", result.diagnostics);
    manifest.scriptAssembly = ParseArtifact(artifacts, "scriptAssembly", result.diagnostics);
    manifest.scriptAssemblyManifest = ParseArtifact(
        artifacts, "scriptAssemblyManifest", result.diagnostics);
    manifest.scriptReferenceManifest = ParseArtifact(
        artifacts, "scriptReferenceManifest", result.diagnostics);
    manifest.scriptRuntimeConfig = ParseArtifact(
        artifacts, "scriptRuntimeConfig", result.diagnostics);
    const auto& bridge = root["runtimeBridge"];
    if (!ExactFields(bridge, {"abi", "assemblyHash", "logicalReferenceId",
                              "runtimeCompatibility", "targetFramework", "trust"}) ||
        !NonEmptyString(bridge, "abi") || !NonEmptyString(bridge, "logicalReferenceId") ||
        !NonEmptyString(bridge, "targetFramework") || !NonEmptyString(bridge, "runtimeCompatibility") ||
        bridge.value("abi", std::string{}) != "NativeCallbackTable.v3+NamedAction.v1" ||
        bridge.value("targetFramework", std::string{}) != "net10.0" ||
        bridge.value("runtimeCompatibility", std::string{}) != "0.0.11" ||
        bridge.value("trust", std::string{}) != "TrustedEngineDistribution" ||
        !IsSha256(bridge.value("assemblyHash", std::string{})))
        result.diagnostics.push_back(Error("Runtime.Launch.RuntimeBridgeInvalid",
            "RuntimeBridge identity, ABI, compatibility, trust, or binary hash is invalid."));
    else manifest.runtimeBridge = {bridge["logicalReferenceId"].get<std::string>(),
        bridge["assemblyHash"].get<std::string>(), bridge["abi"].get<std::string>(),
        bridge["targetFramework"].get<std::string>(), bridge["runtimeCompatibility"].get<std::string>(),
        bridge["trust"].get<std::string>()};
    for (const auto& capability : root["requiredRuntimeCapabilities"])
        if (capability.is_string() && !capability.get_ref<const std::string&>().empty())
            manifest.requiredRuntimeCapabilities.push_back(capability.get<std::string>());
        else result.diagnostics.push_back(Error("Runtime.Launch.CapabilityInvalid",
            "Required runtime capabilities must be non-empty strings."));
    for (const auto& [name, version] : root["toolVersions"].items())
        if (version.is_string() && !name.empty() && !version.get_ref<const std::string&>().empty())
            manifest.toolVersions.emplace(name, version.get<std::string>());
        else result.diagnostics.push_back(Error("Runtime.Launch.ToolVersionInvalid",
            "Tool version entries must be non-empty strings.", name));
    result.canonicalBytes = SerializeCanonical(manifest);
    if (result.canonicalBytes != bytes)
        result.diagnostics.push_back(Error("Runtime.Launch.ManifestNotCanonical",
            "Runtime launch manifest must use canonical UTF-8/LF JSON bytes."));
    if (result.diagnostics.empty()) result.manifest = std::move(manifest);
    return result;
}

std::string SerializeRuntimeStartupReadyV1(const RuntimeStartupReadyV1& ready)
{
    return Canonical({{"canonicalLaunchManifestHash", ready.canonicalLaunchManifestHash},
        {"playSessionId", ready.playSessionId}, {"schemaVersion", ready.schemaVersion},
        {"status", ready.status}});
}

RuntimeStartupReadyParseResult ParseRuntimeStartupReadyV1(
    std::string_view bytes, std::string_view expectedPlaySessionId,
    std::string_view expectedCanonicalManifestHash)
{
    RuntimeStartupReadyParseResult parsed;
    Json root;
    try { root = Json::parse(bytes); }
    catch (const Json::exception& error)
    {
        parsed.diagnostics.push_back(Error("Runtime.StartupReady.JsonInvalid", error.what()));
        return parsed;
    }
    if (!ExactFields(root, {"canonicalLaunchManifestHash", "playSessionId",
                            "schemaVersion", "status"}) ||
        root.value("schemaVersion", -1) != 1 || !NonEmptyString(root, "playSessionId") ||
        !NonEmptyString(root, "canonicalLaunchManifestHash") ||
        root.value("status", std::string{}) != "Ready" ||
        !IsSha256(root.value("canonicalLaunchManifestHash", std::string{})))
    {
        parsed.diagnostics.push_back(Error("Runtime.StartupReady.SchemaInvalid",
            "Startup-ready transport must satisfy the complete schema-version-1 contract."));
        return parsed;
    }
    RuntimeStartupReadyV1 ready;
    ready.playSessionId = root["playSessionId"].get<std::string>();
    ready.canonicalLaunchManifestHash = root["canonicalLaunchManifestHash"].get<std::string>();
    ready.status = root["status"].get<std::string>();
    if (!expectedPlaySessionId.empty() && ready.playSessionId != expectedPlaySessionId)
        parsed.diagnostics.push_back(Error("Runtime.StartupReady.PlaySessionMismatch",
            "Startup-ready record belongs to a different play session.", ready.playSessionId));
    if (!expectedCanonicalManifestHash.empty() &&
        ready.canonicalLaunchManifestHash != expectedCanonicalManifestHash)
        parsed.diagnostics.push_back(Error("Runtime.StartupReady.ManifestHashMismatch",
            "Startup-ready record belongs to a different canonical launch manifest."));
    if (parsed.diagnostics.empty()) parsed.ready = std::move(ready);
    return parsed;
}
RuntimeResultParseResult ParseRuntimeResultV1(std::string_view bytes,
    std::string_view expectedPlaySessionId, std::string_view expectedCanonicalManifestHash)
{
    RuntimeResultParseResult parsed;
    Json root;
    try { root = Json::parse(bytes); }
    catch (const Json::exception& error)
    {
        parsed.diagnostics.push_back(Error("Runtime.Result.JsonInvalid", error.what()));
        return parsed;
    }
    if (!ExactFields(root, {"canonicalGameplayState", "canonicalGameplayStateHash",
            "canonicalLaunchManifestHash", "crashArtifactPath",
            "exitClassification", "exitCode", "framesExecuted", "interactionActionCount",
            "logPath", "observedState", "playSessionId", "runtimeDiagnostics",
            "schemaVersion", "shutdownClassification", "startupDiagnostics", "status"}) ||
        root.value("schemaVersion", -1) != 1 || !NonEmptyString(root, "playSessionId") ||
        !NonEmptyString(root, "canonicalLaunchManifestHash") || !NonEmptyString(root, "status") ||
        !NonEmptyString(root, "exitClassification") || !NonEmptyString(root, "shutdownClassification") ||
        !root.contains("exitCode") || !root["exitCode"].is_number_integer() ||
        !root.contains("framesExecuted") || !root["framesExecuted"].is_number_unsigned() ||
        !root.contains("interactionActionCount") || !root["interactionActionCount"].is_number_unsigned() ||
        !root.contains("canonicalGameplayState") || !root["canonicalGameplayState"].is_string() ||
        !IsSha256(root.value("canonicalGameplayStateHash", std::string{})) ||
        !root.contains("observedState") || !root["observedState"].is_object() ||
        !root.contains("startupDiagnostics") || !root["startupDiagnostics"].is_array() ||
        !root.contains("runtimeDiagnostics") || !root["runtimeDiagnostics"].is_array() ||
        !IsSha256(root.value("canonicalLaunchManifestHash", std::string{})))
    {
        parsed.diagnostics.push_back(Error("Runtime.Result.SchemaInvalid",
            "Runtime result must satisfy the complete schema-version-1 contract."));
        return parsed;
    }
    const auto gameplay = ParseCanonicalGameplayState(root["canonicalGameplayState"].get_ref<const std::string&>(),
        root.value("canonicalGameplayStateHash", std::string{}));
    if (!gameplay.Valid())
    {
        parsed.diagnostics.push_back(Error("Runtime.Result.GameplayStateInvalid",
            gameplay.diagnostics.empty() ? "Canonical gameplay state is invalid." :
                                           gameplay.diagnostics.front().message));
        return parsed;
    }
    if (root["status"] == "Succeeded" && (!gameplay.state->scenarioIdentity ||
        !gameplay.state->packageGenerationId || !gameplay.state->packageGenerationHash ||
        !gameplay.state->packageGraphHash || !gameplay.state->activeImplementationIdentity))
    { parsed.diagnostics.push_back(Error("Runtime.Result.GameplayIdentityMissing",
        "Successful runtime results require complete gameplay identities.")); return parsed; }
    RuntimeResultV1 result;
    result.playSessionId = root["playSessionId"].get<std::string>(); result.status = root["status"].get<std::string>();
    result.canonicalLaunchManifestHash = root["canonicalLaunchManifestHash"].get<std::string>();
    result.exitClassification = root["exitClassification"].get<std::string>(); result.exitCode = root["exitCode"].get<int>();
    result.shutdownClassification = root["shutdownClassification"].get<std::string>();
    result.framesExecuted = root["framesExecuted"].get<std::uint64_t>(); result.interactionActionCount = root["interactionActionCount"].get<std::uint64_t>();
    result.canonicalGameplayStateJson = gameplay.canonicalBytes; result.canonicalGameplayStateHash = root["canonicalGameplayStateHash"].get<std::string>();
    result.observedStateCanonicalJson = Canonical(root["observedState"]);
    if (root["logPath"].is_string()) result.logPath = root["logPath"].get<std::string>();
    else if (!root["logPath"].is_null()) parsed.diagnostics.push_back(Error(
        "Runtime.Result.LogPathInvalid", "logPath must be a string or null."));
    if (root["crashArtifactPath"].is_string())
        result.crashArtifactPath = root["crashArtifactPath"].get<std::string>();
    else if (!root["crashArtifactPath"].is_null()) parsed.diagnostics.push_back(Error(
        "Runtime.Result.CrashPathInvalid", "crashArtifactPath must be a string or null."));
    auto diagnostics = [&](std::string_view field, std::vector<RuntimeDiagnostic>& output)
    {
        for (const auto& item : root[std::string(field)])
        {
            if (!ExactFields(item, {"code", "message", "subject"}) ||
                !NonEmptyString(item, "code") || !NonEmptyString(item, "message") ||
                !item.contains("subject") || !item["subject"].is_string())
            {
                parsed.diagnostics.push_back(Error("Runtime.Result.DiagnosticInvalid",
                    "Runtime diagnostics require code, message, and subject.", std::string(field)));
                return;
            }
            output.push_back({item["code"].get<std::string>(), item["message"].get<std::string>(),
                              item["subject"].get<std::string>()});
        }
    };
    diagnostics("startupDiagnostics", result.startupDiagnostics);
    diagnostics("runtimeDiagnostics", result.runtimeDiagnostics);
    if (result.status != "Succeeded" && result.status != "Failed")
        parsed.diagnostics.push_back(Error("Runtime.Result.StatusInvalid", "Runtime result status must be Succeeded or Failed."));
    static const std::set<std::string> exitClassifications = {"ActionBindingFailure",
        "ActionDispatchFailure", "ArtifactFailure", "Completed", "DependencyFailure",
        "GracefulStop", "LaunchValidationFailure", "ObservedStateMismatch", "RendererFailure",
        "ScriptLifecycleFailure", "ScriptLoadFailure"};
    static const std::set<std::string> shutdownClassifications = {"Clean", "GracefulStop", "StartupFailure"};
    if (!exitClassifications.contains(result.exitClassification))
        parsed.diagnostics.push_back(Error("Runtime.Result.ExitClassificationInvalid", "Runtime result exit classification is unsupported."));
    if (!shutdownClassifications.contains(result.shutdownClassification))
        parsed.diagnostics.push_back(Error("Runtime.Result.ShutdownClassificationInvalid", "Runtime result shutdown classification is unsupported."));
    if (result.status == "Succeeded" && result.exitCode != 0)
        parsed.diagnostics.push_back(Error("Runtime.Result.StatusExitMismatch", "A successful runtime result requires exitCode zero."));
    if (!expectedPlaySessionId.empty() && result.playSessionId != expectedPlaySessionId)
        parsed.diagnostics.push_back(Error("Runtime.Result.PlaySessionMismatch", "Runtime result belongs to a different play session.", result.playSessionId));
    if (!expectedCanonicalManifestHash.empty() &&
        result.canonicalLaunchManifestHash != expectedCanonicalManifestHash)
        parsed.diagnostics.push_back(Error("Runtime.Result.ManifestHashMismatch", "Runtime result belongs to a different canonical launch manifest."));
    if (parsed.diagnostics.empty()) parsed.result = std::move(result);
    return parsed;
}

std::vector<RuntimeDiagnostic> ValidateLocalEnvelope(
    const ResolvedLocalLaunchEnvelope& envelope,
    const std::vector<std::filesystem::path>& approvedRoots)
{
    std::vector<RuntimeDiagnostic> diagnostics;
    if (envelope.schemaVersion != 1 || envelope.playSessionId.empty() ||
        !IsSha256(envelope.canonicalManifestHash))
        diagnostics.push_back(Error("Runtime.Envelope.SchemaInvalid",
            "Local launch envelope schema, session, and manifest hash are required."));
    auto approved = [&](const std::filesystem::path& path)
    {
        return !path.empty() && std::any_of(approvedRoots.begin(), approvedRoots.end(),
            [&](const auto& root) { return Contained(root, path); });
    };
    const std::vector<std::pair<std::string, std::filesystem::path>> paths = {
        {"canonicalManifest", envelope.canonicalManifestPath}, {"runtimeExecutable", envelope.runtimeExecutable},
        {"workingDirectory", envelope.workingDirectory}, {"result", envelope.resultPath},
        {"log", envelope.logPath}, {"stop", envelope.stopPath},
        {"startupReady", envelope.startupReadyPath}};
    for (const auto& [name, path] : paths)
        if (!approved(path)) diagnostics.push_back(Error("Runtime.Envelope.PathEscaped",
            "Resolved local path is outside approved roots.", name));
    for (const auto& [id, path] : envelope.artifacts)
        if (id.empty() || !approved(path)) diagnostics.push_back(Error("Runtime.Envelope.ArtifactPathEscaped",
            "Resolved artifact path is outside approved roots.", id));
    static const std::set<std::string> allowedEnvironment = {
        "DOTNET_ROOT", "SAGA_PLAY_SESSION_ID", "SAGA_RUNTIME_PROFILE"};
    for (const auto& [name, _] : envelope.environment)
        if (!allowedEnvironment.contains(name)) diagnostics.push_back(Error(
            "Runtime.Envelope.EnvironmentNotAllowed", "Environment entry is not allowlisted.", name));
    std::error_code error;
    const auto manifestBytes = ReadFile(envelope.canonicalManifestPath, error);
    if (error || Sha256Bytes(manifestBytes) != envelope.canonicalManifestHash)
        diagnostics.push_back(Error("Runtime.Envelope.ManifestHashMismatch",
            "Resolved canonical manifest bytes failed hash verification."));
    std::sort(diagnostics.begin(), diagnostics.end(), [](const auto& left, const auto& right)
    { return std::tie(left.code, left.subject) < std::tie(right.code, right.subject); });
    return diagnostics;
}

ArtifactLoadResult<RuntimeSceneArtifact> LoadRuntimeSceneArtifact(
    const std::filesystem::path& path)
{
    std::error_code error;
    const auto bytes = ReadFile(path, error);
    if (error) return ArtifactFailure<RuntimeSceneArtifact>("Runtime.Scene.ReadFailed", error.message(), path);
    Json root;
    try { root = Json::parse(bytes); }
    catch (const Json::exception& exception)
    { return ArtifactFailure<RuntimeSceneArtifact>("Runtime.Scene.JsonInvalid", exception.what(), path); }
    ArtifactLoadResult<RuntimeSceneArtifact> result;
    if (!ValidateWrapper(root, "runtime.scene", result.diagnostics)) return result;
    const auto& payload = root["payload"];
    const auto sceneSchema = payload.value("schemaVersion", 1);
    const bool payloadFieldsValid = sceneSchema == 1
        ? ExactFields(payload, {"entities", "sceneId"}) ||
          ExactFields(payload, {"entities", "sceneId", "schemaVersion"})
        : sceneSchema == 2 &&
          ExactFields(payload, {"entities", "sceneId", "schemaVersion"});
    if (!payloadFieldsValid || !NonEmptyString(payload, "sceneId") ||
        !payload.contains("entities") || !payload["entities"].is_array())
    {
        result.diagnostics.push_back(Error("Runtime.Scene.PayloadInvalid",
            "Runtime scene payload requires only sceneId and entities."));
        return result;
    }
    RuntimeSceneArtifact scene;
    scene.schemaVersion = static_cast<std::uint32_t>(sceneSchema);
    scene.artifactId = root["artifactId"].get<std::string>();
    scene.contentHash = root["contentHash"].get<std::string>();
    scene.sourceHash = root["sourceHash"].get<std::string>();
    std::set<std::string> entityIds;
    std::set<std::string> attachmentIds;
    std::map<std::string, std::string> parents;
    for (const auto& source : payload["entities"])
    {
        if (!ExactFields(source, {"components", "entityId", "name", "parentEntityId"}) ||
            !NonEmptyString(source, "entityId") || !NonEmptyString(source, "name") ||
            !source.contains("components") || !source["components"].is_array())
        {
            result.diagnostics.push_back(Error("Runtime.Scene.EntityInvalid",
                "Runtime entity identity, name, parent, or component array is invalid."));
            continue;
        }
        RuntimeSceneEntity entity;
        entity.entityId = source["entityId"].get<std::string>();
        entity.name = source["name"].get<std::string>();
        if (!entityIds.insert(entity.entityId).second)
            result.diagnostics.push_back(Error("Runtime.Scene.EntityDuplicate",
                "Runtime scene entity ids must be unique.", entity.entityId));
        if (!source["parentEntityId"].is_null())
        {
            if (!source["parentEntityId"].is_string() || source["parentEntityId"].get_ref<const std::string&>().empty())
                result.diagnostics.push_back(Error("Runtime.Scene.ParentInvalid",
                    "Runtime parent identity must be null or a non-empty string.", entity.entityId));
            else
            {
                entity.parentEntityId = source["parentEntityId"].get<std::string>();
                parents[entity.entityId] = *entity.parentEntityId;
            }
        }
        std::set<std::string> componentTypes;
        for (const auto& component : source["components"])
        {
            if (!NonEmptyString(component, "type"))
            { result.diagnostics.push_back(Error("Runtime.Scene.ComponentInvalid", "Component type is required.", entity.entityId)); continue; }
            const auto type = component["type"].get<std::string>();
            const bool repeatable =
                type == "ScriptActionBinding" || type == "ScriptEventBinding";
            if (!repeatable && !componentTypes.insert(type).second)
            { result.diagnostics.push_back(Error("Runtime.Scene.ComponentDuplicate", "Singular component type is duplicated.", entity.entityId + "#" + type)); continue; }
            if (type == "Transform")
            {
                if (!ExactFields(component, {"position", "rotation", "scale", "type"}) ||
                    !NumberArray(component, "position", entity.transform.position) ||
                    !NumberArray(component, "rotation", entity.transform.rotation) ||
                    !NumberArray(component, "scale", entity.transform.scale))
                    result.diagnostics.push_back(Error("Runtime.Scene.TransformInvalid", "Transform vectors are malformed or non-finite.", entity.entityId));
            }
            else if (type == "MeshRenderer")
            {
                if (!ExactFields(component, {"materialAssetId", "materialHash", "meshAssetId", "meshHash", "type"}) ||
                    !NonEmptyString(component, "meshAssetId") || !NonEmptyString(component, "materialAssetId") ||
                    !IsSha256(component.value("meshHash", std::string{})) ||
                    !IsSha256(component.value("materialHash", std::string{})))
                    result.diagnostics.push_back(Error("Runtime.Scene.MeshRendererInvalid", "MeshRenderer identities and hashes are invalid.", entity.entityId));
                else entity.meshRenderer = RuntimeMeshRenderer{
                    {component["meshAssetId"].get<std::string>(), component["meshHash"].get<std::string>()},
                    {component["materialAssetId"].get<std::string>(), component["materialHash"].get<std::string>()}};
            }
            else if (type == "BehaviorAttachment")
            {
                if (!ExactFields(component, {"attachmentId", "behaviorModuleId", "enabled",
                                              "instanceConfiguration", "requiredCapabilities",
                                              "scriptArtifactLogicalId", "type"}) ||
                    !NonEmptyString(component, "attachmentId") ||
                    !NonEmptyString(component, "behaviorModuleId") ||
                    !NonEmptyString(component, "scriptArtifactLogicalId") ||
                    !component.contains("enabled") || !component["enabled"].is_boolean() ||
                    !component.contains("instanceConfiguration") || !component["instanceConfiguration"].is_object() ||
                    !component.contains("requiredCapabilities") || !component["requiredCapabilities"].is_array())
                    result.diagnostics.push_back(Error("Runtime.Scene.BehaviorAttachmentInvalid", "BehaviorAttachment logical runtime contract is invalid.", entity.entityId));
                else
                {
                    RuntimeScriptBehavior behavior;
                    behavior.attachmentId = component["attachmentId"].get<std::string>();
                    behavior.behaviorModuleId = component["behaviorModuleId"].get<std::string>();
                    if (!attachmentIds.insert(behavior.attachmentId).second ||
                        !behavior.behaviorModuleId.starts_with("behavior://"))
                        result.diagnostics.push_back(Error(
                            "Runtime.Scene.BehaviorAttachmentIdentityInvalid",
                            "Attachment ids must be unique and module ids must use behavior://.",
                            entity.entityId));
                    behavior.scriptArtifactLogicalId = component["scriptArtifactLogicalId"].get<std::string>();
                    behavior.enabled = component["enabled"].get<bool>();
                    behavior.instanceConfigurationCanonicalJson = Canonical(component["instanceConfiguration"]);
                    for (const auto& capability : component["requiredCapabilities"])
                        if (capability.is_string() && !capability.get_ref<const std::string&>().empty())
                            behavior.requiredCapabilities.push_back(capability.get<std::string>());
                        else result.diagnostics.push_back(Error("Runtime.Scene.CapabilityInvalid", "Script capability is invalid.", entity.entityId));
                    entity.scriptBehavior = std::move(behavior);
                }
            }
            else if (type == "ScriptBehavior")
            {
                if (!ExactFields(component, {"behaviorTypeId", "instanceConfiguration",
                                              "requiredCapabilities", "scriptArtifactLogicalId",
                                              "type"}) ||
                    !NonEmptyString(component, "behaviorTypeId") ||
                    !NonEmptyString(component, "scriptArtifactLogicalId") ||
                    !component.contains("instanceConfiguration") ||
                    !component["instanceConfiguration"].is_object() ||
                    !component.contains("requiredCapabilities") ||
                    !component["requiredCapabilities"].is_array())
                    result.diagnostics.push_back(Error("Runtime.Scene.ScriptBehaviorInvalid",
                        "Legacy ScriptBehavior runtime contract is invalid.", entity.entityId));
                else
                {
                    RuntimeScriptBehavior behavior;
                    behavior.behaviorModuleId = component["behaviorTypeId"].get<std::string>();
                    behavior.attachmentId = "legacy-attachment://" + entity.entityId + "/" +
                        behavior.behaviorModuleId;
                    behavior.scriptArtifactLogicalId =
                        component["scriptArtifactLogicalId"].get<std::string>();
                    behavior.instanceConfigurationCanonicalJson =
                        Canonical(component["instanceConfiguration"]);
                    behavior.legacyCSharpTypeIdentity = true;
                    for (const auto& capability : component["requiredCapabilities"])
                        if (capability.is_string() &&
                            !capability.get_ref<const std::string&>().empty())
                            behavior.requiredCapabilities.push_back(capability.get<std::string>());
                        else result.diagnostics.push_back(Error("Runtime.Scene.CapabilityInvalid",
                            "Script capability is invalid.", entity.entityId));
                    entity.scriptBehavior = std::move(behavior);
                }
            }
            else if (type == "ScriptActionBinding")
            {
                const bool legacy = component.contains("behaviorTypeId");
                const bool shape = legacy
                    ? ExactFields(component, {"behaviorTypeId", "inputActionId",
                                               "namedActionId", "type"})
                    : ExactFields(component, {"behaviorModuleId", "inputActionId",
                                               "namedActionId", "targetAttachmentId", "type"});
                if (!shape ||
                    !(legacy ? NonEmptyString(component, "behaviorTypeId") :
                        NonEmptyString(component, "behaviorModuleId") &&
                        NonEmptyString(component, "targetAttachmentId")) ||
                    !NonEmptyString(component, "inputActionId") ||
                    !NonEmptyString(component, "namedActionId"))
                    result.diagnostics.push_back(Error("Runtime.Scene.ActionBindingInvalid", "Script action binding identities are required.", entity.entityId));
                else entity.actionBindings.push_back({component["inputActionId"].get<std::string>(),
                    component[legacy ? "behaviorTypeId" : "behaviorModuleId"].get<std::string>(),
                    legacy ? std::string{} : component["targetAttachmentId"].get<std::string>(),
                    component["namedActionId"].get<std::string>()});
            }
            else if (type == "TriggerParticipant" && sceneSchema == 2)
            {
                RuntimeTriggerParticipant participant;
                if (!ExactFields(component, {"bounds", "category", "type"}) ||
                    !NumberArray(component, "bounds", participant.bounds) ||
                    std::any_of(participant.bounds.begin(), participant.bounds.end(),
                        [](const double value) { return value <= 0.0; }) ||
                    !NonEmptyString(component, "category"))
                    result.diagnostics.push_back(Error("Runtime.Scene.TriggerParticipantInvalid",
                        "TriggerParticipant requires positive bounds and category.", entity.entityId));
                else
                {
                    participant.category = component["category"].get<std::string>();
                    entity.triggerParticipant = std::move(participant);
                }
            }
            else if (type == "TriggerVolume" && sceneSchema == 2)
            {
                RuntimeTriggerVolume volume;
                if (!ExactFields(component, {"acceptedCategory", "bounds", "enabled", "type"}) ||
                    !NumberArray(component, "bounds", volume.bounds) ||
                    std::any_of(volume.bounds.begin(), volume.bounds.end(),
                        [](const double value) { return value <= 0.0; }) ||
                    !component.contains("enabled") || !component["enabled"].is_boolean() ||
                    !NonEmptyString(component, "acceptedCategory"))
                    result.diagnostics.push_back(Error("Runtime.Scene.TriggerVolumeInvalid",
                        "TriggerVolume requires positive bounds, enabled, and acceptedCategory.",
                        entity.entityId));
                else
                {
                    volume.enabled = component["enabled"].get<bool>();
                    volume.acceptedCategory =
                        component["acceptedCategory"].get<std::string>();
                    entity.triggerVolume = std::move(volume);
                }
            }
            else if (type == "ScriptEventBinding" && sceneSchema == 2)
            {
                const bool legacy = component.contains("behaviorTypeId");
                const bool shape = legacy
                    ? ExactFields(component, {"behaviorTypeId", "eventType", "namedActionId",
                                               "sourceEntityId", "targetEntityId", "type"})
                    : ExactFields(component, {"behaviorModuleId", "eventType", "namedActionId",
                                               "sourceEntityId", "targetAttachmentId",
                                               "targetEntityId", "type"});
                if (!shape ||
                    !(legacy ? NonEmptyString(component, "behaviorTypeId") :
                        NonEmptyString(component, "behaviorModuleId") &&
                        NonEmptyString(component, "targetAttachmentId")) ||
                    !NonEmptyString(component, "namedActionId") ||
                    !NonEmptyString(component, "sourceEntityId") ||
                    !NonEmptyString(component, "targetEntityId") ||
                    (component.value("eventType", std::string{}) != "TriggerEnter" &&
                     component.value("eventType", std::string{}) != "TriggerExit" &&
                     component.value("eventType", std::string{}) != "FocusLost"))
                    result.diagnostics.push_back(Error("Runtime.Scene.EventBindingInvalid",
                        "ScriptEventBinding identities and event type are invalid.",
                        entity.entityId));
                else entity.eventBindings.push_back({
                    component["eventType"].get<std::string>(),
                    component["sourceEntityId"].get<std::string>(),
                    component["targetEntityId"].get<std::string>(),
                    component[legacy ? "behaviorTypeId" : "behaviorModuleId"].get<std::string>(),
                    legacy ? std::string{} : component["targetAttachmentId"].get<std::string>(),
                    component["namedActionId"].get<std::string>()});
            }
            else if (type == "Camera")
            {
                const auto fov = component.value("verticalFovDegrees", -1.0);
                const auto nearClip = component.value("nearClip", -1.0);
                const auto farClip = component.value("farClip", -1.0);
                if (!ExactFields(component, {"farClip", "nearClip", "primary", "projection", "type",
                                              "verticalFovDegrees"}) ||
                    component.value("projection", std::string{}) != "Perspective" ||
                    !component.contains("primary") || !component["primary"].is_boolean() ||
                    !std::isfinite(fov) || !std::isfinite(nearClip) || !std::isfinite(farClip) ||
                    fov <= 0.0 || fov >= 180.0 || nearClip <= 0.0 || farClip <= nearClip)
                    result.diagnostics.push_back(Error("Runtime.Scene.CameraInvalid",
                        "Runtime camera values are malformed or non-finite.", entity.entityId));
                else
                {
                    entity.primaryCamera = component["primary"].get<bool>();
                    entity.verticalFovDegrees = fov;
                    entity.nearClip = nearClip;
                    entity.farClip = farClip;
                }
            }
            else if (type == "DirectionalLight")
            {
                std::array<double, 3> color{};
                const auto intensity = component.value("intensity", -1.0);
                if (!ExactFields(component, {"castsShadows", "color", "intensity", "type"}) ||
                    !NumberArray(component, "color", color) ||
                    std::any_of(color.begin(), color.end(), [](double value) { return value < 0.0; }) ||
                    !std::isfinite(intensity) || intensity < 0.0 ||
                    !component.contains("castsShadows") || !component["castsShadows"].is_boolean())
                    result.diagnostics.push_back(Error("Runtime.Scene.DirectionalLightInvalid",
                        "Runtime directional-light values are malformed or non-finite.", entity.entityId));
                else
                {
                    entity.directionalLight = true;
                    entity.lightColor = color;
                    entity.lightIntensity = intensity;
                    entity.castsShadows = component["castsShadows"].get<bool>();
                }
            }
            else result.diagnostics.push_back(Error("Runtime.Scene.ComponentUnknown", "Unsupported runtime scene component.", type));
        }
        if (!componentTypes.contains("Transform")) result.diagnostics.push_back(Error(
            "Runtime.Scene.TransformMissing", "Every runtime entity requires Transform.", entity.entityId));
        scene.entities.push_back(std::move(entity));
    }
    for (const auto& [child, parent] : parents)
        if (!entityIds.contains(parent)) result.diagnostics.push_back(Error(
            "Runtime.Scene.ParentMissing", "Runtime parent entity does not exist.", child));
    for (const auto& entity : scene.entities)
        for (const auto& binding : entity.eventBindings)
        {
            if (!entityIds.contains(binding.sourceEntityId))
                result.diagnostics.push_back(Error("Runtime.Scene.EventSourceMissing",
                    "Script event source entity does not exist.", binding.sourceEntityId));
            if (!entityIds.contains(binding.targetEntityId))
                result.diagnostics.push_back(Error("Runtime.Scene.EventTargetMissing",
                    "Script event target entity does not exist.", binding.targetEntityId));
        }
    for (const auto& id : entityIds)
    {
        std::set<std::string> visited;
        auto cursor = id;
        while (parents.contains(cursor))
        {
            cursor = parents.at(cursor);
            if (!visited.insert(cursor).second)
            { result.diagnostics.push_back(Error("Runtime.Scene.ParentCycle", "Runtime entity hierarchy contains a cycle.", id)); break; }
        }
    }
    if (result.diagnostics.empty()) result.value = std::move(scene);
    return result;
}

ArtifactLoadResult<RuntimeInputMapArtifact> LoadRuntimeInputMapArtifact(
    const std::filesystem::path& path)
{
    std::error_code error;
    const auto bytes = ReadFile(path, error);
    if (error) return ArtifactFailure<RuntimeInputMapArtifact>("Runtime.Input.ReadFailed", error.message(), path);
    Json root;
    try { root = Json::parse(bytes); }
    catch (const Json::exception& exception)
    { return ArtifactFailure<RuntimeInputMapArtifact>("Runtime.Input.JsonInvalid", exception.what(), path); }
    ArtifactLoadResult<RuntimeInputMapArtifact> result;
    if (!ValidateWrapper(root, "runtime.input-map", result.diagnostics)) return result;
    if (root.value("schemaVersion", -1) != 1)
        return ArtifactFailure<RuntimeInputMapArtifact>(
            "Runtime.Input.SchemaUnsupported",
            "Runtime input artifact schema must be 1.", path);
    const auto& payload = root["payload"];
    if (!ExactFields(payload, {"actions", "inputMapId"}) || !NonEmptyString(payload, "inputMapId") ||
        !payload.contains("actions") || !payload["actions"].is_array())
    { result.diagnostics.push_back(Error("Runtime.Input.PayloadInvalid", "Runtime input payload is invalid.")); return result; }
    RuntimeInputMapArtifact map;
    map.artifactId = root["artifactId"].get<std::string>();
    map.contentHash = root["contentHash"].get<std::string>();
    map.sourceHash = root["sourceHash"].get<std::string>();
    std::set<std::string> actionIds, physical;
    for (const auto& source : payload["actions"])
    {
        if (!ExactFields(source, {"actionId", "bindings", "displayName", "runtimeRole"}) ||
            !NonEmptyString(source, "actionId") || !NonEmptyString(source, "displayName") ||
            !source.contains("bindings") || !source["bindings"].is_array() ||
            (source.value("runtimeRole", std::string{}) != "Gameplay" &&
             source.value("runtimeRole", std::string{}) != "UI"))
        { result.diagnostics.push_back(Error("Runtime.Input.ActionInvalid", "Runtime input action is invalid.")); continue; }
        RuntimeInputAction action;
        action.actionId = source["actionId"].get<std::string>();
        action.displayName = source["displayName"].get<std::string>();
        action.runtimeRole = source["runtimeRole"].get<std::string>();
        if (!actionIds.insert(action.actionId).second) result.diagnostics.push_back(Error(
            "Runtime.Input.ActionDuplicate", "Runtime input action ids must be unique.", action.actionId));
        for (const auto& binding : source["bindings"])
        {
            if (!ExactFields(binding, {"control", "device", "displayLabel", "displayName"}) ||
                !NonEmptyString(binding, "device") || !NonEmptyString(binding, "control") ||
                !NonEmptyString(binding, "displayLabel") ||
                !NonEmptyString(binding, "displayName"))
            { result.diagnostics.push_back(Error("Runtime.Input.BindingInvalid", "Runtime physical binding is invalid.", action.actionId)); continue; }
            RuntimePhysicalBinding value{binding["device"].get<std::string>(),
                binding["control"].get<std::string>(), binding["displayLabel"].get<std::string>(),
                binding["displayName"].get<std::string>()};
            const bool supportedControl =
                (value.control.size() == 1 && value.control[0] >= 'A' &&
                 value.control[0] <= 'Z') ||
                value.control == "Enter" || value.control == "Space" ||
                value.control == "Escape";
            if (value.device != "Keyboard" || !supportedControl ||
                value.displayLabel != value.control ||
                value.displayName != value.device + " " + value.control)
                result.diagnostics.push_back(Error("Runtime.Input.BindingUnsupported", "Physical device/control/display contract is unsupported.", action.actionId));
            if (!physical.insert(value.device + "\x1f" + value.control).second)
                result.diagnostics.push_back(Error("Runtime.Input.BindingAmbiguous", "Physical binding maps ambiguously.", value.displayName));
            action.bindings.push_back(std::move(value));
        }
        map.actions.push_back(std::move(action));
    }
    if (result.diagnostics.empty()) result.value = std::move(map);
    return result;
}

ArtifactLoadResult<RuntimeUiSetArtifact> LoadRuntimeUiSetArtifact(
    const std::filesystem::path& path)
{
    std::error_code error;
    const auto bytes = ReadFile(path, error);
    if (error) return ArtifactFailure<RuntimeUiSetArtifact>(
        "Runtime.UI.ReadFailed", error.message(), path);
    Json root;
    try { root = Json::parse(bytes); }
    catch (const Json::exception& exception)
    { return ArtifactFailure<RuntimeUiSetArtifact>("Runtime.UI.JsonInvalid", exception.what(), path); }
    ArtifactLoadResult<RuntimeUiSetArtifact> result;
    if (!ValidateWrapper(root, "runtime.ui-set", result.diagnostics)) return result;
    const auto& payload = root["payload"];
    const auto uiSchema = root.value("schemaVersion", -1);
    const bool fieldsValid = uiSchema == 1
        ? ExactFields(payload, {"aggregateManifestHash", "documents", "fonts", "prompts",
                                "screenId", "stylesheets", "tool"})
        : uiSchema == 2 && ExactFields(payload, {"aggregateManifestHash", "documents",
              "fonts", "inputLabelBindings", "prompts", "schemaVersion", "screenId",
              "stylesheets", "textBindings", "tool"}) &&
          payload.value("schemaVersion", -1) == 2;
    if (!fieldsValid ||
        !IsSha256(payload.value("aggregateManifestHash", std::string{})) ||
        !NonEmptyString(payload, "screenId") || !payload.contains("documents") ||
        !payload["documents"].is_array() || payload["documents"].empty() ||
        !payload.contains("stylesheets") || !payload["stylesheets"].is_array() ||
        payload["stylesheets"].empty() || !payload.contains("fonts") ||
        !payload["fonts"].is_array() || payload["fonts"].empty() ||
        !payload.contains("prompts") || !payload["prompts"].is_array() ||
        (uiSchema == 1 && payload["prompts"].empty()) ||
        (uiSchema == 2 && (!payload.contains("textBindings") ||
            !payload["textBindings"].is_array() || payload["textBindings"].empty() ||
            !payload.contains("inputLabelBindings") ||
            !payload["inputLabelBindings"].is_array() ||
            payload["inputLabelBindings"].empty())) ||
        !payload.contains("tool") ||
        !ExactFields(payload["tool"], {"name", "version"}) ||
        payload["tool"].value("name", std::string{}) != "sagapack" ||
        payload["tool"].value("version", std::string{}) != "0.0.11")
    {
        result.diagnostics.push_back(Error("Runtime.UI.PayloadInvalid",
            "runtime.ui-set payload is incomplete or malformed."));
        return result;
    }
    RuntimeUiSetArtifact ui;
    ui.schemaVersion = static_cast<std::uint32_t>(uiSchema);
    ui.artifactId = root["artifactId"].get<std::string>();
    ui.contentHash = root["contentHash"].get<std::string>();
    ui.sourceHash = root["sourceHash"].get<std::string>();
    ui.aggregateManifestHash = payload["aggregateManifestHash"].get<std::string>();
    ui.screenId = payload["screenId"].get<std::string>();
    const auto contentRoot = path.parent_path().parent_path();
    auto loadResources = [&](std::string_view field, bool document, bool font,
                             std::vector<RuntimeUiResource>& output)
    {
        for (const auto& item : payload[std::string(field)])
        {
            const bool fieldsValid = document
                ? (uiSchema == 1
                    ? ExactFields(item, {"documentId", "hash", "path", "sourceId"})
                    : ExactFields(item, {"documentId", "hash", "path", "sourceId",
                                        "visibility"}))
                : font ? ExactFields(item, {"family", "hash", "path", "sourceId"})
                       : ExactFields(item, {"hash", "path", "sourceId"});
            if (!fieldsValid || !NonEmptyString(item, "sourceId") ||
                !NonEmptyString(item, "path") || !IsSha256(item.value("hash", std::string{})) ||
                (document && !NonEmptyString(item, "documentId")) ||
                (font && !NonEmptyString(item, "family")))
            {
                result.diagnostics.push_back(Error("Runtime.UI.ResourceInvalid",
                    "UI resource identity, path, or hash is invalid.", std::string(field)));
                continue;
            }
            const auto resolved = contentRoot / item["path"].get<std::string>();
            const auto resourceBytes = ReadFile(resolved, error);
            if (!Contained(contentRoot, resolved) || error ||
                Sha256Bytes(resourceBytes) != item["hash"].get<std::string>())
            {
                result.diagnostics.push_back(Error("Runtime.UI.ResourceHashMismatch",
                    "UI resource escaped the content root or failed hash verification.",
                    resolved.generic_string()));
                error.clear();
                continue;
            }
            RuntimeUiResource resource{item["sourceId"].get<std::string>(), resolved,
                item["hash"].get<std::string>(),
                document ? item["documentId"].get<std::string>() : std::string{},
                font ? item["family"].get<std::string>() : std::string{}};
            if (document && uiSchema == 2)
            {
                const auto& visibility = item["visibility"];
                if (!ExactFields(visibility, {"behaviorId", "entityId", "equalsAny",
                                               "stateKey", "valueType"}) ||
                    !NonEmptyString(visibility, "behaviorId") ||
                    !NonEmptyString(visibility, "entityId") ||
                    !NonEmptyString(visibility, "stateKey") ||
                    visibility.value("valueType", std::string{}) != "String" ||
                    !visibility.contains("equalsAny") ||
                    !visibility["equalsAny"].is_array() ||
                    visibility["equalsAny"].empty())
                {
                    result.diagnostics.push_back(Error(
                        "Runtime.UI.VisibilityInvalid",
                        "Typed document visibility is malformed.",
                        resource.documentId));
                    continue;
                }
                resource.visibilityBehaviorId =
                    visibility["behaviorId"].get<std::string>();
                resource.visibilityEntityId =
                    visibility["entityId"].get<std::string>();
                resource.visibilityStateKey =
                    visibility["stateKey"].get<std::string>();
                resource.visibilityValueType =
                    visibility["valueType"].get<std::string>();
                for (const auto& value : visibility["equalsAny"])
                    if (value.is_string() && !value.get<std::string>().empty())
                        resource.visibilityEqualsAny.push_back(
                            value.get<std::string>());
            }
            output.push_back(std::move(resource));
        }
    };
    loadResources("documents", true, false, ui.documents);
    loadResources("stylesheets", false, false, ui.stylesheets);
    loadResources("fonts", false, true, ui.fonts);
    std::set<std::string> promptIdentities;
    for (const auto& item : payload["prompts"])
    {
        if (!ExactFields(item, {"actionId", "behaviorId", "elementId", "entityId",
                                "falseLabel", "stateKey", "trueLabel", "visibility"}) ||
            !NonEmptyString(item, "actionId") || !NonEmptyString(item, "behaviorId") ||
            !NonEmptyString(item, "elementId") || !NonEmptyString(item, "entityId") ||
            !NonEmptyString(item, "falseLabel") || !NonEmptyString(item, "stateKey") ||
            !NonEmptyString(item, "trueLabel") || item.value("visibility", std::string{}) != "Always")
        {
            result.diagnostics.push_back(Error("Runtime.UI.PromptInvalid",
                "UI prompt binding is incomplete or unsupported."));
            continue;
        }
        const auto identity = item["actionId"].get<std::string>() + "\x1f" +
            item["entityId"].get<std::string>() + "\x1f" + item["behaviorId"].get<std::string>();
        if (!promptIdentities.insert(identity).second)
            result.diagnostics.push_back(Error("Runtime.UI.PromptDuplicate",
                "UI prompt binding identity is duplicated.", identity));
        ui.prompts.push_back({item["actionId"].get<std::string>(),
            item["behaviorId"].get<std::string>(), item["elementId"].get<std::string>(),
            item["entityId"].get<std::string>(), item["falseLabel"].get<std::string>(),
            item["stateKey"].get<std::string>(), item["trueLabel"].get<std::string>(),
            item["visibility"].get<std::string>()});
    }
    if (uiSchema == 2)
    {
        for (const auto& item : payload["textBindings"])
        {
            if (!ExactFields(item, {"behaviorId", "documentId", "elementId",
                                    "entityId", "format", "stateKey", "valueType"}) ||
                !NonEmptyString(item, "behaviorId") ||
                !NonEmptyString(item, "documentId") ||
                !NonEmptyString(item, "elementId") ||
                !NonEmptyString(item, "entityId") ||
                !NonEmptyString(item, "format") ||
                !NonEmptyString(item, "stateKey") ||
                (item.value("valueType", std::string{}) != "Bool" &&
                 item.value("valueType", std::string{}) != "Int32" &&
                 item.value("valueType", std::string{}) != "Double" &&
                 item.value("valueType", std::string{}) != "String"))
            {
                result.diagnostics.push_back(Error("Runtime.UI.TextBindingInvalid",
                    "Typed text binding is malformed."));
                continue;
            }
            ui.textBindings.push_back({item["behaviorId"].get<std::string>(),
                item["documentId"].get<std::string>(),
                item["elementId"].get<std::string>(),
                item["entityId"].get<std::string>(),
                item["format"].get<std::string>(),
                item["stateKey"].get<std::string>(),
                item["valueType"].get<std::string>()});
        }
        for (const auto& item : payload["inputLabelBindings"])
        {
            if (!ExactFields(item, {"actionId", "documentId", "elementId", "format"}) ||
                !NonEmptyString(item, "actionId") ||
                !NonEmptyString(item, "documentId") ||
                !NonEmptyString(item, "elementId") ||
                !NonEmptyString(item, "format"))
            {
                result.diagnostics.push_back(Error("Runtime.UI.InputBindingInvalid",
                    "Input display-label binding is malformed."));
                continue;
            }
            ui.inputLabelBindings.push_back({item["actionId"].get<std::string>(),
                item["documentId"].get<std::string>(),
                item["elementId"].get<std::string>(),
                item["format"].get<std::string>()});
        }
    }
    if (result.diagnostics.empty()) result.value = std::move(ui);
    return result;
}

std::vector<RuntimeDiagnostic> ScriptActionBindingDispatcher::Build(
    const RuntimeSceneArtifact& scene)
{
    bindings_.clear();
    std::vector<RuntimeDiagnostic> diagnostics;
    std::set<std::string> identities;
    std::map<std::string, const RuntimeSceneEntity*> byId;
    std::map<std::string, const RuntimeSceneEntity*> byAttachment;
    for (const auto& entity : scene.entities)
    {
        byId.emplace(entity.entityId, &entity);
        if (entity.scriptBehavior && entity.scriptBehavior->enabled)
            byAttachment.emplace(entity.scriptBehavior->attachmentId, &entity);
    }
    for (const auto& entity : scene.entities)
        for (const auto& binding : entity.actionBindings)
        {
            const RuntimeSceneEntity* targetEntity = nullptr;
            if (!binding.targetAttachmentId.empty())
            {
                const auto found = byAttachment.find(binding.targetAttachmentId);
                if (found != byAttachment.end()) targetEntity = found->second;
            }
            else
            {
                targetEntity = &entity;
                while ((!targetEntity->scriptBehavior ||
                        targetEntity->scriptBehavior->behaviorModuleId != binding.behaviorModuleId) &&
                       targetEntity->parentEntityId &&
                       byId.contains(*targetEntity->parentEntityId))
                    targetEntity = byId.at(*targetEntity->parentEntityId);
            }
            if (targetEntity == nullptr || !targetEntity->scriptBehavior ||
                targetEntity->scriptBehavior->behaviorModuleId != binding.behaviorModuleId)
            { diagnostics.push_back(Error("Runtime.Binding.BehaviorMissing", "Binding target attachment does not own the declared behavior module.", entity.entityId)); continue; }
            const auto identity = binding.inputActionId + "\x1f" + targetEntity->entityId + "\x1f" +
                binding.behaviorModuleId + "\x1f" + binding.namedActionId;
            if (!identities.insert(identity).second)
            { diagnostics.push_back(Error("Runtime.Binding.Duplicate", "Runtime action binding identity is duplicated.", identity)); continue; }
            bindings_[binding.inputActionId].push_back({targetEntity->entityId,
                binding.behaviorModuleId, binding.namedActionId});
        }
    return diagnostics;
}

std::vector<RuntimeDiagnostic> ScriptEventBindingDispatcher::Build(
    const RuntimeSceneArtifact& scene)
{
    bindings_.clear();
    dispatchCount_ = 0;
    std::vector<RuntimeDiagnostic> diagnostics;
    std::map<std::string, const RuntimeSceneEntity*> byId;
    for (const auto& entity : scene.entities) byId.emplace(entity.entityId, &entity);
    std::set<std::string> identities;
    for (const auto& owner : scene.entities)
        for (const auto& binding : owner.eventBindings)
        {
            const auto target = byId.find(binding.targetEntityId);
            if (target == byId.end() || !target->second->scriptBehavior ||
                (!binding.targetAttachmentId.empty() &&
                 target->second->scriptBehavior->attachmentId != binding.targetAttachmentId) ||
                target->second->scriptBehavior->behaviorModuleId != binding.behaviorModuleId)
            {
                diagnostics.push_back(Error("Runtime.EventBinding.BehaviorMissing",
                    "Event target does not own the declared behavior.",
                    binding.targetEntityId));
                continue;
            }
            const auto identity = binding.eventType + "\x1f" + binding.sourceEntityId +
                "\x1f" + binding.targetEntityId + "\x1f" + binding.namedActionId;
            if (!identities.insert(identity).second)
            {
                diagnostics.push_back(Error("Runtime.EventBinding.Duplicate",
                    "Runtime event binding identity is duplicated.", identity));
                continue;
            }
            bindings_[{binding.eventType, binding.sourceEntityId}].push_back(
                {binding.targetEntityId, binding.behaviorModuleId, binding.namedActionId});
        }
    return diagnostics;
}

std::vector<RuntimeDiagnostic> ScriptEventBindingDispatcher::Dispatch(
    const std::string_view eventType, const std::string_view sourceEntityId)
{
    std::vector<RuntimeDiagnostic> diagnostics;
    const auto found =
        bindings_.find({std::string(eventType), std::string(sourceEntityId)});
    if (found == bindings_.end()) return diagnostics;
    for (const auto& binding : found->second)
        if (!target_.InvokeNamedAction(binding.behaviorModuleId,
                                      binding.namedActionId,
                                      binding.targetEntityId))
            diagnostics.push_back(Error("Runtime.EventBinding.InvocationFailed",
                "Validated script event invocation failed.",
                binding.behaviorModuleId + "." + binding.namedActionId));
        else ++dispatchCount_;
    return diagnostics;
}

std::size_t ScriptEventBindingDispatcher::BindingCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& [_, bindings] : bindings_) count += bindings.size();
    return count;
}

std::vector<RuntimeDiagnostic> ScriptActionBindingDispatcher::Dispatch(
    std::string_view inputActionId)
{
    std::vector<RuntimeDiagnostic> diagnostics;
    const auto iterator = bindings_.find(std::string(inputActionId));
    if (iterator == bindings_.end())
    { diagnostics.push_back(Error("Runtime.Binding.ActionUnbound", "Logical input action has no script binding.", std::string(inputActionId))); return diagnostics; }
    for (const auto& binding : iterator->second)
        if (!target_.InvokeNamedAction(binding.behaviorModuleId, binding.namedActionId, binding.entityId))
            diagnostics.push_back(Error("Runtime.Binding.InvocationFailed", "Validated named action invocation failed.",
                binding.behaviorModuleId + "." + binding.namedActionId));
        else ++dispatchCount_;
    return diagnostics;
}
} // namespace SagaRuntime::Play
