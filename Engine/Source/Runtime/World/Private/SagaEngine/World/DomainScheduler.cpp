/// @file DomainScheduler.cpp
/// @brief Multi-rate tick scheduler implementation.

#include "SagaEngine/World/DomainScheduler.h"
#include <algorithm>

namespace SagaEngine::World {

void DomainScheduler::RegisterDomain(SimDomainType type, const char* name,
                                       uint32_t ticksPerSecond) noexcept
{
    const auto idx = static_cast<size_t>(type);
    if (idx >= m_domains.size())
        return;

    auto& ds = m_domains[idx];
    ds.descriptor.type            = type;
    ds.descriptor.name            = name;
    ds.descriptor.ticksPerSecond  = ticksPerSecond;
    ds.interval                   = ticksPerSecond > 0 ? (60u / ticksPerSecond) : 1u;
    ds.accumulator                = 0;
    ds.active                     = true;
    ds.stats                      = DomainStats{};
}

void DomainScheduler::UnregisterDomain(SimDomainType type) noexcept
{
    const auto idx = static_cast<size_t>(type);
    if (idx >= m_domains.size())
        return;

    m_domains[idx] = DomainState{}; // zero out
    m_domainCells[idx].clear();
}

void DomainScheduler::RegisterCell(SimCellId cellId, SimDomainType domain) noexcept
{
    const auto dIdx = static_cast<size_t>(domain);
    if (dIdx >= m_domainCells.size())
        return;

    // Check if already registered.
    for (const auto& cid : m_domainCells[dIdx])
    {
        if (cid.worldX == cellId.worldX && cid.worldZ == cellId.worldZ)
            return; // duplicate
    }

    m_domainCells[dIdx].push_back(cellId);
    m_domains[dIdx].descriptor.cellCount = static_cast<uint32_t>(m_domainCells[dIdx].size());
}

void DomainScheduler::UnregisterCell(SimCellId cellId, SimDomainType domain) noexcept
{
    const auto dIdx = static_cast<size_t>(domain);
    if (dIdx >= m_domainCells.size())
        return;

    auto& cells = m_domainCells[dIdx];
    cells.erase(
        std::remove_if(cells.begin(), cells.end(),
            [cellId](SimCellId c) {
                return c.worldX == cellId.worldX && c.worldZ == cellId.worldZ;
            }),
        cells.end());

    m_domains[dIdx].descriptor.cellCount = static_cast<uint32_t>(cells.size());
}

void DomainScheduler::UnregisterCellAll(SimCellId cellId) noexcept
{
    for (size_t d = 0; d < m_domainCells.size(); ++d)
    {
        auto& cells = m_domainCells[d];
        cells.erase(
            std::remove_if(cells.begin(), cells.end(),
                [cellId](SimCellId c) {
                    return c.worldX == cellId.worldX && c.worldZ == cellId.worldZ;
                }),
            cells.end());
        m_domains[d].descriptor.cellCount = static_cast<uint32_t>(cells.size());
    }
}

void DomainScheduler::Tick(uint64_t worldTick,
                              std::vector<SimDomainType>& outFiringDomains) noexcept
{
    outFiringDomains.clear();

    for (size_t d = 0; d < m_domains.size(); ++d)
    {
        auto& ds = m_domains[d];
        if (!ds.active || ds.interval == 0)
            continue;

        ds.accumulator++;

        if (ds.accumulator < ds.interval)
            continue;

        // Fire this domain.
        ds.accumulator -= ds.interval;

        const auto type = static_cast<SimDomainType>(d);
        outFiringDomains.push_back(type);

        ds.stats.totalTicksFired++;
        ds.stats.lastFireWorldTick = worldTick;
        ds.stats.subTickAccumulator = static_cast<float>(ds.accumulator) /
                                       static_cast<float>(ds.interval);
    }
}

const SimDomainDescriptor* DomainScheduler::GetDomain(SimDomainType type) const noexcept
{
    const auto idx = static_cast<size_t>(type);
    if (idx >= m_domains.size() || !m_domains[idx].active)
        return nullptr;
    return &m_domains[idx].descriptor;
}

uint32_t DomainScheduler::RegisteredCellCount(SimDomainType type) const noexcept
{
    const auto idx = static_cast<size_t>(type);
    if (idx >= m_domainCells.size())
        return 0;
    return static_cast<uint32_t>(m_domainCells[idx].size());
}

const DomainScheduler::DomainStats& DomainScheduler::GetDomainStats(SimDomainType type) const noexcept
{
    const auto idx = static_cast<size_t>(type);
    static const DomainStats kEmpty{};
    if (idx >= m_domains.size())
        return kEmpty;
    return m_domains[idx].stats;
}

const std::vector<SimCellId>& DomainScheduler::GetDomainCells(SimDomainType type) const noexcept
{
    static const std::vector<SimCellId> kEmpty{};
    const auto idx = static_cast<size_t>(type);
    if (idx >= m_domainCells.size())
        return kEmpty;
    return m_domainCells[idx];
}

} // namespace SagaEngine::World
