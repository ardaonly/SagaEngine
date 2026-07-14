#include "SagaEngine/Render/Streaming/RenderStreamingResidency.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

namespace SagaEngine::Render::Streaming {
namespace {

[[nodiscard]] float Clamp01(float value) noexcept
{
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] std::uint8_t ClampMip(std::uint8_t mip, std::uint8_t count) noexcept
{
    if (count == 0)
    {
        return 0;
    }
    return std::min<std::uint8_t>(mip, static_cast<std::uint8_t>(count - 1));
}

[[nodiscard]] bool HasLod(std::uint8_t mask, std::uint8_t lod) noexcept
{
    return lod < 8 && (mask & (1u << lod)) != 0;
}

[[nodiscard]] const char* ToString(RenderStreamingAssetKind kind) noexcept
{
    switch (kind)
    {
        case RenderStreamingAssetKind::Texture: return "texture";
        case RenderStreamingAssetKind::Geometry: return "geometry";
    }
    return "unknown";
}

[[nodiscard]] const char* ToString(RenderTextureResidencyState state) noexcept
{
    switch (state)
    {
        case RenderTextureResidencyState::Missing: return "missing";
        case RenderTextureResidencyState::Requested: return "requested";
        case RenderTextureResidencyState::Resident: return "resident";
        case RenderTextureResidencyState::Fallback: return "fallback";
    }
    return "unknown";
}

[[nodiscard]] const char* ToString(RenderMeshResidencyState state) noexcept
{
    switch (state)
    {
        case RenderMeshResidencyState::Missing: return "missing";
        case RenderMeshResidencyState::Requested: return "requested";
        case RenderMeshResidencyState::Resident: return "resident";
        case RenderMeshResidencyState::Fallback: return "fallback";
    }
    return "unknown";
}

[[nodiscard]] const char* ToString(RenderBudgetPressure pressure) noexcept
{
    switch (pressure)
    {
        case RenderBudgetPressure::WithinBudget: return "within";
        case RenderBudgetPressure::SoftLimitExceeded: return "soft";
        case RenderBudgetPressure::HardLimitExceeded: return "hard";
    }
    return "unknown";
}

[[nodiscard]] const char* ToString(RenderGeometryCategory category) noexcept
{
    switch (category)
    {
        case RenderGeometryCategory::StaticMesh: return "static";
        case RenderGeometryCategory::SkinnedMesh: return "skinned";
        case RenderGeometryCategory::TerrainChunk: return "terrain";
    }
    return "unknown";
}

[[nodiscard]] const char* ToString(Resources::StreamingPriority priority) noexcept
{
    switch (priority)
    {
        case Resources::StreamingPriority::Critical: return "critical";
        case Resources::StreamingPriority::High: return "high";
        case Resources::StreamingPriority::Normal: return "normal";
        case Resources::StreamingPriority::Low: return "low";
    }
    return "unknown";
}

void SortDiagnostics(std::vector<RenderStreamingDiagnostic>& diagnostics)
{
    std::stable_sort(
        diagnostics.begin(),
        diagnostics.end(),
        [](const RenderStreamingDiagnostic& lhs, const RenderStreamingDiagnostic& rhs)
        {
            if (lhs.id != rhs.id) return lhs.id < rhs.id;
            if (lhs.assetId != rhs.assetId) return lhs.assetId < rhs.assetId;
            if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
            return lhs.detailLevel < rhs.detailLevel;
        });
}

void AppendDiagnostic(std::vector<RenderStreamingDiagnostic>& diagnostics,
                      std::string id,
                      std::uint64_t assetId,
                      RenderStreamingAssetKind kind,
                      std::uint8_t detailLevel,
                      std::string message)
{
    RenderStreamingDiagnostic diagnostic;
    diagnostic.id = std::move(id);
    diagnostic.assetId = assetId;
    diagnostic.kind = kind;
    diagnostic.detailLevel = detailLevel;
    diagnostic.message = std::move(message);
    diagnostics.push_back(std::move(diagnostic));
}

[[nodiscard]] std::uint8_t DesiredTextureMip(
    const RenderTextureResidencyInput& input,
    RenderBudgetPressure pressure) noexcept
{
    std::uint8_t mip = 0;
    const float importance = Clamp01(input.materialImportance);

    if (!input.criticalVisible &&
        input.residencyHint != MaterialTextureResidencyHint::AlwaysResident)
    {
        if (input.cameraDistance >= 480.0f || importance <= 0.15f)
        {
            mip = 3;
        }
        else if (input.cameraDistance >= 180.0f || importance <= 0.35f)
        {
            mip = 2;
        }
        else if (input.cameraDistance >= 64.0f || importance <= 0.60f)
        {
            mip = 1;
        }
    }

    if (input.residencyHint == MaterialTextureResidencyHint::Streamed)
    {
        mip = std::max<std::uint8_t>(mip, 1);
    }
    else if (input.residencyHint == MaterialTextureResidencyHint::PlaceholderAllowed)
    {
        mip = std::min<std::uint8_t>(
            static_cast<std::uint8_t>(mip + 1),
            kRenderMaxTextureMips - 1);
    }
    else if (input.residencyHint == MaterialTextureResidencyHint::AlwaysResident)
    {
        mip = 0;
    }

    if (pressure == RenderBudgetPressure::SoftLimitExceeded)
    {
        mip = static_cast<std::uint8_t>(mip + 1);
    }
    else if (pressure == RenderBudgetPressure::HardLimitExceeded)
    {
        mip = static_cast<std::uint8_t>(mip + 2);
    }

    return ClampMip(mip, input.authoredMipCount);
}

[[nodiscard]] std::uint8_t DesiredGeometryLod(
    const RenderGeometryStreamingInput& input,
    RenderBudgetPressure pressure) noexcept
{
    std::uint8_t lod = 0;
    const float importance = Clamp01(input.materialImportance);

    if (!input.criticalVisible)
    {
        if (input.cameraDistance >= 640.0f || importance <= 0.15f)
        {
            lod = 3;
        }
        else if (input.cameraDistance >= 240.0f || importance <= 0.35f)
        {
            lod = 2;
        }
        else if (input.cameraDistance >= 80.0f || importance <= 0.60f)
        {
            lod = 1;
        }
    }

    if (input.category == RenderGeometryCategory::SkinnedMesh && lod > 0)
    {
        --lod;
    }
    else if (input.category == RenderGeometryCategory::TerrainChunk && !input.criticalVisible)
    {
        lod = static_cast<std::uint8_t>(lod + 1);
    }

    if (pressure == RenderBudgetPressure::SoftLimitExceeded)
    {
        lod = static_cast<std::uint8_t>(lod + 1);
    }
    else if (pressure == RenderBudgetPressure::HardLimitExceeded)
    {
        lod = static_cast<std::uint8_t>(lod + 2);
    }

    const auto count = std::min<std::uint8_t>(
        std::max<std::uint8_t>(input.lodCount, 1),
        kRenderMaxGeometryLods);
    return std::min<std::uint8_t>(lod, static_cast<std::uint8_t>(count - 1));
}

[[nodiscard]] std::uint8_t FirstCoarserAvailableMip(
    RenderTextureMipRange range,
    std::uint8_t target) noexcept
{
    if (!range.valid)
    {
        return std::numeric_limits<std::uint8_t>::max();
    }

    const auto first = std::max(target, range.firstMip);
    if (first <= range.lastMip)
    {
        return first;
    }

    return std::numeric_limits<std::uint8_t>::max();
}

[[nodiscard]] std::uint8_t FirstCoarserAvailableLod(
    std::uint8_t mask,
    std::uint8_t target,
    std::uint8_t lodCount) noexcept
{
    const auto count = std::min<std::uint8_t>(lodCount, kRenderMaxGeometryLods);
    for (std::uint8_t lod = target; lod < count; ++lod)
    {
        if (HasLod(mask, lod))
        {
            return lod;
        }
    }
    return std::numeric_limits<std::uint8_t>::max();
}

void AppendTexture(std::ostringstream& out,
                   const RenderTextureStreamingDecision& decision)
{
    out << "texture asset=" << decision.assetId
        << " state=" << ToString(decision.state)
        << " targetMip=" << static_cast<unsigned>(decision.targetMip)
        << " fallbackMip=" << static_cast<unsigned>(decision.fallbackMip)
        << " placeholder=" << (decision.usesPlaceholder ? "true" : "false")
        << " residentBytes=" << decision.residentBytes
        << " requestedBytes=" << decision.requestedBytes
        << " priority=" << ToString(decision.priority.priority)
        << " score=" << decision.priority.score << '\n';
}

void AppendGeometry(std::ostringstream& out,
                    const RenderGeometryStreamingDecision& decision)
{
    out << "geometry asset=" << decision.assetId
        << " category=" << ToString(decision.category)
        << " state=" << ToString(decision.state)
        << " targetLod=" << static_cast<unsigned>(decision.targetLod)
        << " fallbackLod=" << static_cast<unsigned>(decision.fallbackLod)
        << " proxy=" << (decision.usesProxy ? "true" : "false")
        << " residentBytes=" << decision.residentBytes
        << " requestedBytes=" << decision.requestedBytes
        << " priority=" << ToString(decision.priority.priority)
        << " score=" << decision.priority.score << '\n';
}

} // namespace

RenderBudgetPressure EvaluateRenderBudgetPressure(
    std::uint64_t bytes,
    std::uint64_t softLimitBytes,
    std::uint64_t hardLimitBytes) noexcept
{
    if (hardLimitBytes > 0 && bytes > hardLimitBytes)
    {
        return RenderBudgetPressure::HardLimitExceeded;
    }

    if (softLimitBytes > 0 && bytes > softLimitBytes)
    {
        return RenderBudgetPressure::SoftLimitExceeded;
    }

    return RenderBudgetPressure::WithinBudget;
}

RenderStreamingPriorityResult EvaluateRenderStreamingPriority(
    const RenderStreamingPriorityInput& input) noexcept
{
    RenderStreamingPriorityResult result;
    result.assetId = input.assetId;
    result.kind = input.kind;

    std::int32_t score = 0;
    if (input.criticalVisible)
    {
        score += 1'000'000;
    }

    if (input.cameraDistance <= 32.0f)
    {
        score += 60'000;
    }
    else if (input.cameraDistance <= 128.0f)
    {
        score += 40'000;
    }
    else if (input.cameraDistance <= 512.0f)
    {
        score += 20'000;
    }
    else
    {
        score += 5'000;
    }

    score += static_cast<std::int32_t>(Clamp01(input.materialImportance) * 10'000.0f);
    score += static_cast<std::int32_t>(input.urgency) * 1'000;
    score -= static_cast<std::int32_t>(
        std::min<std::uint64_t>(input.estimatedBytes / (1024u * 1024u), 4096u));

    result.score = score;
    if (score >= 1'000'000)
    {
        result.priority = Resources::StreamingPriority::Critical;
    }
    else if (score >= 50'000)
    {
        result.priority = Resources::StreamingPriority::High;
    }
    else if (score >= 20'000)
    {
        result.priority = Resources::StreamingPriority::Normal;
    }
    else
    {
        result.priority = Resources::StreamingPriority::Low;
    }

    return result;
}

std::vector<RenderStreamingPriorityResult>
SortRenderStreamingPriorities(std::vector<RenderStreamingPriorityInput> inputs)
{
    std::vector<RenderStreamingPriorityResult> results;
    results.reserve(inputs.size());
    for (const auto& input : inputs)
    {
        results.push_back(EvaluateRenderStreamingPriority(input));
    }

    std::stable_sort(
        results.begin(),
        results.end(),
        [](const RenderStreamingPriorityResult& lhs,
           const RenderStreamingPriorityResult& rhs)
        {
            if (lhs.score != rhs.score) return lhs.score > rhs.score;
            if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
            return lhs.assetId < rhs.assetId;
        });

    return results;
}

RenderTextureStreamingDecision EvaluateRenderTextureResidency(
    const RenderTextureResidencyInput& input,
    const RenderTexturePoolBudget& budget)
{
    RenderTextureStreamingDecision decision;
    decision.assetId = input.assetId;
    decision.loadedMipRange = input.residentMipRange;
    decision.residentBytes = input.estimatedResidentBytes;
    decision.requestedBytes = input.estimatedRequestBytes;

    const auto pressure = EvaluateRenderBudgetPressure(
        budget.currentResidentBytes + budget.requestedBytes,
        budget.softLimitBytes,
        budget.hardLimitBytes);

    decision.targetMip = DesiredTextureMip(input, pressure);
    decision.fallbackMip = decision.targetMip;
    decision.requestedMipRange = {
        decision.targetMip,
        ClampMip(static_cast<std::uint8_t>(input.authoredMipCount - 1),
                 input.authoredMipCount),
        input.authoredMipCount > 0,
    };

    decision.priority = EvaluateRenderStreamingPriority({
        .assetId = input.assetId,
        .kind = RenderStreamingAssetKind::Texture,
        .criticalVisible = input.criticalVisible ||
            input.residencyHint == MaterialTextureResidencyHint::AlwaysResident,
        .cameraDistance = input.cameraDistance,
        .materialImportance = input.materialImportance,
        .urgency = static_cast<std::uint8_t>(
            input.authoredMipCount > decision.targetMip
                ? input.authoredMipCount - decision.targetMip
                : 0),
        .estimatedBytes = input.estimatedRequestBytes,
    });

    if (input.assetId == 0 || input.authoredMipCount == 0)
    {
        decision.state = RenderTextureResidencyState::Missing;
        decision.usesPlaceholder = true;
        AppendDiagnostic(
            decision.diagnostics,
            "RenderTextureResidency.MissingTextureFallback",
            input.assetId,
            RenderStreamingAssetKind::Texture,
            0,
            "Texture reference is invalid; placeholder texture is required.");
        return decision;
    }

    if (!input.availableMipRange.valid)
    {
        decision.state = RenderTextureResidencyState::Fallback;
        decision.usesPlaceholder = true;
        AppendDiagnostic(
            decision.diagnostics,
            "RenderTextureResidency.MissingMipFallback",
            input.assetId,
            RenderStreamingAssetKind::Texture,
            decision.targetMip,
            "Requested mip range has no available CPU-side source mips.");
        return decision;
    }

    if (input.residentMipRange.Contains(decision.targetMip))
    {
        decision.state = RenderTextureResidencyState::Resident;
        return decision;
    }

    const auto residentFallback =
        FirstCoarserAvailableMip(input.residentMipRange, decision.targetMip);
    if (residentFallback != std::numeric_limits<std::uint8_t>::max())
    {
        decision.state = RenderTextureResidencyState::Fallback;
        decision.fallbackMip = residentFallback;
        AppendDiagnostic(
            decision.diagnostics,
            "RenderTextureResidency.MissingMipFallback",
            input.assetId,
            RenderStreamingAssetKind::Texture,
            decision.targetMip,
            "Requested mip is not resident; using a coarser resident mip.");
        return decision;
    }

    if (input.inFlightMipRange.Contains(decision.targetMip))
    {
        decision.state = RenderTextureResidencyState::Requested;
        return decision;
    }

    const auto sourceFallback =
        FirstCoarserAvailableMip(input.availableMipRange, decision.targetMip);
    if (sourceFallback == std::numeric_limits<std::uint8_t>::max())
    {
        decision.state = RenderTextureResidencyState::Fallback;
        decision.usesPlaceholder = true;
        AppendDiagnostic(
            decision.diagnostics,
            "RenderTextureResidency.MissingMipFallback",
            input.assetId,
            RenderStreamingAssetKind::Texture,
            decision.targetMip,
            "Requested mip is unavailable; placeholder texture is required.");
        return decision;
    }

    if (sourceFallback != decision.targetMip)
    {
        decision.state = RenderTextureResidencyState::Fallback;
        decision.fallbackMip = sourceFallback;
        decision.requestedMipRange.firstMip = sourceFallback;
        AppendDiagnostic(
            decision.diagnostics,
            "RenderTextureResidency.MissingMipFallback",
            input.assetId,
            RenderStreamingAssetKind::Texture,
            decision.targetMip,
            "Requested mip is unavailable; requesting a coarser source mip.");
        return decision;
    }

    decision.state = RenderTextureResidencyState::Requested;
    return decision;
}

RenderGeometryStreamingDecision EvaluateRenderGeometryResidency(
    const RenderGeometryStreamingInput& input,
    const RenderGeometryPoolBudget& budget)
{
    RenderGeometryStreamingDecision decision;
    decision.assetId = input.assetId;
    decision.category = input.category;

    const auto pressure = EvaluateRenderBudgetPressure(
        budget.currentResidentBytes + budget.requestedBytes,
        budget.softLimitBytes,
        budget.hardLimitBytes);

    decision.targetLod = DesiredGeometryLod(input, pressure);
    decision.fallbackLod = decision.targetLod;

    const auto lodCount = std::min<std::uint8_t>(
        std::max<std::uint8_t>(input.lodCount, 1),
        kRenderMaxGeometryLods);

    for (std::uint8_t lod = 0; lod < lodCount; ++lod)
    {
        RenderMeshLodResidency lodState;
        lodState.lod = lod;
        if (HasLod(input.residentLodMask, lod))
        {
            lodState.state = RenderMeshResidencyState::Resident;
            lodState.residentBytes = input.lodBytes[lod];
            decision.residentBytes += input.lodBytes[lod];
        }
        else if (HasLod(input.inFlightLodMask, lod))
        {
            lodState.state = RenderMeshResidencyState::Requested;
            lodState.requestedBytes = input.lodBytes[lod];
            decision.requestedBytes += input.lodBytes[lod];
        }
        decision.lodResidency.push_back(lodState);
    }

    decision.priority = EvaluateRenderStreamingPriority({
        .assetId = input.assetId,
        .kind = RenderStreamingAssetKind::Geometry,
        .criticalVisible = input.criticalVisible,
        .cameraDistance = input.cameraDistance,
        .materialImportance = input.materialImportance,
        .urgency = static_cast<std::uint8_t>(lodCount - decision.targetLod),
        .estimatedBytes = input.lodBytes[decision.targetLod],
    });

    if (input.assetId == 0 || !input.meshAvailable)
    {
        decision.state = RenderMeshResidencyState::Fallback;
        decision.usesProxy = true;
        AppendDiagnostic(
            decision.diagnostics,
            "RenderGeometryResidency.MissingMeshFallback",
            input.assetId,
            RenderStreamingAssetKind::Geometry,
            decision.targetLod,
            "Mesh reference is unavailable; proxy geometry is required.");
        return decision;
    }

    if (HasLod(input.residentLodMask, decision.targetLod))
    {
        decision.state = RenderMeshResidencyState::Resident;
        return decision;
    }

    const auto residentFallback =
        FirstCoarserAvailableLod(input.residentLodMask, decision.targetLod, lodCount);
    if (residentFallback != std::numeric_limits<std::uint8_t>::max())
    {
        decision.state = RenderMeshResidencyState::Fallback;
        decision.fallbackLod = residentFallback;
        AppendDiagnostic(
            decision.diagnostics,
            "RenderGeometryResidency.LodFallback",
            input.assetId,
            RenderStreamingAssetKind::Geometry,
            decision.targetLod,
            "Requested LOD is not resident; using a coarser resident LOD.");
        return decision;
    }

    if (HasLod(input.inFlightLodMask, decision.targetLod))
    {
        decision.state = RenderMeshResidencyState::Requested;
        return decision;
    }

    const auto sourceFallback =
        FirstCoarserAvailableLod(input.availableLodMask, decision.targetLod, lodCount);
    if (sourceFallback == std::numeric_limits<std::uint8_t>::max())
    {
        decision.state = RenderMeshResidencyState::Fallback;
        decision.usesProxy = true;
        AppendDiagnostic(
            decision.diagnostics,
            "RenderGeometryResidency.MissingMeshFallback",
            input.assetId,
            RenderStreamingAssetKind::Geometry,
            decision.targetLod,
            "Requested LOD is unavailable; proxy geometry is required.");
        return decision;
    }

    if (sourceFallback != decision.targetLod)
    {
        decision.state = RenderMeshResidencyState::Fallback;
        decision.fallbackLod = sourceFallback;
        AppendDiagnostic(
            decision.diagnostics,
            "RenderGeometryResidency.LodFallback",
            input.assetId,
            RenderStreamingAssetKind::Geometry,
            decision.targetLod,
            "Requested LOD is unavailable; requesting a coarser source LOD.");
        return decision;
    }

    decision.state = RenderMeshResidencyState::Requested;
    return decision;
}

RenderTextureMemoryReport BuildRenderTextureMemoryReport(
    std::vector<RenderTextureStreamingDecision> decisions,
    RenderTexturePoolBudget budget)
{
    RenderTextureMemoryReport report;
    report.budget = budget;
    report.decisions = std::move(decisions);

    std::stable_sort(
        report.decisions.begin(),
        report.decisions.end(),
        [](const RenderTextureStreamingDecision& lhs,
           const RenderTextureStreamingDecision& rhs)
        {
            return lhs.assetId < rhs.assetId;
        });

    for (const auto& decision : report.decisions)
    {
        report.residentBytes += decision.residentBytes;
        report.requestedBytes += decision.requestedBytes;
        report.diagnostics.insert(
            report.diagnostics.end(),
            decision.diagnostics.begin(),
            decision.diagnostics.end());
    }

    report.pressure = EvaluateRenderBudgetPressure(
        report.residentBytes + report.requestedBytes,
        budget.softLimitBytes,
        budget.hardLimitBytes);

    const auto total = report.residentBytes + report.requestedBytes;
    if (report.pressure == RenderBudgetPressure::HardLimitExceeded)
    {
        report.overflowBytes = total - budget.hardLimitBytes;
        AppendDiagnostic(
            report.diagnostics,
            "RenderTextureResidency.PoolBudgetExceeded",
            0,
            RenderStreamingAssetKind::Texture,
            0,
            "Texture residency exceeds the hard pool budget.");
    }
    else if (report.pressure == RenderBudgetPressure::SoftLimitExceeded)
    {
        report.overflowBytes = total - budget.softLimitBytes;
        AppendDiagnostic(
            report.diagnostics,
            "RenderTextureResidency.PoolSoftBudgetExceeded",
            0,
            RenderStreamingAssetKind::Texture,
            0,
            "Texture residency exceeds the soft pool budget.");
    }

    SortDiagnostics(report.diagnostics);
    return report;
}

RenderGeometryMemoryReport BuildRenderGeometryMemoryReport(
    std::vector<RenderGeometryStreamingDecision> decisions,
    RenderGeometryPoolBudget budget)
{
    RenderGeometryMemoryReport report;
    report.budget = budget;
    report.decisions = std::move(decisions);

    std::stable_sort(
        report.decisions.begin(),
        report.decisions.end(),
        [](const RenderGeometryStreamingDecision& lhs,
           const RenderGeometryStreamingDecision& rhs)
        {
            if (lhs.assetId != rhs.assetId) return lhs.assetId < rhs.assetId;
            return lhs.category < rhs.category;
        });

    for (const auto& decision : report.decisions)
    {
        report.residentBytes += decision.residentBytes;
        report.requestedBytes += decision.requestedBytes;
        report.diagnostics.insert(
            report.diagnostics.end(),
            decision.diagnostics.begin(),
            decision.diagnostics.end());

        for (const auto& lod : decision.lodResidency)
        {
            if (lod.state != RenderMeshResidencyState::Resident ||
                lod.residentBytes == 0)
            {
                continue;
            }

            const auto criticalPenalty =
                decision.priority.priority == Resources::StreamingPriority::Critical
                    ? -1'000'000
                    : 0;
            report.evictionCandidates.push_back({
                .assetId = decision.assetId,
                .lod = lod.lod,
                .bytes = lod.residentBytes,
                .evictionScore = criticalPenalty +
                    static_cast<std::int32_t>(lod.lod) * 10'000 -
                    decision.priority.score,
            });
        }
    }

    std::stable_sort(
        report.evictionCandidates.begin(),
        report.evictionCandidates.end(),
        [](const RenderGeometryEvictionCandidate& lhs,
           const RenderGeometryEvictionCandidate& rhs)
        {
            if (lhs.evictionScore != rhs.evictionScore)
            {
                return lhs.evictionScore > rhs.evictionScore;
            }
            if (lhs.assetId != rhs.assetId) return lhs.assetId < rhs.assetId;
            return lhs.lod < rhs.lod;
        });

    report.pressure = EvaluateRenderBudgetPressure(
        report.residentBytes + report.requestedBytes,
        budget.softLimitBytes,
        budget.hardLimitBytes);

    const auto total = report.residentBytes + report.requestedBytes;
    if (report.pressure == RenderBudgetPressure::HardLimitExceeded)
    {
        report.overflowBytes = total - budget.hardLimitBytes;
        AppendDiagnostic(
            report.diagnostics,
            "RenderGeometryResidency.PoolBudgetExceeded",
            0,
            RenderStreamingAssetKind::Geometry,
            0,
            "Geometry residency exceeds the hard pool budget.");
    }
    else if (report.pressure == RenderBudgetPressure::SoftLimitExceeded)
    {
        report.overflowBytes = total - budget.softLimitBytes;
        AppendDiagnostic(
            report.diagnostics,
            "RenderGeometryResidency.PoolSoftBudgetExceeded",
            0,
            RenderStreamingAssetKind::Geometry,
            0,
            "Geometry residency exceeds the soft pool budget.");
    }

    SortDiagnostics(report.diagnostics);
    return report;
}

RenderResidencyReport BuildRenderResidencyReport(
    std::vector<RenderTextureStreamingDecision> textureDecisions,
    RenderTexturePoolBudget textureBudget,
    std::vector<RenderGeometryStreamingDecision> geometryDecisions,
    RenderGeometryPoolBudget geometryBudget)
{
    RenderResidencyReport report;
    report.texture = BuildRenderTextureMemoryReport(
        std::move(textureDecisions),
        textureBudget);
    report.geometry = BuildRenderGeometryMemoryReport(
        std::move(geometryDecisions),
        geometryBudget);

    for (const auto& decision : report.texture.decisions)
    {
        report.priorities.push_back(decision.priority);
    }

    for (const auto& decision : report.geometry.decisions)
    {
        report.priorities.push_back(decision.priority);
    }

    report.diagnostics.insert(
        report.diagnostics.end(),
        report.texture.diagnostics.begin(),
        report.texture.diagnostics.end());
    report.diagnostics.insert(
        report.diagnostics.end(),
        report.geometry.diagnostics.begin(),
        report.geometry.diagnostics.end());

    std::stable_sort(
        report.priorities.begin(),
        report.priorities.end(),
        [](const RenderStreamingPriorityResult& lhs,
           const RenderStreamingPriorityResult& rhs)
        {
            if (lhs.score != rhs.score) return lhs.score > rhs.score;
            if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
            return lhs.assetId < rhs.assetId;
        });

    SortDiagnostics(report.diagnostics);
    return report;
}

std::string RenderResidencyReport::Serialize() const
{
    std::ostringstream out;
    out << "render_residency_report_v1\n";
    out << "texture pressure=" << ToString(texture.pressure)
        << " residentBytes=" << texture.residentBytes
        << " requestedBytes=" << texture.requestedBytes
        << " overflowBytes=" << texture.overflowBytes << '\n';
    out << "geometry pressure=" << ToString(geometry.pressure)
        << " residentBytes=" << geometry.residentBytes
        << " requestedBytes=" << geometry.requestedBytes
        << " overflowBytes=" << geometry.overflowBytes << '\n';

    auto textureDecisions = texture.decisions;
    std::stable_sort(
        textureDecisions.begin(),
        textureDecisions.end(),
        [](const RenderTextureStreamingDecision& lhs,
           const RenderTextureStreamingDecision& rhs)
        {
            return lhs.assetId < rhs.assetId;
        });
    for (const auto& decision : textureDecisions)
    {
        AppendTexture(out, decision);
    }

    auto geometryDecisions = geometry.decisions;
    std::stable_sort(
        geometryDecisions.begin(),
        geometryDecisions.end(),
        [](const RenderGeometryStreamingDecision& lhs,
           const RenderGeometryStreamingDecision& rhs)
        {
            if (lhs.assetId != rhs.assetId) return lhs.assetId < rhs.assetId;
            return lhs.category < rhs.category;
        });
    for (const auto& decision : geometryDecisions)
    {
        AppendGeometry(out, decision);
    }

    auto sortedPriorities = priorities;
    std::stable_sort(
        sortedPriorities.begin(),
        sortedPriorities.end(),
        [](const RenderStreamingPriorityResult& lhs,
           const RenderStreamingPriorityResult& rhs)
        {
            if (lhs.score != rhs.score) return lhs.score > rhs.score;
            if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
            return lhs.assetId < rhs.assetId;
        });
    for (const auto& priority : sortedPriorities)
    {
        out << "priority asset=" << priority.assetId
            << " kind=" << ToString(priority.kind)
            << " score=" << priority.score
            << " bucket=" << ToString(priority.priority) << '\n';
    }

    auto sortedDiagnostics = diagnostics;
    SortDiagnostics(sortedDiagnostics);
    for (const auto& diagnostic : sortedDiagnostics)
    {
        out << "diagnostic id=" << diagnostic.id
            << " asset=" << diagnostic.assetId
            << " kind=" << ToString(diagnostic.kind)
            << " detail=" << static_cast<unsigned>(diagnostic.detailLevel)
            << " message=" << std::quoted(diagnostic.message) << '\n';
    }

    return out.str();
}

} // namespace SagaEngine::Render::Streaming
