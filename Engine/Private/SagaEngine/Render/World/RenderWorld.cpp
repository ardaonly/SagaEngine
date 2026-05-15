/// @file RenderWorld.cpp
/// @brief Pool + free-list implementation for client-side drawable entities.

#include "SagaEngine/Render/World/RenderWorld.h"

namespace SagaEngine::Render::World
{

namespace
{
constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFu;

[[nodiscard]] std::uint32_t ToIndex(RenderEntityId id) noexcept
{
    return static_cast<std::uint32_t>(id);
}
} // namespace

// ─── Lifecycle ────────────────────────────────────────────────────────

RenderEntityId RenderWorld::Create(const RenderEntity& initial)
{
    std::uint32_t index = kInvalidIndex;
    if (!m_FreeList.empty())
    {
        index = m_FreeList.back();
        m_FreeList.pop_back();
        m_Entities[index] = initial;
        m_Alive[index]    = 1u;
    }
    else
    {
        index = static_cast<std::uint32_t>(m_Entities.size());
        m_Entities.push_back(initial);
        m_Alive.push_back(1u);
    }
    ++m_LiveCount;
    return static_cast<RenderEntityId>(index);
}

void RenderWorld::Destroy(RenderEntityId id)
{
    const std::uint32_t idx = ToIndex(id);
    if (idx >= m_Alive.size() || !m_Alive[idx])
        return;

    m_Alive[idx] = 0u;
    // Clear payload so stale reads surface as "obviously default", not as
    // "last known good state".
    m_Entities[idx] = RenderEntity{};
    m_FreeList.push_back(idx);
    --m_LiveCount;
}

bool RenderWorld::IsAlive(RenderEntityId id) const noexcept
{
    const std::uint32_t idx = ToIndex(id);
    return idx < m_Alive.size() && m_Alive[idx] != 0u;
}

const RenderEntity* RenderWorld::Get(RenderEntityId id) const noexcept
{
    return IsAlive(id) ? &m_Entities[ToIndex(id)] : nullptr;
}

RenderEntity* RenderWorld::Get(RenderEntityId id) noexcept
{
    return IsAlive(id) ? &m_Entities[ToIndex(id)] : nullptr;
}

// ─── Setters ──────────────────────────────────────────────────────────

bool RenderWorld::SetTransform(RenderEntityId id,
                                 const ::SagaEngine::Math::Transform& transform) noexcept
{
    RenderEntity* e = Get(id);
    if (!e) return false;
    e->transform = transform;
    return true;
}

bool RenderWorld::SetMesh(RenderEntityId id, MeshId mesh) noexcept
{
    RenderEntity* e = Get(id);
    if (!e) return false;
    e->mesh = mesh;
    return true;
}

bool RenderWorld::SetMaterial(RenderEntityId id, MaterialId material) noexcept
{
    RenderEntity* e = Get(id);
    if (!e) return false;
    e->material = material;
    return true;
}

bool RenderWorld::SetVisible(RenderEntityId id, bool visible) noexcept
{
    RenderEntity* e = Get(id);
    if (!e) return false;
    e->visible = visible;
    return true;
}

bool RenderWorld::SetVisibilityMask(RenderEntityId id, VisibilityMask mask) noexcept
{
    RenderEntity* e = Get(id);
    if (!e) return false;
    e->visibilityMask = mask;
    return true;
}

bool RenderWorld::SetLodOverride(RenderEntityId id, std::uint8_t lod) noexcept
{
    RenderEntity* e = Get(id);
    if (!e) return false;
    e->lodOverride = lod;
    return true;
}

bool RenderWorld::SetBoundsRadius(RenderEntityId id, float radius) noexcept
{
    RenderEntity* e = Get(id);
    if (!e) return false;
    e->boundsRadius = radius;
    return true;
}

} // namespace SagaEngine::Render::World
