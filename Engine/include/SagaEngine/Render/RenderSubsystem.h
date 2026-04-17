/// @file RenderSubsystem.h
/// @brief Client-side render frame driver: camera list → cull → submit.
///
/// Layer  : SagaEngine / Render  (CLIENT TARGET ONLY)
/// Purpose: One class owns the per-frame sequence:
///            1. Build a FrameContext.
///            2. For each enabled camera (in deterministic role order):
///                 a. RecomputeViewProj + RecomputeFrustum.
///                 b. Run CullingPipeline → per-camera RenderView.
///                 c. Backend.Submit(camera, view).
///            3. Aggregate per-camera stats into FrameStats.
///          Nothing else. No shader binding, no mesh loading, no gameplay
///          hooks. The subsystem is a composition site for the pieces
///          already built under Render/World, Render/Culling, Render/LOD,
///          and Render/Backend.
///
/// Deliberate non-responsibilities:
///   - Does NOT own RenderWorld or IRenderBackend — both are injected.
///   - Does NOT know about meshes, materials, shaders, GPU resources.
///   - Does NOT run on a dedicated-server build. Server targets must not
///     compile this file.
///
/// Determinism:
///   Cameras are grouped by CameraRole and submitted strictly in that
///   order: Shadow → Main → Minimap → Debug → Custom. Within a role,
///   insertion order is preserved. This matters for debugging and for
///   any future backend-side dependency wiring (e.g., a shadow map pass
///   must run before the main pass that samples it).

#pragma once

#include "SagaEngine/Render/Backend/IRenderBackend.h"
#include "SagaEngine/Render/Culling/CullingPipeline.h"
#include "SagaEngine/Render/LOD/LODSelector.h"
#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/FrameContext.h"
#include "SagaEngine/Render/Scene/RenderView.h"
#include "SagaEngine/Render/World/RenderWorld.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Render
{

// ─── Camera role ──────────────────────────────────────────────────────

/// Coarse classification used to define deterministic submit order. The
/// subsystem sorts cameras by this enum; equal roles preserve insertion
/// order. New roles may only be inserted at the end (stable ordering).
enum class CameraRole : std::uint8_t
{
    Shadow  = 0,   ///< Shadow cascades / depth-only passes run first.
    Main    = 1,   ///< Primary world camera.
    Minimap = 2,   ///< Overhead / UI-adjacent world view.
    Debug   = 3,   ///< Editor / profiling fly-cam.
    Custom  = 4,   ///< Reserved for application extensions.
};

// ─── Per-frame aggregate stats ────────────────────────────────────────

/// Summed cull counters across every camera submitted this frame.
struct FrameStats
{
    std::uint32_t cameraCount           = 0;
    std::uint32_t totalVisited          = 0;
    std::uint32_t totalDrawn            = 0;
    std::uint32_t totalCulledFrustum    = 0;
    std::uint32_t totalCulledDistance   = 0;
    std::uint32_t totalCulledVisibility = 0;
    std::uint32_t totalCulledHidden     = 0;

    void Reset()
    {
        *this = FrameStats{};
    }
};

// ─── RenderSubsystem ──────────────────────────────────────────────────

class RenderSubsystem
{
public:
    /// Non-owning references. Caller keeps both alive for the subsystem's
    /// lifetime.
    RenderSubsystem(World::RenderWorld&       world,
                     Backend::IRenderBackend&  backend) noexcept;

    ~RenderSubsystem() = default;

    RenderSubsystem(const RenderSubsystem&)            = delete;
    RenderSubsystem& operator=(const RenderSubsystem&) = delete;

    // ── Camera management ────────────────────────────────────────

    /// Register a camera. The returned handle is stable; removing a
    /// camera does not recycle its CameraId within this subsystem's
    /// lifetime.
    Scene::CameraId AddCamera(CameraRole            role,
                                const Scene::Camera&  initial,
                                std::string           debugName = {});

    /// Remove a previously added camera. No-op if unknown.
    bool RemoveCamera(Scene::CameraId id);

    /// Mutable / const access. Mutation takes effect on next ExecuteFrame.
    [[nodiscard]] Scene::Camera*       GetCamera(Scene::CameraId id)       noexcept;
    [[nodiscard]] const Scene::Camera* GetCamera(Scene::CameraId id) const noexcept;

    /// Temporarily disable / re-enable a camera. Disabled cameras are
    /// skipped by ExecuteFrame but keep their handle and last view.
    bool SetCameraEnabled(Scene::CameraId id, bool enabled) noexcept;

    [[nodiscard]] bool        IsEnabled(Scene::CameraId id) const noexcept;
    [[nodiscard]] CameraRole  RoleOf  (Scene::CameraId id) const noexcept;
    [[nodiscard]] std::size_t CameraCount() const noexcept;
    [[nodiscard]] std::size_t EnabledCameraCount() const noexcept;

    // ── Pipeline configuration ────────────────────────────────────

    /// Install the mesh → LOD-thresholds lookup the culler forwards to
    /// each camera. The backend typically provides this from its mesh
    /// resource table.
    void SetLodLookup(Culling::LodThresholdLookupFn fn);

    /// Global quality preset (multiplier on every LOD threshold). Per-
    /// camera lodBias stacks on top of this.
    void               SetQualityPreset(LOD::QualityPreset preset) noexcept { m_Quality = preset; }
    [[nodiscard]] LOD::QualityPreset QualityPreset() const noexcept         { return m_Quality; }

    // ── Per-frame ────────────────────────────────────────────────

    /// Run one frame end-to-end. Safe to call with zero cameras (counts
    /// as a no-op frame but still advances the backend's frame index).
    void ExecuteFrame(const Scene::FrameContext& ctx);

    // ── Diagnostics ──────────────────────────────────────────────

    [[nodiscard]] const FrameStats&         LastFrameStats() const noexcept { return m_LastStats; }
    [[nodiscard]] const Scene::RenderView*  LastViewFor(Scene::CameraId id) const noexcept;

    /// Snapshot of the deterministic submit order the next ExecuteFrame
    /// will use. Useful for tests and debug HUDs.
    [[nodiscard]] std::vector<Scene::CameraId> SubmitOrder() const;

private:
    // ── Internal storage ─────────────────────────────────────────

    struct CameraSlot
    {
        Scene::CameraId       id{Scene::CameraId::kInvalid};
        CameraRole            role       = CameraRole::Main;
        Scene::Camera         camera{};
        Scene::RenderView     view{};          ///< Persistent capacity reuse.
        bool                  enabled    = true;
        bool                  valid      = false;
        std::uint32_t         insertSeq  = 0;  ///< Tie-breaker within a role.
        std::string           debugName;
    };

    [[nodiscard]] CameraSlot*       FindSlot(Scene::CameraId id)       noexcept;
    [[nodiscard]] const CameraSlot* FindSlot(Scene::CameraId id) const noexcept;

    /// Build the per-frame submit ordering (role → insertSeq → slot).
    void BuildSubmitOrder(std::vector<std::uint32_t>& outIndices) const;

    // ── Members ──────────────────────────────────────────────────

    World::RenderWorld*       m_World   = nullptr;
    Backend::IRenderBackend*  m_Backend = nullptr;
    Culling::CullingPipeline  m_Pipeline;
    LOD::QualityPreset        m_Quality = LOD::QualityPreset::High;

    std::vector<std::unique_ptr<CameraSlot>> m_Slots;  ///< Index in vector == CameraId value.
    std::uint32_t                            m_NextInsertSeq = 0;

    FrameStats                m_LastStats;
};

} // namespace SagaEngine::Render
