/// @file FirstPlayableWorkspacePolicy.cpp
#include "FirstPlayableWorkspacePolicy.h"

#include <QByteArrayView>
#include <QCryptographicHash>

#include <array>
#include <fstream>

namespace SagaProduct
{
namespace
{
const std::array<std::filesystem::path, 7> kSources = {
    "StarterArena.sagaproj", "Scenes/arena.scene.json", "Scripts/GameRules.cs",
    "Input/playable.synthetic-input.json", "README.md", "ACCEPTANCE.md",
    "KNOWN_LIMITATIONS.md"};

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

bool Within(const std::filesystem::path& child, const std::filesystem::path& parent)
{
    std::error_code ec;
    const auto c = std::filesystem::weakly_canonical(child, ec);
    if (ec) return false;
    const auto p = std::filesystem::weakly_canonical(parent, ec);
    if (ec) return false;
    auto ci = c.begin();
    for (auto pi = p.begin(); pi != p.end(); ++pi, ++ci)
        if (ci == c.end() || *ci != *pi) return false;
    return true;
}

bool PrivatePath(const std::filesystem::path& path)
{
    for (const auto& part : path) if (part == ".saga-private") return true;
    return false;
}

std::filesystem::path RepositoryRoot(std::filesystem::path path)
{
    for (path = std::filesystem::absolute(path); !path.empty();
         path = path.parent_path())
    {
        if (std::filesystem::exists(path / ".git")) return path;
        if (path == path.root_path()) break;
    }
    return {};
}

std::map<std::string, std::string> Tree(const std::filesystem::path& root)
{
    std::map<std::string, std::string> result;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end;
         !ec && it != end; it.increment(ec))
    {
        const auto relative = std::filesystem::relative(it->path(), root, ec);
        if (ec) break;
        if (it->is_directory()) result[relative.generic_string()] = "directory";
        else if (it->is_regular_file())
            result[relative.generic_string()] = "file:" + Hash(it->path());
        else result[relative.generic_string()] = "unsupported";
    }
    if (ec) result["<scan-error>"] = ec.message();
    return result;
}

void Error(std::vector<FirstPlayableDiagnostic>& diagnostics,
           std::string code, std::string message, const std::filesystem::path& path)
{
    FirstPlayableDiagnostic value;
    value.code = std::move(code); value.message = std::move(message); value.path = path;
    value.actionHint = "Use a fresh output directory outside the repository and retry.";
    diagnostics.push_back(std::move(value));
}
} // namespace

FirstPlayableWorkspacePolicySession FirstPlayableWorkspacePolicy::Begin(
    const std::filesystem::path& manifest, const std::filesystem::path& output,
    const std::filesystem::path& summary)
{
    FirstPlayableWorkspacePolicySession result;
    result.projectManifest = std::filesystem::absolute(manifest);
    result.projectRoot = result.projectManifest.parent_path();
    result.outputRoot = std::filesystem::absolute(output);
    result.summaryPath = std::filesystem::absolute(summary);
    const auto repository = RepositoryRoot(result.projectRoot);
    if (!std::filesystem::is_regular_file(result.projectManifest))
        Error(result.diagnostics, "ProductShell.FirstPlayable.SourceFileMissing",
              "StarterArena project manifest is missing", result.projectManifest);
    if (Within(result.outputRoot, result.projectRoot) ||
        (!repository.empty() && Within(result.outputRoot, repository)))
        Error(result.diagnostics, "ProductShell.FirstPlayable.GeneratedOutputInsideSample",
              "first-playable output must be outside the source repository", result.outputRoot);
    if (PrivatePath(result.outputRoot) || PrivatePath(result.summaryPath))
        Error(result.diagnostics, "ProductShell.FirstPlayable.SagaPrivateTouched",
              "first-playable paths must not target .saga-private", result.outputRoot);
    if (!Within(result.summaryPath, result.outputRoot))
        Error(result.diagnostics, "ProductShell.FirstPlayable.SummaryOutsideOutputRoot",
              "the summary must remain inside the evidence output root", result.summaryPath);
    std::error_code ec;
    if (std::filesystem::exists(result.outputRoot, ec) &&
        (!std::filesystem::is_directory(result.outputRoot, ec) ||
         !std::filesystem::is_empty(result.outputRoot, ec)))
        Error(result.diagnostics, "ProductShell.FirstPlayable.OutputRootNotEmpty",
              "the release-candidate output root must be absent or empty", result.outputRoot);
    for (const auto& relative : kSources)
    {
        FirstPlayableSourceFile source;
        source.relativePath = relative;
        const auto path = result.projectRoot / relative;
        if (!Within(path, result.projectRoot) || !std::filesystem::is_regular_file(path))
            Error(result.diagnostics, "ProductShell.FirstPlayable.SourceFileMissing",
                  "required StarterArena source file is missing", path);
        else source.beforeSha256 = Hash(path);
        result.sourceFiles.push_back(std::move(source));
    }
    if (std::filesystem::exists(result.projectRoot / "VisualBlocks"))
        Error(result.diagnostics, "ProductShell.FirstPlayable.GeneratedOutputInsideSample",
              "samples/StarterArena/VisualBlocks must remain absent",
              result.projectRoot / "VisualBlocks");
    result.beforeTree = Tree(result.projectRoot);
    result.status = result.diagnostics.empty() ? EvidenceStatus::Passed :
                                                 EvidenceStatus::Incomplete;
    return result;
}

FirstPlayableWorkspacePolicyResult FirstPlayableWorkspacePolicy::Complete(
    const FirstPlayableWorkspacePolicySession& session)
{
    FirstPlayableWorkspacePolicyResult result;
    result.projectRoot = session.projectRoot; result.outputRoot = session.outputRoot;
    result.sourceFiles = session.sourceFiles; result.diagnostics = session.diagnostics;
    if (session.status != EvidenceStatus::Passed)
    {
        result.status = result.sourceIntegrity = EvidenceStatus::Incomplete;
        return result;
    }
    for (auto& source : result.sourceFiles)
    {
        const auto path = result.projectRoot / source.relativePath;
        source.afterSha256 = Hash(path);
        source.unchanged = !source.beforeSha256.empty() &&
            source.beforeSha256 == source.afterSha256;
        if (!source.unchanged)
            Error(result.diagnostics, "ProductShell.FirstPlayable.SourceHashChanged",
                  "StarterArena source changed during the workflow", path);
    }
    const auto after = Tree(result.projectRoot);
    if (after != session.beforeTree)
    {
        result.sampleSourceModified = true;
        for (const auto& [path, value] : after)
        {
            const auto before = session.beforeTree.find(path);
            if (before == session.beforeTree.end() || before->second != value)
                result.generatedFilesInsideSample.push_back(path);
        }
        Error(result.diagnostics, "ProductShell.FirstPlayable.WorkspacePolicyFailed",
              "StarterArena source tree changed during the workflow", result.projectRoot);
    }
    if (std::filesystem::exists(result.projectRoot / "VisualBlocks"))
        Error(result.diagnostics, "ProductShell.FirstPlayable.GeneratedOutputInsideSample",
              "VisualBlocks output appeared inside StarterArena",
              result.projectRoot / "VisualBlocks");
    result.status = result.diagnostics.empty() ? EvidenceStatus::Passed : EvidenceStatus::Failed;
    result.sourceIntegrity = result.status;
    return result;
}
} // namespace SagaProduct
