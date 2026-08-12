// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Performance/CollectibleConversionHistory.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <algorithm>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::ordered_json;

const char* Name(CollectibleConversionHistoryState state)
{
    switch (state)
    {
    case CollectibleConversionHistoryState::Applied: return "Applied";
    case CollectibleConversionHistoryState::Reversed: return "Reversed";
    case CollectibleConversionHistoryState::DetachedRequiresReview:
        return "DetachedRequiresReview";
    }
    return "DetachedRequiresReview";
}

std::optional<CollectibleConversionHistoryState> ParseState(std::string_view value)
{
    if (value == "Applied") return CollectibleConversionHistoryState::Applied;
    if (value == "Reversed") return CollectibleConversionHistoryState::Reversed;
    if (value == "DetachedRequiresReview")
        return CollectibleConversionHistoryState::DetachedRequiresReview;
    return std::nullopt;
}

bool IsHash(std::string_view value)
{
    if (!value.starts_with("sha256:")) return false;
    value.remove_prefix(7);
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char character)
    { return (character >= '0' && character <= '9') ||
             (character >= 'a' && character <= 'f'); });
}

bool Valid(const CollectibleConversionHistoryEntry& entry)
{
    return IsHash(entry.operationId) && IsHash(entry.planHash) &&
        IsHash(entry.sourceHash) && IsHash(entry.resultingSourceHash) &&
        IsHash(entry.projectHash) && IsHash(entry.packageGraphHash) &&
        IsHash(entry.projectGeneration) && !entry.toolIdentity.empty() &&
        IsHash(entry.provenanceHash);
}

Json ToJson(const CollectibleConversionHistoryEntry& entry)
{
    return {{"operationId", entry.operationId}, {"packageGraphHash", entry.packageGraphHash},
        {"planHash", entry.planHash}, {"projectGeneration", entry.projectGeneration},
        {"projectHash", entry.projectHash}, {"provenanceHash", entry.provenanceHash},
        {"resultingSourceHash", entry.resultingSourceHash}, {"sourceHash", entry.sourceHash},
        {"state", Name(entry.state)}, {"toolIdentity", entry.toolIdentity}};
}

CollectibleConversionHistoryResult Fail(std::string code, std::string message)
{ return {false, std::move(code), std::move(message), {}, {}}; }
}

std::filesystem::path CollectibleConversionHistory::Path(
    const std::filesystem::path& projectRoot)
{ return projectRoot / "CollectibleConversion.saga.json"; }

CollectibleConversionHistoryResult CollectibleConversionHistory::Read(
    const std::filesystem::path& historyPath)
{
    CollectibleConversionHistoryResult result;
    Json root = Json::object();
    std::error_code error;
    if (std::filesystem::exists(historyPath, error))
    {
        std::ifstream input(historyPath, std::ios::binary);
        try { root = Json::parse(input); }
        catch (const std::exception& exception)
        { return Fail("Conversion.HistoryInvalid", exception.what()); }
    }
    try
    {
    if (!root.is_object())
        return Fail("Conversion.HistoryInvalid", "Conversion provenance is invalid.");
    const Json emptyHistory{{"entries", Json::array()},
        {"schema", "saga.collectible-conversion-history.v1"}};
    const auto history = root.value("conversionHistory", emptyHistory);
    if (!history.is_object() || history.size() != 2 ||
        history.value("schema", std::string{}) != "saga.collectible-conversion-history.v1" ||
        !history.contains("entries") || !history["entries"].is_array())
        return Fail("Conversion.HistoryInvalid", "Conversion history schema is invalid.");
    for (const auto& value : history["entries"])
    {
        if (!value.is_object() || value.size() != 10 ||
            !ParseState(value.value("state", std::string{})))
            return Fail("Conversion.HistoryInvalid", "Conversion history entry is invalid.");
        CollectibleConversionHistoryEntry entry{value.value("operationId", ""),
            *ParseState(value.value("state", std::string{})), value.value("planHash", ""),
            value.value("sourceHash", ""), value.value("resultingSourceHash", ""),
            value.value("projectHash", ""), value.value("packageGraphHash", ""),
            value.value("projectGeneration", ""), value.value("toolIdentity", ""),
            value.value("provenanceHash", "")};
        if (!Valid(entry))
            return Fail("Conversion.HistoryInvalid",
                "Conversion history identity is invalid.");
        result.entries.push_back(std::move(entry));
    }
    result.succeeded = true;
    result.candidateBytes = root.dump(2) + "\n";
    return result;
    }
    catch (const std::exception& exception)
    { return Fail("Conversion.HistoryInvalid", exception.what()); }
}

CollectibleConversionHistoryResult CollectibleConversionHistory::Append(
    const std::filesystem::path& historyPath,
    const CollectibleConversionHistoryEntry& entry,
    std::optional<std::string> replacementDocument)
{
    auto result = Read(historyPath);
    if (!result.succeeded) return result;
    if (!Valid(entry))
        return Fail("Conversion.HistoryIdentityMissing",
            "Conversion history requires all plan, source, package, tool, and provenance identities.");
    if (!result.entries.empty() &&
        result.entries.back().operationId == entry.operationId &&
        result.entries.back().state == entry.state)
        return result;
    result.entries.push_back(entry);
    Json root;
    try { root = replacementDocument ? Json::parse(*replacementDocument) :
        Json::parse(result.candidateBytes); }
    catch (const std::exception& exception)
    { return Fail("Conversion.HistoryInvalid", exception.what()); }
    Json entries = Json::array();
    for (const auto& value : result.entries) entries.push_back(ToJson(value));
    root["conversionHistory"] = {{"entries", std::move(entries)},
        {"schema", "saga.collectible-conversion-history.v1"}};
    result.candidateBytes = root.dump(2) + "\n";
    return result;
}
} // namespace SagaEditor
