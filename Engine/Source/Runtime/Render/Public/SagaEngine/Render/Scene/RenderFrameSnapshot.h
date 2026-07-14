/// @file RenderFrameSnapshot.h
/// @brief CPU-side scene extraction snapshot and view-family contract.
///
/// Layer  : SagaEngine / Render / Scene
/// Purpose: Bridges RenderWorld data to deterministic per-frame draw packets
///          without owning backend resources. This is a render-thread-ready
///          data contract, not a render thread or GPU submission API.

#pragma once

#include "SagaEngine/Math/Mat4.h"
#include "SagaEngine/Render/Culling/CullingPipeline.h"
#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/FrameContext.h"
#include "SagaEngine/Render/Streaming/RenderStreamingResidency.h"
#include "SagaEngine/Render/World/RenderEntity.h"
#include "SagaEngine/Render/World/RenderWorld.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SagaEngine::Render::Scene {

namespace Culling = ::SagaEngine::Render::Culling;
namespace LOD = ::SagaEngine::Render::LOD;
namespace Math = ::SagaEngine::Math;
namespace Streaming = ::SagaEngine::Render::Streaming;
namespace World = ::SagaEngine::Render::World;

enum class RenderViewKind : std::uint8_t
{
    Shadow = 0,
    Main,
    Probe,
    Minimap,
    Debug,
};

enum class RenderDrawPassBucket : std::uint8_t
{
    Opaque = 0,
    AlphaTest,
    Transparent,
    Overlay,
};

struct RenderViewDescriptor
{
    RenderViewKind kind = RenderViewKind::Main;
    Camera camera{};
    std::uint32_t viewIndex = 0;
    std::uint32_t stableOrder = 0;
    bool enabled = true;
    bool placeholderOnly = false;
    std::string debugName;
};

struct RenderViewFamily
{
    std::vector<RenderViewDescriptor> views;

    void AddView(RenderViewDescriptor view);
    [[nodiscard]] std::size_t EnabledViewCount() const noexcept;
};

struct RenderDrawPacketSortKey
{
    std::uint32_t viewOrder = 0;
    RenderDrawPassBucket passBucket = RenderDrawPassBucket::Opaque;
    std::uint64_t materialKey = 0;
    std::uint64_t meshKey = 0;
    std::uint32_t entityKey = 0;
    std::uint8_t lod = 0;
};

struct RenderDrawPacket
{
    std::uint32_t viewIndex = 0;
    RenderViewKind viewKind = RenderViewKind::Main;
    RenderDrawPacketSortKey sortKey{};
    World::RenderEntityId entity = World::RenderEntityId::kInvalid;
    World::MeshId mesh = World::MeshId::kInvalid;
    World::MaterialId material = World::MaterialId::kInvalid;
    Math::Mat4 model = Math::Mat4::Identity();
    std::uint8_t lod = 0;
    float distanceSq = 0.0f;
    bool usesResidencyFallback = false;
    std::vector<Streaming::RenderStreamingDiagnostic> residencyDiagnostics;
};

struct RenderFrameExtractionStats
{
    std::uint32_t viewCount = 0;
    std::uint32_t submittedViewCount = 0;
    std::uint32_t placeholderViewCount = 0;
    std::uint32_t packetCount = 0;
    std::uint32_t visited = 0;
    std::uint32_t culledHidden = 0;
    std::uint32_t culledVisibility = 0;
    std::uint32_t culledDistance = 0;
    std::uint32_t culledFrustum = 0;
    std::uint32_t invalidMesh = 0;
    std::uint32_t invalidMaterial = 0;
    std::uint32_t residencyFallbacks = 0;
};

struct RenderSceneExtractionDiagnostic
{
    std::string id;
    std::uint32_t viewIndex = 0;
    World::RenderEntityId entity = World::RenderEntityId::kInvalid;
    std::string message;
};

struct RenderFrameSnapshot
{
    FrameContext frame{};
    std::vector<RenderViewDescriptor> views;
    std::vector<RenderDrawPacket> drawPackets;
    RenderFrameExtractionStats stats{};
    std::vector<RenderSceneExtractionDiagnostic> diagnostics;

    [[nodiscard]] std::string Serialize() const;
};

using RenderResidencyDiagnosticLookupFn = std::function<
    std::vector<Streaming::RenderStreamingDiagnostic>(World::RenderEntityId)>;

struct RenderSceneExtractionConfig
{
    LOD::QualityPreset quality = LOD::QualityPreset::High;
    Culling::LodThresholdLookupFn lodLookup;
    RenderResidencyDiagnosticLookupFn residencyDiagnostics;
};

[[nodiscard]] RenderFrameSnapshot ExtractRenderFrameSnapshot(
    const World::RenderWorld& world,
    RenderViewFamily viewFamily,
    const FrameContext& frame,
    const RenderSceneExtractionConfig& config = {});

[[nodiscard]] std::vector<RenderDrawPacket> SortRenderDrawPackets(
    std::vector<RenderDrawPacket> packets);

} // namespace SagaEngine::Render::Scene
