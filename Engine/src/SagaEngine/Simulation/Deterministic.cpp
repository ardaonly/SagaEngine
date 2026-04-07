/// @file Deterministic.cpp
/// @brief Deterministic method implementations.

#include "SagaEngine/Simulation/Deterministic.h"
#include "SagaEngine/Core/Log/Log.h"

namespace SagaEngine::Simulation {

// ─── Recording ─────────────────────────────────────────────────────────────────

uint64_t Deterministic::Record(uint64_t tickNumber, const WorldState& state) noexcept
{
    const uint64_t hash = state.Hash();

    Entry& slot = m_ring[SlotFor(tickNumber)];
    slot.tick  = tickNumber;
    slot.hash  = hash;
    slot.valid = true;

    if (tickNumber > m_latestTick)
        m_latestTick = tickNumber;

    return hash;
}

void Deterministic::RecordExternal(uint64_t tickNumber, uint64_t hash) noexcept
{
    Entry& slot = m_ring[SlotFor(tickNumber)];
    slot.tick  = tickNumber;
    slot.hash  = hash;
    slot.valid = true;

    if (tickNumber > m_latestTick)
        m_latestTick = tickNumber;
}

// ─── Lookup ────────────────────────────────────────────────────────────────────

std::optional<uint64_t> Deterministic::HashAt(uint64_t tickNumber) const noexcept
{
    const Entry& slot = m_ring[SlotFor(tickNumber)];
    if (!slot.valid || slot.tick != tickNumber)
        return std::nullopt;

    return slot.hash;
}

// ─── Verification ──────────────────────────────────────────────────────────────

bool Deterministic::Verify(uint64_t tickNumber, uint64_t remoteHash) const noexcept
{
    const std::optional<uint64_t> local = HashAt(tickNumber);
    if (!local.has_value())
    {
        // Tick outside the history window — cannot verify, assume OK.
        return true;
    }

    if (*local != remoteHash)
    {
        LOG_WARN("Deterministic",
            "Divergence detected at tick %llu: local=0x%016llX remote=0x%016llX.",
            static_cast<unsigned long long>(tickNumber),
            static_cast<unsigned long long>(*local),
            static_cast<unsigned long long>(remoteHash));
        return false;
    }

    return true;
}

// ─── Reset ─────────────────────────────────────────────────────────────────────

void Deterministic::Reset() noexcept
{
    for (auto& entry : m_ring)
        entry = {};
    m_latestTick = 0;
}

} // namespace SagaEngine::Simulation
