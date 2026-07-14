#include "SagaEngine/Render/Scene/RenderFrameSnapshot.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace SagaEngine::Render::Scene {
namespace {

[[nodiscard]] std::uint32_t ToEntityKey(World::RenderEntityId id) noexcept
{
    return static_cast<std::uint32_t>(id);
}

[[nodiscard]] std::uint64_t ToMeshKey(World::MeshId id) noexcept
{
    return static_cast<std::uint32_t>(id);
}

[[nodiscard]] std::uint64_t ToMaterialKey(World::MaterialId id) noexcept
{
    return static_cast<std::uint32_t>(id);
}

[[nodiscard]] const char* ToString(RenderViewKind kind) noexcept
{
    switch (kind)
    {
        case RenderViewKind::Shadow: return "shadow";
        case RenderViewKind::Main: return "main";
        case RenderViewKind::Probe: return "probe";
        case RenderViewKind::Minimap: return "minimap";
        case RenderViewKind::Debug: return "debug";
    }
    return "unknown";
}

[[nodiscard]] const char* ToString(RenderDrawPassBucket bucket) noexcept
{
    switch (bucket)
    {
        case RenderDrawPassBucket::Opaque: return "opaque";
        case RenderDrawPassBucket::AlphaTest: return "alpha_test";
        case RenderDrawPassBucket::Transparent: return "transparent";
        case RenderDrawPassBucket::Overlay: return "overlay";
    }
    return "unknown";
}

[[nodiscard]] float DistanceSq(const Math::Vec3& a, const Math::Vec3& b) noexcept
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

void AppendDiagnostic(std::vector<RenderSceneExtractionDiagnostic>& diagnostics,
                      std::string id,
                      std::uint32_t viewIndex,
                      World::RenderEntityId entity,
                      std::string message)
{
    RenderSceneExtractionDiagnostic diagnostic;
    diagnostic.id = std::move(id);
    diagnostic.viewIndex = viewIndex;
    diagnostic.entity = entity;
    diagnostic.message = std::move(message);
    diagnostics.push_back(std::move(diagnostic));
}

void SortDiagnostics(std::vector<RenderSceneExtractionDiagnostic>& diagnostics)
{
    std::stable_sort(
        diagnostics.begin(),
        diagnostics.end(),
        [](const RenderSceneExtractionDiagnostic& lhs,
           const RenderSceneExtractionDiagnostic& rhs)
        {
            if (lhs.id != rhs.id) return lhs.id < rhs.id;
            if (lhs.viewIndex != rhs.viewIndex) return lhs.viewIndex < rhs.viewIndex;
            return ToEntityKey(lhs.entity) < ToEntityKey(rhs.entity);
        });
}

[[nodiscard]] bool PacketLess(const RenderDrawPacket& lhs,
                              const RenderDrawPacket& rhs) noexcept
{
    if (lhs.sortKey.viewOrder != rhs.sortKey.viewOrder)
    {
        return lhs.sortKey.viewOrder < rhs.sortKey.viewOrder;
    }
    if (lhs.sortKey.passBucket != rhs.sortKey.passBucket)
    {
        return lhs.sortKey.passBucket < rhs.sortKey.passBucket;
    }
    if (lhs.sortKey.materialKey != rhs.sortKey.materialKey)
    {
        return lhs.sortKey.materialKey < rhs.sortKey.materialKey;
    }
    if (lhs.sortKey.meshKey != rhs.sortKey.meshKey)
    {
        return lhs.sortKey.meshKey < rhs.sortKey.meshKey;
    }
    if (lhs.sortKey.lod != rhs.sortKey.lod)
    {
        return lhs.sortKey.lod < rhs.sortKey.lod;
    }
    return lhs.sortKey.entityKey < rhs.sortKey.entityKey;
}

} // namespace

void RenderViewFamily::AddView(RenderViewDescriptor view)
{
    if (view.stableOrder == 0)
    {
        view.stableOrder = static_cast<std::uint32_t>(views.size() + 1u);
    }
    if (view.viewIndex == 0 && !views.empty())
    {
        view.viewIndex = static_cast<std::uint32_t>(views.size());
    }
    views.push_back(std::move(view));
}

std::size_t RenderViewFamily::EnabledViewCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& view : views)
    {
        if (view.enabled)
        {
            ++count;
        }
    }
    return count;
}

std::vector<RenderDrawPacket> SortRenderDrawPackets(
    std::vector<RenderDrawPacket> packets)
{
    std::stable_sort(packets.begin(), packets.end(), PacketLess);
    return packets;
}

