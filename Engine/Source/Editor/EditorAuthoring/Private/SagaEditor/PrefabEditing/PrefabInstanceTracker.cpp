/// @file PrefabInstanceTracker.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/PrefabEditing/PrefabInstanceTracker.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct PrefabInstanceTracker::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

void PrefabInstanceTracker::TrackInstance(uint64_t /*entityId*/, const std::string& /*prefabPath*/)
{
    /* stub */
}

void PrefabInstanceTracker::UntrackInstance(uint64_t /*entityId*/)
{
    /* stub */
}

bool PrefabInstanceTracker::IsInstance(uint64_t /*entityId*/) const noexcept
{
    return false;
}

std::string PrefabInstanceTracker::GetPrefabPath(uint64_t /*entityId*/) const
{
    return {};
}

std::vector<uint64_t> PrefabInstanceTracker::GetInstances(const std::string& /*prefabPath*/) const
{
    return {};
}

} // namespace SagaEditor
