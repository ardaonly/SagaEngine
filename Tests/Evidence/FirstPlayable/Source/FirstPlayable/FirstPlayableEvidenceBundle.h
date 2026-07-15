/// @file FirstPlayableEvidenceBundle.h
/// @brief Writes durable first-playable source and evidence manifests.
#pragma once

#include "FirstPlayable/FirstPlayableGate.h"

#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <vector>

namespace SagaProduct
{
struct FirstPlayableEvidenceBundleResult
{
    EvidenceStatus status = EvidenceStatus::Incomplete;
    std::filesystem::path sourceManifestPath;
    std::filesystem::path evidenceManifestPath;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

class FirstPlayableEvidenceBundle final
{
public:
    [[nodiscard]] static bool WriteJson(
        const std::filesystem::path& path,
        const nlohmann::json& value,
        std::vector<FirstPlayableDiagnostic>& diagnostics,
        const std::string& code);
    [[nodiscard]] static FirstPlayableEvidenceBundleResult Write(
        const std::filesystem::path& outputRoot,
        const RuntimeEvidenceRunResult& runtime,
        const VisualBlocksDescriptorGenerationResult& descriptor,
        const FirstPlayableGateResult& gate,
        const std::filesystem::path& summaryPath,
        const std::filesystem::path& gatePath);
};
} // namespace SagaProduct
