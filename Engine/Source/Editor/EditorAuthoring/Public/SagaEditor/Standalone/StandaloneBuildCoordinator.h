// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Play/PlaySourceSnapshot.h"
#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{
enum class StandaloneBuildState
{
    Idle, Capturing, Materializing, Building, Verifying, Publishing,
    Succeeded, Failed, Cancelled
};

struct BuildInputFile
{
    std::filesystem::path sourcePath;
    std::filesystem::path snapshotPath;
    std::string hash;
    std::uintmax_t size = 0;
};

struct BuildInputSnapshot
{
    int schemaVersion = 1;
    std::string snapshotId;
    std::string projectId;
    std::string projectHash;
    std::string lockHash;
    std::string packageGenerationId;
    std::string packageGenerationHash;
    std::string packageGraphHash;
    std::string profileId;
    std::string profileHash;
    std::string target;
    std::string runtimeIdentifier;
    std::string configuration;
    std::string runtimeHash;
    std::map<std::string, std::string> toolHashes;
    std::vector<BuildInputFile> files;
    std::string canonicalJson;
};

struct StandaloneBuildProgress
{
    StandaloneBuildState state = StandaloneBuildState::Idle;
    unsigned completed = 0;
    unsigned total = 0;
    std::string message;
};

struct StandaloneBuildRequest
{
    PlaySourceSnapshot source;
    std::filesystem::path buildProfilesPath;
    std::string profileId;
    std::string profileHash;
    std::string target = "desktop";
    std::string runtimeIdentifier;
    std::string configuration = "RelWithDebInfo";
    std::filesystem::path runtimeExecutable;
    std::filesystem::path outputDirectory;
    ScriptAnalysisToolchain toolchain;
    unsigned captureAttempts = 2;
    std::function<bool()> cancellationRequested;
    std::function<void(const StandaloneBuildProgress&)> progress;
    std::function<void()> beforeIdentitySecondRead;
    std::function<void()> beforePublish;
    /// Optional publication policy/failure-injection boundary. Production
    /// callers leave it empty; false preserves the prior valid output.
    std::function<bool()> publicationAllowed;
};

struct StandaloneBuildExecution
{
    bool passed = false;
    bool cancelled = false;
    std::filesystem::path preparedRoot;
    std::filesystem::path canonicalManifestPath;
    std::filesystem::path localEnvelopePath;
    std::map<std::string, std::string> hashes;
    std::vector<DocumentDiagnostic> diagnostics;
};

struct StandaloneBuildResult
{
    StandaloneBuildState state = StandaloneBuildState::Failed;
    bool published = false;
    bool preservedPreviousOutput = false;
    BuildInputSnapshot snapshot;
    std::filesystem::path outputDirectory;
    std::filesystem::path manifestPath;
    std::string manifestHash;
    std::string reproducibility = "not-compared";
    std::map<std::string, std::string> artifactHashes;
    std::vector<DocumentDiagnostic> diagnostics;
    [[nodiscard]] bool Succeeded() const noexcept
    { return state == StandaloneBuildState::Succeeded && published; }
};

using StandaloneBuildExecutor = std::function<StandaloneBuildExecution(
    const PlaySourceSnapshot&, const std::filesystem::path&,
    const StandaloneBuildRequest&)>;

/// Shared snapshot, staging, verification, and publication authority used by
/// the Editor and the command-line entry point. Compilation remains delegated
/// to the existing Play, SagaScript, and SagaPackager authorities.
class StandaloneBuildCoordinator
{
public:
    [[nodiscard]] StandaloneBuildResult Build(
        const StandaloneBuildRequest& request,
        StandaloneBuildExecutor executor = {}) const;
    [[nodiscard]] static std::optional<BuildInputSnapshot> Capture(
        const StandaloneBuildRequest& request,
        std::vector<DocumentDiagnostic>& diagnostics);
    [[nodiscard]] static std::string ClassifyReproducibility(
        const StandaloneBuildResult& first,
        const StandaloneBuildResult& second);
};
} // namespace SagaEditor
