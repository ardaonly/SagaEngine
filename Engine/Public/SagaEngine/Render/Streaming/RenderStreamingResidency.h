/// @file RenderStreamingResidency.h
/// @brief CPU-side render residency policy and diagnostics.
///
/// Layer  : SagaEngine / Render / Streaming
/// Purpose: Render systems need texture mip and geometry LOD decisions that
///          are more specific than the generic byte cache in Resources. This
///          header defines the render-facing policy DTOs and pure evaluation
///          functions. The Resources streaming manager remains the owner of
///          asset bytes; this layer only decides what render detail should be
///          requested, what fallback is acceptable, and how to report memory
///          pressure.

#pragma once

#include "SagaEngine/Render/Materials/Material.h"
#include "SagaEngine/Resources/StreamingRequest.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Render::Streaming {

inline constexpr std::uint8_t kRenderMaxTextureMips = 16;
inline constexpr std::uint8_t kRenderMaxGeometryLods = 4;

enum class RenderStreamingAssetKind : std::uint8_t
{
    Texture = 0,
    Geometry,
};

enum class RenderTextureResidencyState : std::uint8_t
{
    Missing = 0,
    Requested,
    Resident,
    Fallback,
};

enum class RenderMeshResidencyState : std::uint8_t
{
    Missing = 0,
    Requested,
    Resident,
    Fallback,
};

enum class RenderGeometryCategory : std::uint8_t
{
    StaticMesh = 0,
    SkinnedMesh,
    TerrainChunk,
};

enum class RenderBudgetPressure : std::uint8_t
{
    WithinBudget = 0,
    SoftLimitExceeded,
    HardLimitExceeded,
};

struct RenderTextureMipRange
{
    std::uint8_t firstMip = 0;
    std::uint8_t lastMip = 0;
    bool valid = false;

    [[nodiscard]] bool Contains(std::uint8_t mip) const noexcept
    {
        return valid && mip >= firstMip && mip <= lastMip;
    }
};

struct RenderTexturePoolBudget
{
    std::uint64_t softLimitBytes = 0;
    std::uint64_t hardLimitBytes = 0;
    std::uint64_t currentResidentBytes = 0;
    std::uint64_t requestedBytes = 0;
};

struct RenderGeometryPoolBudget
{
    std::uint64_t softLimitBytes = 0;
    std::uint64_t hardLimitBytes = 0;
    std::uint64_t currentResidentBytes = 0;
    std::uint64_t requestedBytes = 0;
};

struct RenderStreamingDiagnostic
{
    std::string id;
    std::uint64_t assetId = 0;
    RenderStreamingAssetKind kind = RenderStreamingAssetKind::Texture;
    std::uint8_t detailLevel = 0;
    std::string message;
};

struct RenderStreamingPriorityInput
{
    std::uint64_t assetId = 0;
    RenderStreamingAssetKind kind = RenderStreamingAssetKind::Texture;
    bool criticalVisible = false;
    float cameraDistance = 0.0f;
    float materialImportance = 0.0f;
    std::uint8_t urgency = 0;
    std::uint64_t estimatedBytes = 0;
};

struct RenderStreamingPriorityResult
{
    std::uint64_t assetId = 0;
    RenderStreamingAssetKind kind = RenderStreamingAssetKind::Texture;
    std::int32_t score = 0;
    Resources::StreamingPriority priority = Resources::StreamingPriority::Normal;
};

struct RenderTextureResidencyInput
{
    std::uint64_t assetId = 0;
    float cameraDistance = 0.0f;
    float materialImportance = 0.0f;
    bool criticalVisible = false;
    MaterialTextureResidencyHint residencyHint = MaterialTextureResidencyHint::Default;
    std::uint8_t authoredMipCount = 1;
    RenderTextureMipRange availableMipRange{};
    RenderTextureMipRange residentMipRange{};
    RenderTextureMipRange inFlightMipRange{};
    std::uint64_t estimatedResidentBytes = 0;
    std::uint64_t estimatedRequestBytes = 0;
};

struct RenderTextureStreamingDecision
{
    std::uint64_t assetId = 0;
    RenderTextureResidencyState state = RenderTextureResidencyState::Missing;
    RenderTextureMipRange requestedMipRange{};
    RenderTextureMipRange loadedMipRange{};
    std::uint8_t targetMip = 0;
    std::uint8_t fallbackMip = 0;
    bool usesPlaceholder = false;
    RenderStreamingPriorityResult priority{};
    std::uint64_t residentBytes = 0;
    std::uint64_t requestedBytes = 0;
    std::vector<RenderStreamingDiagnostic> diagnostics;
};

