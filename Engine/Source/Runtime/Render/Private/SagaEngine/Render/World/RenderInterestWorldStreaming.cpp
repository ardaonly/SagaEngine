#include "SagaEngine/Render/World/RenderInterestWorldStreaming.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace SagaEngine::Render::World {
namespace {

[[nodiscard]] std::uint32_t ToEntityKey(RenderEntityId id) noexcept
{
    return static_cast<std::uint32_t>(id);
}

[[nodiscard]] const char* ToString(RenderInterestState state) noexcept
{
    switch (state)
    {
        case RenderInterestState::NotRelevant: return "not_relevant";
        case RenderInterestState::NetworkRelevant: return "network_relevant";
        case RenderInterestState::StreamingRelevant: return "streaming_relevant";
        case RenderInterestState::RenderVisible: return "render_visible";
    }
    return "unknown";
}

[[nodiscard]] const char* ToString(RenderVisibilityState state) noexcept
{
    switch (state)
    {
        case RenderVisibilityState::Hidden: return "hidden";
        case RenderVisibilityState::PendingStreaming: return "pending_streaming";
        case RenderVisibilityState::Visible: return "visible";
        case RenderVisibilityState::Fallback: return "fallback";
    }
    return "unknown";
}

[[nodiscard]] const char* ToString(RenderVisibilityLifecycleEvent event) noexcept
{
    switch (event)
    {
        case RenderVisibilityLifecycleEvent::Discovered: return "discovered";
        case RenderVisibilityLifecycleEvent::PreloadRequested: return "preload_requested";
        case RenderVisibilityLifecycleEvent::EnteredVisibility: return "entered_visibility";
        case RenderVisibilityLifecycleEvent::FallbackUsed: return "fallback_used";
        case RenderVisibilityLifecycleEvent::LeftVisibility: return "left_visibility";
        case RenderVisibilityLifecycleEvent::ReleaseHinted: return "release_hinted";
    }
    return "unknown";
}

[[nodiscard]] const char* ToString(
    Streaming::RenderStreamingAssetKind kind) noexcept
{
    switch (kind)
    {
        case Streaming::RenderStreamingAssetKind::Texture: return "texture";
        case Streaming::RenderStreamingAssetKind::Geometry: return "geometry";
    }
    return "unknown";
}

void AppendDiagnostic(std::vector<RenderInterestDiagnostic>& diagnostics,
                      std::string id,
                      RenderEntityId entity,
                      RenderVisibilityLifecycleEvent event,
                      std::string message)
{
    RenderInterestDiagnostic diagnostic;
    diagnostic.id = std::move(id);
    diagnostic.entity = entity;
    diagnostic.event = event;
    diagnostic.message = std::move(message);
    diagnostics.push_back(std::move(diagnostic));
}

void SortDiagnostics(std::vector<RenderInterestDiagnostic>& diagnostics)
{
    std::stable_sort(
        diagnostics.begin(),
        diagnostics.end(),
        [](const RenderInterestDiagnostic& lhs,
           const RenderInterestDiagnostic& rhs)
        {
            if (lhs.id != rhs.id) return lhs.id < rhs.id;
            if (lhs.entity != rhs.entity)
            {
                return ToEntityKey(lhs.entity) < ToEntityKey(rhs.entity);
            }
            return lhs.event < rhs.event;
        });
}

} // namespace

RenderInterestRecord EvaluateRenderInterestRecord(
    RenderEntityId entity,
    std::uint32_t networkId,
    bool networkRelevant,
    bool renderRelevant,
    bool streamingRegionLoaded,
    bool resourcesResident,
    std::vector<RenderStreamingPreloadHint> preloadHints)
{
    RenderInterestRecord record;
    record.entity = entity;
    record.networkId = networkId;
    record.networkRelevant = networkRelevant;
    record.renderRelevant = renderRelevant;
    record.streamingRegionLoaded = streamingRegionLoaded;
    record.resourcesResident = resourcesResident;
    record.preloadHints = std::move(preloadHints);

    if (!networkRelevant && !renderRelevant)
    {
        record.interest = RenderInterestState::NotRelevant;
        record.visibility = RenderVisibilityState::Hidden;
        AppendDiagnostic(
            record.diagnostics,
            "RenderInterest.NotRelevant",
            entity,
            RenderVisibilityLifecycleEvent::LeftVisibility,
            "Entity is outside network and render relevance.");
        return record;
    }

    if (networkRelevant && !renderRelevant)
    {
        record.interest = RenderInterestState::NetworkRelevant;
        record.visibility = RenderVisibilityState::Hidden;
        AppendDiagnostic(
            record.diagnostics,
            "RenderInterest.NetworkOnly",
            entity,
            RenderVisibilityLifecycleEvent::Discovered,
            "Network relevance does not imply render visibility.");
        return record;
    }

    if (!streamingRegionLoaded)
    {
        record.interest = RenderInterestState::StreamingRelevant;
        record.visibility = RenderVisibilityState::PendingStreaming;
        AppendDiagnostic(
            record.diagnostics,
            "RenderInterest.PreloadHint",
            entity,
            RenderVisibilityLifecycleEvent::PreloadRequested,
            "Render-relevant entity is waiting for client streaming region.");
        return record;
    }

    if (!resourcesResident)
    {
        record.interest = RenderInterestState::RenderVisible;
        record.visibility = RenderVisibilityState::Fallback;
        AppendDiagnostic(
            record.diagnostics,
            "RenderInterest.UnloadedZoneFallback",
            entity,
            RenderVisibilityLifecycleEvent::FallbackUsed,
            "Render-relevant entity uses unloaded-zone fallback policy.");
        return record;
    }

    record.interest = RenderInterestState::RenderVisible;
    record.visibility = RenderVisibilityState::Visible;
    AppendDiagnostic(
        record.diagnostics,
        "RenderInterest.EnteredVisibility",
        entity,
        RenderVisibilityLifecycleEvent::EnteredVisibility,
        "Entity entered render visibility with resident resources.");
    return record;
}

