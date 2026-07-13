/// @file FirstPlayableWorkspacePolicy.h
/// @brief Source-integrity and generated-output policy for the RC gate.
#pragma once

#include "Reports/RuntimeEvidenceReport.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace SagaProduct
{
struct FirstPlayableSourceFile
{
    std::filesystem::path relativePath;
    std::string beforeSha256;
    std::string afterSha256;
    bool unchanged = false;
};

struct FirstPlayableWorkspacePolicySession
{
    EvidenceStatus status = EvidenceStatus::Incomplete;
    std::filesystem::path projectManifest;
    std::filesystem::path projectRoot;
    std::filesystem::path outputRoot;
    std::filesystem::path summaryPath;
    std::map<std::string, std::string> beforeTree;
    std::vector<FirstPlayableSourceFile> sourceFiles;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

struct FirstPlayableWorkspacePolicyResult
{
    EvidenceStatus status = EvidenceStatus::Incomplete;
    EvidenceStatus sourceIntegrity = EvidenceStatus::Incomplete;
    std::filesystem::path projectRoot;
    std::filesystem::path outputRoot;
    bool sampleSourceModified = false;
    bool sagaPrivateAccessedByWorkflow = false;
    std::vector<std::string> generatedFilesInsideSample;
    std::vector<FirstPlayableSourceFile> sourceFiles;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

class FirstPlayableWorkspacePolicy final
{
public:
    [[nodiscard]] static FirstPlayableWorkspacePolicySession Begin(
        const std::filesystem::path& projectManifest,
        const std::filesystem::path& outputRoot,
        const std::filesystem::path& summaryPath);
    [[nodiscard]] static FirstPlayableWorkspacePolicyResult Complete(
        const FirstPlayableWorkspacePolicySession& session);
};
} // namespace SagaProduct