RenderFrameSnapshot ExtractRenderFrameSnapshot(
    const World::RenderWorld& world,
    RenderViewFamily viewFamily,
    const FrameContext& frame,
    const RenderSceneExtractionConfig& config)
{
    RenderFrameSnapshot snapshot;
    snapshot.frame = frame;

    if (viewFamily.views.empty())
    {
        AppendDiagnostic(
            snapshot.diagnostics,
            "RenderSceneExtraction.EmptyViewFamily",
            0,
            World::RenderEntityId::kInvalid,
            "Render frame extraction received no views.");
        return snapshot;
    }

    std::stable_sort(
        viewFamily.views.begin(),
        viewFamily.views.end(),
        [](const RenderViewDescriptor& lhs, const RenderViewDescriptor& rhs)
        {
            if (lhs.enabled != rhs.enabled) return lhs.enabled > rhs.enabled;
            if (lhs.stableOrder != rhs.stableOrder)
            {
                return lhs.stableOrder < rhs.stableOrder;
            }
            if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
            return lhs.viewIndex < rhs.viewIndex;
        });

    snapshot.views = viewFamily.views;
    snapshot.stats.viewCount = static_cast<std::uint32_t>(viewFamily.views.size());

    const auto& entities = world.EntitiesView();
    const auto& alive = world.AliveSlotsView();

    for (const auto& viewDescriptor : viewFamily.views)
    {
        if (!viewDescriptor.enabled)
        {
            continue;
        }

        ++snapshot.stats.submittedViewCount;
        if (viewDescriptor.placeholderOnly)
        {
            ++snapshot.stats.placeholderViewCount;
            AppendDiagnostic(
                snapshot.diagnostics,
                "RenderSceneExtraction.PlaceholderView",
                viewDescriptor.viewIndex,
                World::RenderEntityId::kInvalid,
                "View descriptor is placeholder-only; no draw packets emitted.");
            continue;
        }

        Camera camera = viewDescriptor.camera;
        camera.RecomputeViewProj();
        camera.RecomputeFrustum();

        const bool hasDistanceLimit = camera.maxDrawDistance > 0.0f;
        const float maxDistanceSq = camera.maxDrawDistance * camera.maxDrawDistance;

        for (std::size_t i = 0; i < entities.size(); ++i)
        {
            if (i >= alive.size() || !alive[i])
            {
                continue;
            }

            const auto entityId = static_cast<World::RenderEntityId>(
                static_cast<std::uint32_t>(i));
            const World::RenderEntity& entity = entities[i];
            ++snapshot.stats.visited;

            if (!entity.visible)
            {
                ++snapshot.stats.culledHidden;
                continue;
            }

            if ((entity.visibilityMask & camera.filter) == 0u)
            {
                ++snapshot.stats.culledVisibility;
                continue;
            }

            const float distSq = DistanceSq(camera.position, entity.transform.position);
            if (hasDistanceLimit && distSq > maxDistanceSq)
            {
                ++snapshot.stats.culledDistance;
                continue;
            }

            if (!camera.frustum.IntersectsSphere(
                    entity.transform.position,
                    entity.boundsRadius))
            {
                ++snapshot.stats.culledFrustum;
                continue;
            }

            if (entity.mesh == World::MeshId::kInvalid)
            {
                ++snapshot.stats.invalidMesh;
                AppendDiagnostic(
                    snapshot.diagnostics,
                    "RenderSceneExtraction.InvalidMesh",
                    viewDescriptor.viewIndex,
                    entityId,
                    "Visible render entity has an invalid mesh handle.");
                continue;
            }

            if (entity.material == World::MaterialId::kInvalid)
            {
                ++snapshot.stats.invalidMaterial;
                AppendDiagnostic(
                    snapshot.diagnostics,
                    "RenderSceneExtraction.InvalidMaterial",
                    viewDescriptor.viewIndex,
                    entityId,
                    "Visible render entity has an invalid material handle.");
                continue;
            }

            std::uint8_t lod = 0;
            if (entity.lodOverride != World::kLodOverrideAuto)
            {
                lod = entity.lodOverride;
            }
            else if (config.lodLookup)
            {
                lod = LOD::SelectLodSq(
                    config.lodLookup(entity.mesh),
                    distSq,
                    config.quality,
                    camera.lodBias);
            }

            RenderDrawPacket packet;
            packet.viewIndex = viewDescriptor.viewIndex;
            packet.viewKind = viewDescriptor.kind;
            packet.entity = entityId;
            packet.mesh = entity.mesh;
            packet.material = entity.material;
            packet.model = Math::Mat4::FromTransform(entity.transform);
            packet.lod = lod;
            packet.distanceSq = distSq;
            packet.sortKey.viewOrder = viewDescriptor.stableOrder;
            packet.sortKey.passBucket = RenderDrawPassBucket::Opaque;
            packet.sortKey.materialKey = ToMaterialKey(entity.material);
            packet.sortKey.meshKey = ToMeshKey(entity.mesh);
            packet.sortKey.entityKey = ToEntityKey(entityId);
            packet.sortKey.lod = lod;

            if (config.residencyDiagnostics)
            {
                packet.residencyDiagnostics =
                    config.residencyDiagnostics(entityId);
                packet.usesResidencyFallback =
                    !packet.residencyDiagnostics.empty();
                if (packet.usesResidencyFallback)
                {
                    ++snapshot.stats.residencyFallbacks;
                    AppendDiagnostic(
                        snapshot.diagnostics,
                        "RenderSceneExtraction.ResidencyFallback",
                        viewDescriptor.viewIndex,
                        entityId,
                        "Draw packet carries CPU-side residency fallback diagnostics.");
                }
            }

            snapshot.drawPackets.push_back(std::move(packet));
        }
    }

    snapshot.drawPackets = SortRenderDrawPackets(std::move(snapshot.drawPackets));
    snapshot.stats.packetCount =
        static_cast<std::uint32_t>(snapshot.drawPackets.size());
    SortDiagnostics(snapshot.diagnostics);
    return snapshot;
}

