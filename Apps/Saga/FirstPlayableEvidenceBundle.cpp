/// @file FirstPlayableEvidenceBundle.cpp
#include "FirstPlayableEvidenceBundle.h"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <nlohmann/json.hpp>

#include <array>
#include <fstream>

namespace SagaProduct
{
namespace
{
using Json = nlohmann::json;
std::string Hash(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<char, 16384> data{};
    while (input)
    {
        input.read(data.data(), static_cast<std::streamsize>(data.size()));
        hash.addData(QByteArrayView(data.data(), input.gcount()));
    }
    return hash.result().toHex().toStdString();
}
void Failure(std::vector<FirstPlayableDiagnostic>& diagnostics,
             std::string code, std::string message,
             const std::filesystem::path& path)
{
    FirstPlayableDiagnostic diagnostic;
    diagnostic.code = std::move(code); diagnostic.message = std::move(message);
    diagnostic.path = path;
    diagnostic.actionHint = "Use a writable fresh evidence output directory and retry.";
    diagnostics.push_back(std::move(diagnostic));
}
} // namespace

bool FirstPlayableEvidenceBundle::WriteJson(
    const std::filesystem::path& path, const Json& value,
    std::vector<FirstPlayableDiagnostic>& diagnostics, const std::string& code)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream output(path, std::ios::trunc);
    if (ec || !output)
    {
        Failure(diagnostics, code, "could not write first-playable JSON report", path);
        return false;
    }
    output << value.dump(2) << '\n';
    if (!output)
    {
        Failure(diagnostics, code, "failed while writing first-playable JSON report", path);
        return false;
    }
    return true;
}

FirstPlayableEvidenceBundleResult FirstPlayableEvidenceBundle::Write(
    const std::filesystem::path& outputRoot,
    const RuntimeEvidenceRunResult& runtime,
    const VisualBlocksDescriptorGenerationResult& descriptor,
    const FirstPlayableGateResult& gate,
    const std::filesystem::path& summaryPath,
    const std::filesystem::path& gatePath)
{
    FirstPlayableEvidenceBundleResult result;
    result.sourceManifestPath = outputRoot / "source_manifest.json";
    result.evidenceManifestPath = outputRoot / "evidence_manifest.json";
    Json sources = Json::array();
    for (const auto& source : gate.workspacePolicy.sourceFiles)
        sources.push_back({{"path", source.relativePath.generic_string()},
            {"sha256", source.afterSha256}, {"beforeSha256", source.beforeSha256},
            {"unchanged", source.unchanged}});
    const Json sourceManifest = {{"schemaVersion", 1},
        {"sourceRoot", gate.workspacePolicy.projectRoot.string()},
        {"sourceFiles", sources}};
    if (!WriteJson(result.sourceManifestPath, sourceManifest, result.diagnostics,
                   "ProductShell.FirstPlayable.SourceManifestWriteFailed"))
    {
        result.status = EvidenceStatus::Failed;
        return result;
    }
    if (gate.manualEvidence.status == EvidenceStatus::Passed &&
        gate.manualEvidence.reportPath)
    {
        const auto imported = outputRoot / "manual" / "real_keyboard_report.json";
        std::error_code ec;
        std::filesystem::create_directories(imported.parent_path(), ec);
        if (!ec) std::filesystem::copy_file(*gate.manualEvidence.reportPath, imported,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec || Hash(imported) != gate.manualEvidence.reportSha256)
            Failure(result.diagnostics,
                "ProductShell.FirstPlayable.EvidenceManifestWriteFailed",
                "could not import validated keyboard evidence", imported);
    }
    Json artifacts = Json::array();
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(outputRoot, ec), end;
         !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file()) continue;
        const auto relative = std::filesystem::relative(it->path(), outputRoot, ec);
        if (ec) break;
        if (relative == "evidence_manifest.json") continue;
        const std::string generic = relative.generic_string();
        std::string role = "generated-workspace";
        if (generic == "first_playable_summary.json") role = "summary";
        else if (generic == "first_playable_gate.json") role = "gate";
        else if (generic == "source_manifest.json") role = "source-manifest";
        else if (generic.starts_with("profiles/")) role = "profile-evidence";
        else if (generic.starts_with("script/")) role = "script-evidence";
        else if (generic.starts_with("manual/")) role = "manual-evidence";
        Json artifact = {{"path", generic}, {"role", role},
            {"sha256", Hash(it->path())},
            {"bytes", std::filesystem::file_size(it->path(), ec)}};
        if (generic.starts_with("profiles/"))
        {
            const auto slash = generic.find('/', 9);
            if (slash != std::string::npos)
                artifact["profileId"] = generic.substr(9, slash - 9);
        }
        artifacts.push_back(std::move(artifact));
        if (ec) break;
    }
    if (ec) Failure(result.diagnostics,
        "ProductShell.FirstPlayable.EvidenceManifestWriteFailed",
        "could not inventory generated evidence", outputRoot);
    const auto hasPath = [&](const std::filesystem::path& path) {
        return std::filesystem::is_regular_file(path) &&
               path.lexically_normal().string().starts_with(outputRoot.lexically_normal().string());
    };
    if (!hasPath(summaryPath) || !hasPath(gatePath) ||
        !hasPath(result.sourceManifestPath) || !hasPath(descriptor.reportPath) ||
        runtime.profiles.size() != 5)
        Failure(result.diagnostics,
            "ProductShell.FirstPlayable.EvidenceManifestWriteFailed",
            "mandatory evidence artifacts are missing", outputRoot);
    for (const auto& profile : runtime.profiles)
        if (!hasPath(profile.reportPath) || !hasPath(profile.stdoutPath) ||
            !hasPath(profile.stderrPath))
            Failure(result.diagnostics,
                "ProductShell.FirstPlayable.EvidenceManifestWriteFailed",
                "runtime profile evidence is incomplete", profile.reportPath);
    Json profiles = Json::array();
    for (const auto& profile : runtime.profiles)
        profiles.push_back({{"id", ToString(profile.profile)},
            {"report", std::filesystem::relative(profile.reportPath, outputRoot).generic_string()},
            {"stdout", std::filesystem::relative(profile.stdoutPath, outputRoot).generic_string()},
            {"stderr", std::filesystem::relative(profile.stderrPath, outputRoot).generic_string()}});
    profiles.push_back({{"id", kVisualBlocksDescriptorProfileId},
        {"report", std::filesystem::relative(descriptor.reportPath, outputRoot).generic_string()},
        {"stdout", nullptr}, {"stderr", nullptr}});
    const Json manifest = {{"schemaVersion", 1}, {"tool", "Saga"},
        {"command", "first-playable-check"},
        {"status", result.diagnostics.empty() ? "Passed" : "Failed"},
        {"outputRoot", outputRoot.string()},
        {"summary", std::filesystem::relative(summaryPath, outputRoot).generic_string()},
        {"gate", std::filesystem::relative(gatePath, outputRoot).generic_string()},
        {"sourceManifest", "source_manifest.json"},
        {"profiles", profiles},
        {"manualEvidence", {{"realKeyboard", ToString(gate.manualEvidence.status)}}},
        {"nonClaims", gate.nonClaims}, {"artifacts", artifacts}};
    if (!WriteJson(result.evidenceManifestPath, manifest, result.diagnostics,
                   "ProductShell.FirstPlayable.EvidenceManifestWriteFailed"))
        result.status = EvidenceStatus::Failed;
    else result.status = result.diagnostics.empty() ? EvidenceStatus::Passed :
                                                     EvidenceStatus::Failed;
    return result;
}
} // namespace SagaProduct
