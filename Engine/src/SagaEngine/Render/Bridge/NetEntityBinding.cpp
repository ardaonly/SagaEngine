/// @file NetEntityBinding.cpp
/// @brief Network-id → RenderEntityId mapping with thin forwarding to RenderWorld.

#include "SagaEngine/Render/Bridge/NetEntityBinding.h"

namespace SagaEngine::Render::Bridge
{

// ─── Create / release ─────────────────────────────────────────────────

World::RenderEntityId NetEntityBinding::BindOrCreate(std::uint32_t networkId,
                                                       const World::RenderEntity& initial)
{
    if (m_World == nullptr)
        return World::RenderEntityId::kInvalid;

    auto it = m_NetToRender.find(networkId);
    if (it != m_NetToRender.end())
    {
        // Already bound — update the existing slot in place.
        if (auto* e = m_World->Get(it->second))
        {
            *e = initial;
            e->networkId = networkId;   // Always keep this field authoritative.
        }
        return it->second;
    }

    World::RenderEntity copy = initial;
    copy.networkId = networkId;
    const auto id = m_World->Create(copy);
    m_NetToRender.emplace(networkId, id);
    return id;
}

void NetEntityBinding::Release(std::uint32_t networkId)
{
    if (m_World == nullptr) return;

    auto it = m_NetToRender.find(networkId);
    if (it == m_NetToRender.end()) return;

    m_World->Destroy(it->second);
    m_NetToRender.erase(it);
}

World::RenderEntityId NetEntityBinding::Resolve(std::uint32_t networkId) const noexcept
{
    const auto it = m_NetToRender.find(networkId);
    return (it == m_NetToRender.end())
         ? World::RenderEntityId::kInvalid
         : it->second;
}

// ─── Hot-path updates ─────────────────────────────────────────────────

bool NetEntityBinding::UpdateTransform(std::uint32_t networkId,
                                         const ::SagaEngine::Math::Transform& transform) noexcept
{
    if (m_World == nullptr) return false;
    const auto id = Resolve(networkId);
    return (id != World::RenderEntityId::kInvalid) && m_World->SetTransform(id, transform);
}

bool NetEntityBinding::UpdateVisible(std::uint32_t networkId, bool visible) noexcept
{
    if (m_World == nullptr) return false;
    const auto id = Resolve(networkId);
    return (id != World::RenderEntityId::kInvalid) && m_World->SetVisible(id, visible);
}

bool NetEntityBinding::UpdateMesh(std::uint32_t networkId, World::MeshId mesh) noexcept
{
    if (m_World == nullptr) return false;
    const auto id = Resolve(networkId);
    return (id != World::RenderEntityId::kInvalid) && m_World->SetMesh(id, mesh);
}

bool NetEntityBinding::UpdateMaterial(std::uint32_t networkId, World::MaterialId material) noexcept
{
    if (m_World == nullptr) return false;
    const auto id = Resolve(networkId);
    return (id != World::RenderEntityId::kInvalid) && m_World->SetMaterial(id, material);
}

bool NetEntityBinding::UpdateBoundsRadius(std::uint32_t networkId, float radius) noexcept
{
    if (m_World == nullptr) return false;
    const auto id = Resolve(networkId);
    return (id != World::RenderEntityId::kInvalid) && m_World->SetBoundsRadius(id, radius);
}

void NetEntityBinding::Clear()
{
    if (m_World == nullptr)
    {
        m_NetToRender.clear();
        return;
    }
    for (auto& kv : m_NetToRender)
        m_World->Destroy(kv.second);
    m_NetToRender.clear();
}

} // namespace SagaEngine::Render::Bridge
