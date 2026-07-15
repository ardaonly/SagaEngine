/// @file SagaPackageStaging.cpp
/// @brief Product-owned package manifest staging for Saga projects.

#include "ProductIntegration/SagaPackageStaging.h"

#include "Projects/SagaProjectSystem.h"

#include <SagaAssetPipeline/Packages/PackageManifestWriter.hpp>
#include <SagaEngine/Packages/PackageManifestLoader.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <utility>

namespace SagaProduct
{
namespace
{

constexpr const char* kClientPackageManifest = "package_manifest.client.json";
constexpr const char* kServerPackageManifest = "package_manifest.server.json";
constexpr const char* kAssetIdentityManifest =
    "Build/Manifests/asset_identity.json";

struct ManifestRef
{
    std::string id;
    std::filesystem::path path;
};

struct ScriptArtifactStageEntry
{
    std::string id;
    std::filesystem::path assemblyPath;
    std::string hash;
    std::string packageDestinationIntent;
};

void WriteJsonFile(const std::filesystem::path& path, const nlohmann::json& json);

[[nodiscard]] std::filesystem::path AbsoluteProjectRoot(
    const std::filesystem::path& projectRoot)
{
    if (projectRoot.empty())
    {
        return {};
    }
    return std::filesystem::absolute(projectRoot);
}

[[nodiscard]] SagaProductDiagnostic MakePackageStagingDiagnostic(
    const char* id,
    std::string message,
    std::filesystem::path path)
{
    SagaProductDiagnostic diagnostic;
    diagnostic.stage = SagaProductDiagnosticStage::ProjectValidation;
    diagnostic.diagnosticId = id;
    diagnostic.message = std::move(message);
    diagnostic.path = std::move(path);
    return diagnostic;
}

[[nodiscard]] std::string GenericProjectRelativePath(
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path relative =
        std::filesystem::relative(path, projectRoot, error);
    if (error || relative.empty() || relative.native().find("..") == 0)
    {
        return path.generic_string();
    }
    return relative.generic_string();
}

void AddExistingManifestRef(std::vector<ManifestRef>& refs,
                            const std::filesystem::path& projectRoot,
                            const std::string& id,
                            const std::filesystem::path& relativePath)
{
    const std::filesystem::path absolutePath = projectRoot / relativePath;
    if (!std::filesystem::exists(absolutePath))
    {
        return;
    }

    ManifestRef ref;
    ref.id = id;
    ref.path = relativePath;
    refs.push_back(std::move(ref));
}

[[nodiscard]] std::vector<ManifestRef> DiscoverAssetManifestRefs(
    const std::filesystem::path& projectRoot)
{
    std::vector<ManifestRef> refs;
    AddExistingManifestRef(
        refs, projectRoot, "assets.main", "Build/Manifests/assets.json");
    AddExistingManifestRef(
        refs,
        projectRoot,
        "assets.runtime",
        "Build/Manifests/asset_manifest.json");
    return refs;
}

[[nodiscard]] std::optional<std::string> DiscoverAssetIdentityManifestRef(
    const std::filesystem::path& projectRoot)
{
    const std::filesystem::path relativePath = kAssetIdentityManifest;
    if (!std::filesystem::exists(projectRoot / relativePath))
    {
        return std::nullopt;
    }

    return relativePath.generic_string();
}

[[nodiscard]] std::vector<ManifestRef> DiscoverArtifactManifestRefs(
    const std::filesystem::path& projectRoot)
{
    std::vector<ManifestRef> refs;
    AddExistingManifestRef(
        refs,
        projectRoot,
        "artifacts.main",
        "Build/Manifests/artifact_manifest.json");
    AddExistingManifestRef(
        refs,
        projectRoot,
        "artifacts.legacy",
        "Build/Manifests/artifacts.json");
    return refs;
}

[[nodiscard]] std::optional<std::string> ReadRequiredString(
    const nlohmann::json& object,
    const char* field)
{
    const auto iterator = object.find(field);
    if (iterator == object.end() || !iterator->is_string())
    {
        return std::nullopt;
    }
    return iterator->get<std::string>();
}

void AddScriptArtifactDiagnostic(SagaPackageStagingResult& result,
                                 const std::filesystem::path& path,
                                 std::string message)
{
    result.diagnostics.push_back(MakePackageStagingDiagnostic(
        SagaProductDiagnostics::PackageStageScriptArtifactInvalid,
        std::move(message),
        path));
}

[[nodiscard]] std::vector<ScriptArtifactStageEntry> LoadScriptArtifacts(
    SagaPackageStagingResult& result,
    const std::filesystem::path& projectRoot)
{
    const std::filesystem::path manifestPath =
        projectRoot / "Build" / "Manifests" / "script_artifacts.json";
    if (!std::filesystem::exists(manifestPath))
    {
        return {};
    }

    std::ifstream input(manifestPath);
    nlohmann::json root;
    try
    {
        input >> root;
    }
    catch (const nlohmann::json::exception& exception)
    {
        AddScriptArtifactDiagnostic(
            result,
            manifestPath,
            std::string("Script artifact manifest JSON parse failed: ") +
                exception.what());
        return {};
    }

    const auto artifactsIterator = root.find("artifacts");
    if (artifactsIterator == root.end() || !artifactsIterator->is_array())
    {
        AddScriptArtifactDiagnostic(
            result,
            manifestPath,
            "Script artifact manifest must contain an artifacts array.");
        return {};
    }

    std::vector<ScriptArtifactStageEntry> entries;
    for (const nlohmann::json& artifact : *artifactsIterator)
    {
        if (!artifact.is_object())
        {
            AddScriptArtifactDiagnostic(
                result,
                manifestPath,
                "Script artifact entry must be an object.");
            continue;
        }

        const std::optional<std::string> id =
            ReadRequiredString(artifact, "artifactId");
        const std::optional<std::string> assemblyPath =
            ReadRequiredString(artifact, "assemblyPath");
        const std::optional<std::string> destination =
            ReadRequiredString(artifact, "packageDestinationIntent");
        if (!id.has_value() || !assemblyPath.has_value() ||
            !destination.has_value())
        {
            AddScriptArtifactDiagnostic(
                result,
                manifestPath,
                "Script artifact entry is missing artifactId, assemblyPath, "
                "or packageDestinationIntent.");
            continue;
        }

        if (*destination != "client" && *destination != "server" &&
            *destination != "shared")
        {
            AddScriptArtifactDiagnostic(
                result,
                manifestPath,
                "Script artifact entry has unsupported package destination: " +
                    *destination);
            continue;
        }

        const std::filesystem::path absoluteAssembly =
            projectRoot / std::filesystem::path(*assemblyPath);
        if (!std::filesystem::exists(absoluteAssembly))
        {
            AddScriptArtifactDiagnostic(
                result,
                absoluteAssembly,
                "Script artifact assembly is missing: " +
                    absoluteAssembly.string());
            continue;
        }

        ScriptArtifactStageEntry entry;
        entry.id = *id;
        entry.assemblyPath = std::filesystem::path(*assemblyPath);
        entry.packageDestinationIntent = *destination;
        const auto hash = artifact.find("hash");
        if (hash != artifact.end() && hash->is_string())
        {
            entry.hash = hash->get<std::string>();
        }
        entries.push_back(std::move(entry));
    }

    return entries;
}

[[nodiscard]] nlohmann::json ScriptArtifactRefsToJson(
    const std::vector<ScriptArtifactStageEntry>& entries)
{
    nlohmann::json artifacts = nlohmann::json::array();
    for (const ScriptArtifactStageEntry& entry : entries)
    {
        nlohmann::json artifact;
        artifact["id"] = entry.id;
        artifact["kind"] = "script";
        artifact["path"] = entry.assemblyPath.generic_string();
        if (!entry.hash.empty())
        {
            artifact["hash"] = entry.hash;
        }
        artifacts.push_back(std::move(artifact));
    }

    nlohmann::json root;
    root["schemaVersion"] = 1;
    root["artifacts"] = std::move(artifacts);
    return root;
}

[[nodiscard]] std::vector<ManifestRef> StageScriptArtifactManifestRefs(
    const std::filesystem::path& projectRoot,
    const std::vector<ScriptArtifactStageEntry>& scriptArtifacts,
    const char* packageKind)
{
    std::vector<ScriptArtifactStageEntry> staged;
    std::copy_if(scriptArtifacts.begin(),
                 scriptArtifacts.end(),
                 std::back_inserter(staged),
                 [packageKind](const ScriptArtifactStageEntry& artifact)
                 {
                     return artifact.packageDestinationIntent == "shared" ||
                         artifact.packageDestinationIntent == packageKind;
                 });

    if (staged.empty())
    {
        return {};
    }

    const std::filesystem::path relativePath =
        std::filesystem::path("Build") / "Manifests" /
        (std::string("artifact_manifest.scripts.") + packageKind + ".json");
    WriteJsonFile(projectRoot / relativePath, ScriptArtifactRefsToJson(staged));

    return {ManifestRef{
        std::string("scripts.") + packageKind,
        relativePath
    }};
}

[[nodiscard]] nlohmann::json ManifestRefsToJson(
    const std::vector<ManifestRef>& refs)
{
    nlohmann::json json = nlohmann::json::array();
    for (const ManifestRef& ref : refs)
    {
        json.push_back({
            {"id", ref.id},
            {"path", ref.path.generic_string()}
        });
    }
    return json;
}

[[nodiscard]] SagaAssetPipeline::PackageManifestRefWriteInput
ToPackageManifestRefWriteInput(const ManifestRef& ref)
{
    SagaAssetPipeline::PackageManifestRefWriteInput input;
    input.id = ref.id;
    input.path = ref.path.generic_string();
    return input;
}

[[nodiscard]] std::vector<SagaAssetPipeline::PackageManifestRefWriteInput>
ToPackageManifestRefWriteInputs(const std::vector<ManifestRef>& refs)
{
    std::vector<SagaAssetPipeline::PackageManifestRefWriteInput> inputs;
    inputs.reserve(refs.size());
    for (const ManifestRef& ref : refs)
    {
        inputs.push_back(ToPackageManifestRefWriteInput(ref));
    }
    return inputs;
}

void WriteJsonFile(const std::filesystem::path& path, const nlohmann::json& json)
{
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::trunc);
    output << json.dump(2) << '\n';
}

[[nodiscard]] SagaAssetPipeline::PackageManifestWriteInput
MakePackageManifestWriteInput(
    const SagaProjectManifest& project,
    const SagaPackageStagingRequest& request,
    const char* packageKind,
    std::optional<std::string> assetIdentityManifest,
    const std::vector<ManifestRef>& assetRefs,
    const std::vector<ManifestRef>& artifactRefs)
{
    SagaAssetPipeline::PackageManifestWriteInput input;
    input.packageId =
        project.projectId + "." + std::string(packageKind) + "." +
        request.profile;
    input.packageKind = std::string(packageKind) == "client"
        ? SagaAssetPipeline::PackageManifestKind::Client
        : SagaAssetPipeline::PackageManifestKind::Server;
    input.buildProfile = request.profile;
    input.targetPlatform = request.targetPlatform;
    input.runtimeCompatibilityVersion = request.runtimeCompatibilityVersion;
    input.assetIdentityManifest = std::move(assetIdentityManifest);
    input.assetManifests = ToPackageManifestRefWriteInputs(assetRefs);
    input.artifactManifests = ToPackageManifestRefWriteInputs(artifactRefs);
    return input;
}

[[nodiscard]] bool WritePackageManifestWithWriter(
    SagaPackageStagingResult& result,
    const std::filesystem::path& outputPath,
    const SagaAssetPipeline::PackageManifestWriteInput& input)
{
    const SagaAssetPipeline::PackageManifestWriteResult writeResult =
        SagaAssetPipeline::PackageManifestWriter::WriteToFile(
            outputPath,
            input);
    if (writeResult.Succeeded())
    {
        return true;
    }

    for (const SagaAssetPipeline::PackageManifestWriteDiagnostic& diagnostic :
         writeResult.diagnostics)
    {
        std::string message = "Generated package manifest writer failed: " +
            diagnostic.message + " (" + diagnostic.diagnosticId + ")";
        if (diagnostic.fieldName.has_value())
        {
            message += " field=" + *diagnostic.fieldName;
        }
        if (diagnostic.referenceIndex.has_value())
        {
            message += " referenceIndex=" +
                std::to_string(*diagnostic.referenceIndex);
        }

        std::filesystem::path diagnosticPath = diagnostic.outputPath;
        if (!diagnostic.referencePath.empty())
        {
            diagnosticPath = diagnostic.referencePath;
        }

        result.diagnostics.push_back(MakePackageStagingDiagnostic(
            SagaProductDiagnostics::PackageStageManifestInvalid,
            std::move(message),
            std::move(diagnosticPath)));
    }

    return false;
}

void ValidateWrittenPackageManifest(SagaPackageStagingResult& result,
                                    const std::filesystem::path& projectRoot,
                                    const std::filesystem::path& manifestPath)
{
    SagaEngine::Packages::PackageManifestLoadOptions options;
    options.validateReferencedManifestFiles = true;
    options.packageBaseDirectory = projectRoot;

    const auto loaded =
        SagaEngine::Packages::PackageManifestLoader::LoadFromFile(
            manifestPath,
            options);
    if (loaded.Succeeded())
    {
        return;
    }

    for (const auto& error : loaded.errors)
    {
        result.diagnostics.push_back(MakePackageStagingDiagnostic(
            SagaProductDiagnostics::PackageStageManifestInvalid,
            "Generated package manifest is invalid: " + error.message,
            error.manifestPath));
    }
}

void WriteStageReport(const SagaPackageStagingRequest& request,
                      const SagaPackageStagingResult& result,
                      const std::filesystem::path& projectRoot,
                      const std::optional<std::string>& assetIdentityManifest,
                      const std::vector<ManifestRef>& assetRefs,
                      const std::vector<ManifestRef>& clientArtifactRefs,
                      const std::vector<ManifestRef>& serverArtifactRefs)
{
    nlohmann::json report;
    report["schemaVersion"] = 1;
    report["status"] = result.ok ? "staged" : "blocked";
    report["projectRoot"] = projectRoot.string();
    report["profile"] = request.profile;
    report["targetPlatform"] = request.targetPlatform;
    report["runtimeCompatibilityVersion"] =
        request.runtimeCompatibilityVersion;
    report["outputs"] = {
        {"clientPackageManifest",
         GenericProjectRelativePath(projectRoot,
                                    result.paths.clientPackageManifest)},
        {"serverPackageManifest",
         GenericProjectRelativePath(projectRoot,
                                    result.paths.serverPackageManifest)}
    };
    if (assetIdentityManifest.has_value())
    {
        report["assetIdentityManifest"] = *assetIdentityManifest;
    }
    else
    {
        report["assetIdentityManifest"] = nullptr;
    }
    report["assetManifests"] = ManifestRefsToJson(assetRefs);
    report["clientArtifactManifests"] = ManifestRefsToJson(clientArtifactRefs);
    report["serverArtifactManifests"] = ManifestRefsToJson(serverArtifactRefs);
    report["diagnostics"] = nlohmann::json::array();
    for (const SagaProductDiagnostic& diagnostic : result.diagnostics)
    {
        nlohmann::json item;
        item["id"] = diagnostic.diagnosticId;
        item["stage"] = ToString(diagnostic.stage);
        item["message"] = diagnostic.message;
        if (diagnostic.path.has_value())
        {
            item["path"] = diagnostic.path->string();
        }
        report["diagnostics"].push_back(std::move(item));
    }

    WriteJsonFile(result.paths.reportPath, report);
}

} // namespace