std::string RenderFrameSnapshot::Serialize() const
{
    std::ostringstream out;
    out << "render_frame_snapshot_v1\n";
    out << "frame index=" << frame.frameIndex
        << " delta=" << std::fixed << std::setprecision(6) << frame.deltaTimeSec
        << " absolute=" << std::fixed << std::setprecision(6)
        << frame.absoluteTimeSec << '\n';
    out << "stats views=" << stats.viewCount
        << " submittedViews=" << stats.submittedViewCount
        << " placeholderViews=" << stats.placeholderViewCount
        << " packets=" << stats.packetCount
        << " visited=" << stats.visited
        << " hidden=" << stats.culledHidden
        << " visibility=" << stats.culledVisibility
        << " distance=" << stats.culledDistance
        << " frustum=" << stats.culledFrustum
        << " invalidMesh=" << stats.invalidMesh
        << " invalidMaterial=" << stats.invalidMaterial
        << " residencyFallbacks=" << stats.residencyFallbacks << '\n';

    auto sortedViews = views;
    std::stable_sort(
        sortedViews.begin(),
        sortedViews.end(),
        [](const RenderViewDescriptor& lhs, const RenderViewDescriptor& rhs)
        {
            if (lhs.stableOrder != rhs.stableOrder)
            {
                return lhs.stableOrder < rhs.stableOrder;
            }
            if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
            return lhs.viewIndex < rhs.viewIndex;
        });
    for (const auto& view : sortedViews)
    {
        out << "view index=" << view.viewIndex
            << " order=" << view.stableOrder
            << " kind=" << ToString(view.kind)
            << " enabled=" << (view.enabled ? "true" : "false")
            << " placeholder=" << (view.placeholderOnly ? "true" : "false")
            << " name=" << std::quoted(view.debugName) << '\n';
    }

    auto sortedPackets = SortRenderDrawPackets(drawPackets);
    for (const auto& packet : sortedPackets)
    {
        out << "packet view=" << packet.viewIndex
            << " kind=" << ToString(packet.viewKind)
            << " bucket=" << ToString(packet.sortKey.passBucket)
            << " entity=" << ToEntityKey(packet.entity)
            << " mesh=" << ToMeshKey(packet.mesh)
            << " material=" << ToMaterialKey(packet.material)
            << " lod=" << static_cast<unsigned>(packet.lod)
            << " distanceSq=" << std::fixed << std::setprecision(3)
            << packet.distanceSq
            << " residencyFallback="
            << (packet.usesResidencyFallback ? "true" : "false") << '\n';
    }

    auto sortedDiagnostics = diagnostics;
    SortDiagnostics(sortedDiagnostics);
    for (const auto& diagnostic : sortedDiagnostics)
    {
        out << "diagnostic id=" << diagnostic.id
            << " view=" << diagnostic.viewIndex
            << " entity=" << ToEntityKey(diagnostic.entity)
            << " message=" << std::quoted(diagnostic.message) << '\n';
    }

    return out.str();
}

} // namespace SagaEngine::Render::Scene
