/// @file PacketBandwidthTracker.cpp
/// @brief PacketBandwidthTracker implementation — simple lock-free adders.

#include "SagaServer/Networking/Core/PacketBandwidthTracker.h"

namespace SagaEngine::Networking
{

// ─── Record ──────────────────────────────────────────────────────────────────

void PacketBandwidthTracker::RecordSent(std::uint16_t type,
                                        std::uint32_t bytes) noexcept
{
    // Out-of-range types are silently ignored — the tracker cannot grow
    // its array at runtime and dropping a stat is better than crashing a
    // production server.  The packet registry should never hand out a
    // type id ≥ kMaxPacketTypes; if it does that is a separate bug.
    if (type >= kMaxPacketTypes)
        return;

    auto& slot = stats_[type];
    slot.bytesSent.Add(bytes);
    slot.countSent.Increment();
}

void PacketBandwidthTracker::RecordReceived(std::uint16_t type,
                                            std::uint32_t bytes) noexcept
{
    if (type >= kMaxPacketTypes)
        return;

    auto& slot = stats_[type];
    slot.bytesReceived.Add(bytes);
    slot.countReceived.Increment();
}

// ─── Snapshot ────────────────────────────────────────────────────────────────

PacketBandwidthTracker::Sample
PacketBandwidthTracker::Snapshot(std::uint16_t type) const noexcept
{
    Sample out{};
    if (type >= kMaxPacketTypes)
        return out;

    const auto& slot = stats_[type];
    out.bytesSent     = slot.bytesSent.Load();
    out.bytesReceived = slot.bytesReceived.Load();
    out.countSent     = slot.countSent.Load();
    out.countReceived = slot.countReceived.Load();
    return out;
}

// ─── Totals ──────────────────────────────────────────────────────────────────

std::uint64_t PacketBandwidthTracker::TotalBytesSent() const noexcept
{
    // Linear scan — bounded by kMaxPacketTypes (256) and called from the
    // low-frequency stats reporter, so a plain sum is fine.
    std::uint64_t total = 0;
    for (const auto& s : stats_)
        total += s.bytesSent.Load();
    return total;
}

std::uint64_t PacketBandwidthTracker::TotalBytesReceived() const noexcept
{
    std::uint64_t total = 0;
    for (const auto& s : stats_)
        total += s.bytesReceived.Load();
    return total;
}

// ─── Reset ───────────────────────────────────────────────────────────────────

void PacketBandwidthTracker::Reset() noexcept
{
    for (auto& s : stats_)
    {
        s.bytesSent.Reset();
        s.bytesReceived.Reset();
        s.countSent.Reset();
        s.countReceived.Reset();
    }
}

} // namespace SagaEngine::Networking