RenderInterestReport BuildRenderInterestReport(
    std::vector<RenderInterestRecord> records)
{
    RenderInterestReport report;
    report.records = std::move(records);
    std::stable_sort(
        report.records.begin(),
        report.records.end(),
        [](const RenderInterestRecord& lhs, const RenderInterestRecord& rhs)
        {
            if (lhs.entity != rhs.entity)
            {
                return ToEntityKey(lhs.entity) < ToEntityKey(rhs.entity);
            }
            return lhs.networkId < rhs.networkId;
        });

    for (const auto& record : report.records)
    {
        report.diagnostics.insert(
            report.diagnostics.end(),
            record.diagnostics.begin(),
            record.diagnostics.end());
    }
    SortDiagnostics(report.diagnostics);
    return report;
}

std::string RenderInterestReport::Serialize() const
{
    std::ostringstream out;
    out << "render_interest_report_v1\n";

    auto sortedRecords = records;
    std::stable_sort(
        sortedRecords.begin(),
        sortedRecords.end(),
        [](const RenderInterestRecord& lhs, const RenderInterestRecord& rhs)
        {
            if (lhs.entity != rhs.entity)
            {
                return ToEntityKey(lhs.entity) < ToEntityKey(rhs.entity);
            }
            return lhs.networkId < rhs.networkId;
        });

    for (auto& record : sortedRecords)
    {
        std::stable_sort(
            record.preloadHints.begin(),
            record.preloadHints.end(),
            [](const RenderStreamingPreloadHint& lhs,
               const RenderStreamingPreloadHint& rhs)
            {
                if (lhs.priorityScore != rhs.priorityScore)
                {
                    return lhs.priorityScore > rhs.priorityScore;
                }
                if (lhs.assetKind != rhs.assetKind) return lhs.assetKind < rhs.assetKind;
                if (lhs.assetId != rhs.assetId) return lhs.assetId < rhs.assetId;
                return lhs.requestedDetail < rhs.requestedDetail;
            });

        out << "record entity=" << ToEntityKey(record.entity)
            << " networkId=" << record.networkId
            << " interest=" << ToString(record.interest)
            << " visibility=" << ToString(record.visibility)
            << " networkRelevant=" << (record.networkRelevant ? "true" : "false")
            << " renderRelevant=" << (record.renderRelevant ? "true" : "false")
            << " streamingLoaded=" << (record.streamingRegionLoaded ? "true" : "false")
            << " resourcesResident=" << (record.resourcesResident ? "true" : "false")
            << '\n';

        for (const auto& hint : record.preloadHints)
        {
            out << "preload entity=" << ToEntityKey(hint.entity)
                << " asset=" << hint.assetId
                << " kind=" << ToString(hint.assetKind)
                << " detail=" << static_cast<unsigned>(hint.requestedDetail)
                << " score=" << hint.priorityScore << '\n';
        }
    }

    auto sortedDiagnostics = diagnostics;
    SortDiagnostics(sortedDiagnostics);
    for (const auto& diagnostic : sortedDiagnostics)
    {
        out << "diagnostic id=" << diagnostic.id
            << " entity=" << ToEntityKey(diagnostic.entity)
            << " event=" << ToString(diagnostic.event)
            << " message=" << std::quoted(diagnostic.message) << '\n';
    }

    return out.str();
}

} // namespace SagaEngine::Render::World