struct RenderMeshLodResidency
{
    std::uint8_t lod = 0;
    RenderMeshResidencyState state = RenderMeshResidencyState::Missing;
    std::uint64_t residentBytes = 0;
    std::uint64_t requestedBytes = 0;
};

struct RenderGeometryStreamingInput
{
    std::uint64_t assetId = 0;
    RenderGeometryCategory category = RenderGeometryCategory::StaticMesh;
    float cameraDistance = 0.0f;
    bool criticalVisible = false;
    float materialImportance = 0.0f;
    bool meshAvailable = true;
    std::uint8_t lodCount = 1;
    std::uint8_t residentLodMask = 0;
    std::uint8_t availableLodMask = 1;
    std::uint8_t inFlightLodMask = 0;
    std::array<std::uint64_t, kRenderMaxGeometryLods> lodBytes{};
};

struct RenderGeometryStreamingDecision
{
    std::uint64_t assetId = 0;
    RenderGeometryCategory category = RenderGeometryCategory::StaticMesh;
    RenderMeshResidencyState state = RenderMeshResidencyState::Missing;
    std::uint8_t targetLod = 0;
    std::uint8_t fallbackLod = 0;
    bool usesProxy = false;
    RenderStreamingPriorityResult priority{};
    std::uint64_t residentBytes = 0;
    std::uint64_t requestedBytes = 0;
    std::vector<RenderMeshLodResidency> lodResidency;
    std::vector<RenderStreamingDiagnostic> diagnostics;
};

struct RenderGeometryEvictionCandidate
{
    std::uint64_t assetId = 0;
    std::uint8_t lod = 0;
    std::uint64_t bytes = 0;
    std::int32_t evictionScore = 0;
};

struct RenderTextureMemoryReport
{
    RenderTexturePoolBudget budget{};
    RenderBudgetPressure pressure = RenderBudgetPressure::WithinBudget;
    std::uint64_t residentBytes = 0;
    std::uint64_t requestedBytes = 0;
    std::uint64_t overflowBytes = 0;
    std::vector<RenderTextureStreamingDecision> decisions;
    std::vector<RenderStreamingDiagnostic> diagnostics;
};

struct RenderGeometryMemoryReport
{
    RenderGeometryPoolBudget budget{};
    RenderBudgetPressure pressure = RenderBudgetPressure::WithinBudget;
    std::uint64_t residentBytes = 0;
    std::uint64_t requestedBytes = 0;
    std::uint64_t overflowBytes = 0;
    std::vector<RenderGeometryStreamingDecision> decisions;
    std::vector<RenderGeometryEvictionCandidate> evictionCandidates;
    std::vector<RenderStreamingDiagnostic> diagnostics;
};

struct RenderResidencyReport
{
    RenderTextureMemoryReport texture;
    RenderGeometryMemoryReport geometry;
    std::vector<RenderStreamingPriorityResult> priorities;
    std::vector<RenderStreamingDiagnostic> diagnostics;

    [[nodiscard]] std::string Serialize() const;
};

[[nodiscard]] RenderBudgetPressure EvaluateRenderBudgetPressure(
    std::uint64_t bytes,
    std::uint64_t softLimitBytes,
    std::uint64_t hardLimitBytes) noexcept;

[[nodiscard]] RenderStreamingPriorityResult EvaluateRenderStreamingPriority(
    const RenderStreamingPriorityInput& input) noexcept;

[[nodiscard]] std::vector<RenderStreamingPriorityResult>
SortRenderStreamingPriorities(std::vector<RenderStreamingPriorityInput> inputs);

[[nodiscard]] RenderTextureStreamingDecision EvaluateRenderTextureResidency(
    const RenderTextureResidencyInput& input,
    const RenderTexturePoolBudget& budget);

[[nodiscard]] RenderGeometryStreamingDecision EvaluateRenderGeometryResidency(
    const RenderGeometryStreamingInput& input,
    const RenderGeometryPoolBudget& budget);

[[nodiscard]] RenderTextureMemoryReport BuildRenderTextureMemoryReport(
    std::vector<RenderTextureStreamingDecision> decisions,
    RenderTexturePoolBudget budget);

[[nodiscard]] RenderGeometryMemoryReport BuildRenderGeometryMemoryReport(
    std::vector<RenderGeometryStreamingDecision> decisions,
    RenderGeometryPoolBudget budget);

[[nodiscard]] RenderResidencyReport BuildRenderResidencyReport(
    std::vector<RenderTextureStreamingDecision> textureDecisions,
    RenderTexturePoolBudget textureBudget,
    std::vector<RenderGeometryStreamingDecision> geometryDecisions,
    RenderGeometryPoolBudget geometryBudget);

} // namespace SagaEngine::Render::Streaming