SagaPackageStagingPaths SagaPackageStagingService::DefaultPathsForProject(
    const std::filesystem::path& projectRoot)
{
    const std::filesystem::path root = AbsoluteProjectRoot(projectRoot);
    SagaPackageStagingPaths paths;
    paths.manifestDirectory = root / "Build" / "Manifests";
    paths.clientPackageManifest =
        paths.manifestDirectory / kClientPackageManifest;
    paths.serverPackageManifest =
        paths.manifestDirectory / kServerPackageManifest;
    paths.reportPath =
        root / "Build" / "Reports" / "package_stage_report.json";
    return paths;
}

SagaPackageStagingResult SagaPackageStagingService::Stage(
    const SagaPackageStagingRequest& request) const
{
    SagaPackageStagingResult result;
    const std::filesystem::path projectRoot =
        AbsoluteProjectRoot(request.projectRoot);
    result.paths = DefaultPathsForProject(projectRoot);
    if (!request.reportPath.empty())
    {
        result.paths.reportPath = std::filesystem::absolute(request.reportPath);
    }

    SagaProjectSystem projects(projectRoot / "Build" / "Reports" /
        "package_stage_recent_projects.json");
    const SagaProjectResult project = projects.OpenProject(projectRoot);
    if (!project.ok)
    {
        result.diagnostics.push_back(MakePackageStagingDiagnostic(
            SagaProductDiagnostics::PackageStageProjectInvalid,
            project.error,
            projectRoot / "saga.project.json"));
        WriteStageReport(request,
                         result,
                         projectRoot,
                         std::nullopt,
                         {},
                         {},
                         {});
        return result;
    }

    const std::vector<ManifestRef> assetRefs =
        DiscoverAssetManifestRefs(projectRoot);
    const std::optional<std::string> assetIdentityManifest =
        DiscoverAssetIdentityManifestRef(projectRoot);
    const std::vector<ManifestRef> baseArtifactRefs =
        DiscoverArtifactManifestRefs(projectRoot);
    const std::vector<ScriptArtifactStageEntry> scriptArtifacts =
        LoadScriptArtifacts(result, projectRoot);
    const std::vector<ManifestRef> clientScriptArtifactRefs =
        StageScriptArtifactManifestRefs(projectRoot, scriptArtifacts, "client");
    const std::vector<ManifestRef> serverScriptArtifactRefs =
        StageScriptArtifactManifestRefs(projectRoot, scriptArtifacts, "server");

    std::vector<ManifestRef> clientArtifactRefs = baseArtifactRefs;
    clientArtifactRefs.insert(clientArtifactRefs.end(),
                              clientScriptArtifactRefs.begin(),
                              clientScriptArtifactRefs.end());
    std::vector<ManifestRef> serverArtifactRefs = baseArtifactRefs;
    serverArtifactRefs.insert(serverArtifactRefs.end(),
                              serverScriptArtifactRefs.begin(),
                              serverScriptArtifactRefs.end());

    if (!result.diagnostics.empty())
    {
        WriteStageReport(request,
                         result,
                         projectRoot,
                         assetIdentityManifest,
                         assetRefs,
                         clientArtifactRefs,
                         serverArtifactRefs);
        return result;
    }

    if (assetRefs.empty() && clientArtifactRefs.empty() &&
        serverArtifactRefs.empty())
    {
        result.diagnostics.push_back(MakePackageStagingDiagnostic(
            SagaProductDiagnostics::PackageStageInputsMissing,
            "Package staging requires at least one existing asset or artifact "
            "manifest under Build/Manifests.",
            projectRoot / "Build" / "Manifests"));
        WriteStageReport(request,
                         result,
                         projectRoot,
                         assetIdentityManifest,
                         assetRefs,
                         clientArtifactRefs,
                         serverArtifactRefs);
        return result;
    }

    const SagaAssetPipeline::PackageManifestWriteInput clientManifest =
        MakePackageManifestWriteInput(project.manifest,
                                      request,
                                      "client",
                                      assetIdentityManifest,
                                      assetRefs,
                                      clientArtifactRefs);
    const SagaAssetPipeline::PackageManifestWriteInput serverManifest =
        MakePackageManifestWriteInput(project.manifest,
                                      request,
                                      "server",
                                      assetIdentityManifest,
                                      assetRefs,
                                      serverArtifactRefs);

    const bool wroteClientManifest = WritePackageManifestWithWriter(
        result,
        result.paths.clientPackageManifest,
        clientManifest);
    const bool wroteServerManifest = WritePackageManifestWithWriter(
        result,
        result.paths.serverPackageManifest,
        serverManifest);

    if (wroteClientManifest)
    {
        ValidateWrittenPackageManifest(
            result, projectRoot, result.paths.clientPackageManifest);
    }
    if (wroteServerManifest)
    {
        ValidateWrittenPackageManifest(
            result, projectRoot, result.paths.serverPackageManifest);
    }

    result.ok = result.diagnostics.empty();
    WriteStageReport(request,
                     result,
                     projectRoot,
                     assetIdentityManifest,
                     assetRefs,
                     clientArtifactRefs,
                     serverArtifactRefs);
    return result;
}

} // namespace SagaProduct
