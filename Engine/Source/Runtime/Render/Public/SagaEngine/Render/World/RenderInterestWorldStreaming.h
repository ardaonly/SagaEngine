/// @file RenderInterestWorldStreaming.h
/// @brief CPU-side contract between world interest, render visibility, and preload hints.

#pragma once

#include "SagaEngine/Render/Streaming/RenderStreamingResidency.h"
#include "SagaEngine/Render/World/RenderEntity.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Render::World {

namespace Streaming = ::SagaEngine::Render::Streaming;

enum class RenderInterestState : std::uint8_t
{
    NotRelevant = 0,
    NetworkRelevant,
    StreamingRelevant,
    RenderVisible,
};

enum class RenderVisibilityState : std::uint8_t
{
    Hidden = 0,
    PendingStreaming,
    Visible,
    Fallback,
};

enum class RenderVisibilityLifecycleEvent : std::uint8_t
{
    Discovered = 0,
    PreloadRequested,
    EnteredVisibility,
    FallbackUsed,
    LeftVisibility,
    ReleaseHinted,
};

struct RenderStreamingPreloadHint
{
    RenderEntityId entity = RenderEntityId::kInvalid;
    std::uint64_t assetId = 0;
    Streaming::RenderStreamingAssetKind assetKind =
        Streaming::RenderStreamingAssetKind::Geometry;
    std::uint8_t requestedDetail = 0;
    std::int32_t priorityScore = 0;
};

struct RenderInterestDiagnostic
{
    std::string id;
    RenderEntityId entity = RenderEntityId::kInvalid;
    RenderVisibilityLifecycleEvent event =
        RenderVisibilityLifecycleEvent::Discovered;
    std::string message;
};

struct RenderInterestRecord
{
    RenderEntityId entity = RenderEntityId::kInvalid;
    std::uint32_t networkId = 0;
    RenderInterestState interest = RenderInterestState::NotRelevant;
    RenderVisibilityState visibility = RenderVisibilityState::Hidden;
    bool networkRelevant = false;
    bool renderRelevant = false;
    bool streamingRegionLoaded = false;
    bool resourcesResident = false;
    std::vector<RenderStreamingPreloadHint> preloadHints;
    std::vector<RenderInterestDiagnostic> diagnostics;
};

struct RenderInterestReport
{
    std::vector<RenderInterestRecord> records;
    std::vector<RenderInterestDiagnostic> diagnostics;

    [[nodiscard]] std::string Serialize() const;
};

[[nodiscard]] RenderInterestRecord EvaluateRenderInterestRecord(
    RenderEntityId entity,
    std::uint32_t networkId,
    bool networkRelevant,
    bool renderRelevant,
    bool streamingRegionLoaded,
    bool resourcesResident,
    std::vector<RenderStreamingPreloadHint> preloadHints = {});

[[nodiscard]] RenderInterestReport BuildRenderInterestReport(
    std::vector<RenderInterestRecord> records);

} // namespace SagaEngine::Render::World
