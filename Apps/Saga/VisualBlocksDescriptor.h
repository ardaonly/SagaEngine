/// @file VisualBlocksDescriptor.h
/// @brief Read-only Product Shell generation of a representation catalog.

#pragma once

#include "RuntimeEvidenceReport.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaProduct
{

struct VisualBlocksPort
{
    std::string id;
    std::string name;
    std::string type;
    bool required = false;
};

struct VisualBlocksStateAccess
{
    std::string operation;
    std::string target;
    std::string valueType;
};

struct VisualBlocksBlock
{
    std::string id;
    std::string kind;
    std::string displayName;
    std::string sourceSymbol;
    std::vector<VisualBlocksPort> inputs;
    std::vector<VisualBlocksPort> outputs;
    std::optional<VisualBlocksStateAccess> stateAccess;
    std::vector<std::string> requiredCapabilities;
};

struct VisualBlocksDescriptor
{
    int schemaVersion = 1;
    std::string projectId;
    std::string scriptId;
    std::string typeName;
    std::filesystem::path source;
    std::string sourceHash;
    std::vector<std::string> capabilities;
    std::vector<VisualBlocksBlock> blocks;
    std::vector<std::string> nonClaims;
};

struct VisualBlocksEvidenceInput
{
    EvidenceStatus status = EvidenceStatus::Incomplete;
    std::filesystem::path reportPath;
};

struct VisualBlocksDescriptorGenerationRequest
{
    std::filesystem::path originalProjectManifest;
    std::filesystem::path generatedWorkspace;
    std::filesystem::path bindingManifestPath;
    std::filesystem::path artifactManifestPath;
    VisualBlocksEvidenceInput lifecycleEvidence;
    VisualBlocksEvidenceInput gameplayEvidence;
    std::filesystem::path reportPath;
};

struct VisualBlocksDescriptorGenerationResult
{
    EvidenceStatus status = EvidenceStatus::Incomplete;
    std::filesystem::path reportPath;
    std::filesystem::path sourcePath;
    std::string projectId;
    std::string scriptId;
    std::string typeName;
    std::string sourceHash;
    std::optional<VisualBlocksDescriptor> descriptor;
    std::vector<std::string> generatedBlockIds;
    std::vector<std::string> observedCallbacks;
    std::vector<std::string> matchedGameplayMutations;
    std::vector<std::string> requiredCapabilities;
    std::vector<std::string> nonClaims;
    std::vector<FirstPlayableDiagnostic> diagnostics;
};

inline constexpr const char* kVisualBlocksDescriptorProfileId =
    "starter-arena-visual-blocks-descriptor";

[[nodiscard]] VisualBlocksDescriptorGenerationResult
GenerateVisualBlocksDescriptorReport(
    const VisualBlocksDescriptorGenerationRequest& request);

} // namespace SagaProduct
