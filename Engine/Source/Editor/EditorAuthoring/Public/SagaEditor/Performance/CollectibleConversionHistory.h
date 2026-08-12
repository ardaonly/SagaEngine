// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{
enum class CollectibleConversionHistoryState
{
    Applied,
    Reversed,
    DetachedRequiresReview,
};

struct CollectibleConversionHistoryEntry
{
    std::string operationId;
    CollectibleConversionHistoryState state =
        CollectibleConversionHistoryState::DetachedRequiresReview;
    std::string planHash;
    std::string sourceHash;
    std::string resultingSourceHash;
    std::string projectHash;
    std::string packageGraphHash;
    std::string projectGeneration;
    std::string toolIdentity;
    std::string provenanceHash;
};

struct CollectibleConversionHistoryResult
{
    bool succeeded = false;
    std::string code;
    std::string message;
    std::string candidateBytes;
    std::vector<CollectibleConversionHistoryEntry> entries;
};

/// Qt-free durable history serializer. It prepares canonical candidate bytes;
/// the conversion controller publishes those bytes in the same project
/// mutation transaction as source, project, lock, and provenance changes.
class CollectibleConversionHistory final
{
public:
    [[nodiscard]] static std::filesystem::path Path(
        const std::filesystem::path& projectRoot);
    [[nodiscard]] static CollectibleConversionHistoryResult Append(
        const std::filesystem::path& historyPath,
        const CollectibleConversionHistoryEntry& entry,
        std::optional<std::string> replacementDocument = std::nullopt);
    [[nodiscard]] static CollectibleConversionHistoryResult Read(
        const std::filesystem::path& historyPath);
};
} // namespace SagaEditor
