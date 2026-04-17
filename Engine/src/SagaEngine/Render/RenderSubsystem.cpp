/// @file RenderSubsystem.cpp
/// @brief Multi-camera frame driver. Owns only the per-camera RenderViews
///        and the aggregate FrameStats.

#include "SagaEngine/Render/RenderSubsystem.h"

#include <algorithm>

namespace SagaEngine::Render
{

namespace
{

[[nodiscard]] std::uint32_t ToIndex(Scene::CameraId id) noexcept
{
    return static_cast<std::uint32_t>(id);
}

[[nodiscard]] Scene::CameraId ToId(std::uint32_t idx) noexcept
{
    return static_cast<Scene::CameraId>(idx);
}

} // namespace

// ─── Construction ─────────────────────────────────────────────────────

RenderSubsystem::RenderSubsystem(World::RenderWorld&      world,
                                   Backend::IRenderBackend& backend) noexcept
    : m_World(&world)
    , m_Backend(&backend)
{}

// ─── Camera management ────────────────────────────────────────────────

Scene::CameraId RenderSubsystem::AddCamera(CameraRole            role,
                                              const Scene::Camera&  initial,
                                              std::string           debugName)
{
    // Find a free slot first (recycles holes left by RemoveCamera).
    std::uint32_t index = static_cast<std::uint32_t>(m_Slots.size());
    for (std::uint32_t i = 0; i < m_Slots.size(); ++i)
    {
        if (!m_Slots[i] || !m_Slots[i]->valid)
        {
            index = i;
            break;
        }
    }

    if (index == m_Slots.size())
        m_Slots.emplace_back(std::make_unique<CameraSlot>());
    else if (!m_Slots[index])
        m_Slots[index] = std::make_unique<CameraSlot>();

    CameraSlot& slot = *m_Slots[index];
    slot.id         = ToId(index);
    slot.role       = role;
    slot.camera     = initial;
    slot.view.Reset();
    slot.enabled    = true;
    slot.valid      = true;
    slot.insertSeq  = ++m_NextInsertSeq;
    slot.debugName  = std::move(debugName);
    return slot.id;
}

bool RenderSubsystem::RemoveCamera(Scene::CameraId id)
{
    CameraSlot* slot = FindSlot(id);
    if (slot == nullptr) return false;
    slot->valid   = false;
    slot->enabled = false;
    slot->view.Reset();
    slot->debugName.clear();
    return true;
}

Scene::Camera* RenderSubsystem::GetCamera(Scene::CameraId id) noexcept
{
    CameraSlot* slot = FindSlot(id);
    return slot ? &slot->camera : nullptr;
}

const Scene::Camera* RenderSubsystem::GetCamera(Scene::CameraId id) const noexcept
{
    const CameraSlot* slot = FindSlot(id);
    return slot ? &slot->camera : nullptr;
}

bool RenderSubsystem::SetCameraEnabled(Scene::CameraId id, bool enabled) noexcept
{
    CameraSlot* slot = FindSlot(id);
    if (slot == nullptr) return false;
    slot->enabled = enabled;
    return true;
}

bool RenderSubsystem::IsEnabled(Scene::CameraId id) const noexcept
{
    const CameraSlot* slot = FindSlot(id);
    return slot && slot->enabled;
}

CameraRole RenderSubsystem::RoleOf(Scene::CameraId id) const noexcept
{
    const CameraSlot* slot = FindSlot(id);
    return slot ? slot->role : CameraRole::Custom;
}

std::size_t RenderSubsystem::CameraCount() const noexcept
{
    std::size_t n = 0;
    for (const auto& s : m_Slots)
        if (s && s->valid) ++n;
    return n;
}

std::size_t RenderSubsystem::EnabledCameraCount() const noexcept
{
    std::size_t n = 0;
    for (const auto& s : m_Slots)
        if (s && s->valid && s->enabled) ++n;
    return n;
}

// ─── Pipeline config ──────────────────────────────────────────────────

void RenderSubsystem::SetLodLookup(Culling::LodThresholdLookupFn fn)
{
    m_Pipeline.SetLodLookup(std::move(fn));
}

// ─── Diagnostics ──────────────────────────────────────────────────────

const Scene::RenderView* RenderSubsystem::LastViewFor(Scene::CameraId id) const noexcept
{
    const CameraSlot* slot = FindSlot(id);
    return slot ? &slot->view : nullptr;
}

std::vector<Scene::CameraId> RenderSubsystem::SubmitOrder() const
{
    std::vector<std::uint32_t> indices;
    BuildSubmitOrder(indices);
    std::vector<Scene::CameraId> out;
    out.reserve(indices.size());
    for (auto i : indices)
        out.push_back(m_Slots[i]->id);
    return out;
}

// ─── Slot helpers ─────────────────────────────────────────────────────

RenderSubsystem::CameraSlot* RenderSubsystem::FindSlot(Scene::CameraId id) noexcept
{
    const std::uint32_t idx = ToIndex(id);
    if (idx >= m_Slots.size() || !m_Slots[idx] || !m_Slots[idx]->valid)
        return nullptr;
    return m_Slots[idx].get();
}

const RenderSubsystem::CameraSlot* RenderSubsystem::FindSlot(Scene::CameraId id) const noexcept
{
    const std::uint32_t idx = ToIndex(id);
    if (idx >= m_Slots.size() || !m_Slots[idx] || !m_Slots[idx]->valid)
        return nullptr;
    return m_Slots[idx].get();
}

void RenderSubsystem::BuildSubmitOrder(std::vector<std::uint32_t>& outIndices) const
{
    outIndices.clear();
    outIndices.reserve(m_Slots.size());
    for (std::uint32_t i = 0; i < m_Slots.size(); ++i)
    {
        if (m_Slots[i] && m_Slots[i]->valid && m_Slots[i]->enabled)
            outIndices.push_back(i);
    }

    // Primary key: role (ascending). Secondary key: insertSeq (stable).
    std::stable_sort(outIndices.begin(), outIndices.end(),
                      [this](std::uint32_t a, std::uint32_t b)
    {
        const CameraSlot& sa = *m_Slots[a];
        const CameraSlot& sb = *m_Slots[b];
        if (sa.role != sb.role)
            return static_cast<std::uint8_t>(sa.role) <
                   static_cast<std::uint8_t>(sb.role);
        return sa.insertSeq < sb.insertSeq;
    });
}

// ─── Per-frame ────────────────────────────────────────────────────────

void RenderSubsystem::ExecuteFrame(const Scene::FrameContext& ctx)
{
    (void)ctx;   // FrameContext is currently observational only; reserved
                 // for future per-camera per-stage hooks (time uniforms,
                 // motion-vector prev-frame swap, etc.).

    m_LastStats.Reset();
    if (m_World == nullptr || m_Backend == nullptr)
        return;

    std::vector<std::uint32_t> order;
    BuildSubmitOrder(order);

    m_Backend->BeginFrame();

    Culling::CullingConfig cfg;
    cfg.quality = m_Quality;

    for (std::uint32_t idx : order)
    {
        CameraSlot& slot = *m_Slots[idx];

        // (1) Refresh derived camera state. The subsystem owns this so
        //     callers never have to remember to call RecomputeFrustum()
        //     after mutating the camera — they mutate, we re-derive.
        slot.camera.RecomputeViewProj();
        slot.camera.RecomputeFrustum();

        // (2) Pre-size the draw list from last frame's count. Cheap
        //     amortisation; harmless on the first frame (reserve 0).
        cfg.reserveHint = slot.view.DrawCount();

        // (3) Cull → view.
        m_Pipeline.Run(*m_World, slot.camera, cfg, slot.view);

        // (4) Submit to backend.
        m_Backend->Submit(slot.camera, slot.view);

        // (5) Aggregate.
        ++m_LastStats.cameraCount;
        m_LastStats.totalVisited          += slot.view.visited;
        m_LastStats.totalDrawn            += static_cast<std::uint32_t>(slot.view.DrawCount());
        m_LastStats.totalCulledFrustum    += slot.view.culledFrustum;
        m_LastStats.totalCulledDistance   += slot.view.culledDistance;
        m_LastStats.totalCulledVisibility += slot.view.culledVisibility;
        m_LastStats.totalCulledHidden     += slot.view.culledHidden;
    }

    m_Backend->EndFrame();
}

} // namespace SagaEngine::Render
