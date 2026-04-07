/// @file NetworkStatsPanel.h
/// @brief HUD panel for runtime network diagnostics.
///
/// Layer  : Sandbox / UI / Panels
/// Purpose: Displays ReliableChannel RTT, jitter, loss rate, and replication
///          count. Populated by whoever owns the active ReliableChannel(s)
///          via SetChannelStats(). Decoupled from the channel itself.

#pragma once

#include "IDebugPanel.h"
#include <array>
#include <cstdint>

namespace SagaSandbox
{

class NetworkStatsPanel final : public IDebugPanel
{
public:
    NetworkStatsPanel()  = default;
    ~NetworkStatsPanel() override = default;

    [[nodiscard]] std::string_view GetTitle() const noexcept override
    {
        return "Network Stats";
    }

    // ── Data feed (called by NetworkReplicationScenario each tick) ────────────

    struct ChannelSnapshot
    {
        float    rttMs              = 0.f;
        float    jitterMs           = 0.f;
        float    packetLossRate     = 0.f;   ///< [0, 1]
        uint64_t packetsSent        = 0;
        uint64_t packetsReceived    = 0;
        uint64_t packetsLost        = 0;
        uint64_t packetsRetransmit  = 0;
    };

    struct ReplicationSnapshot
    {
        uint32_t connectedClients   = 0;
        uint32_t entitiesReplicated = 0;
        uint32_t dirtyComponents    = 0;
        float    replicationHz      = 0.f;
    };

    void UpdateChannelStats(const ChannelSnapshot& snap)    noexcept { m_channel = snap; }
    void UpdateReplicationStats(const ReplicationSnapshot& snap) noexcept { m_replication = snap; }

    void Render(float dt, uint64_t tick) override;

private:
    static constexpr int kHistorySize = 128;

    ChannelSnapshot     m_channel;
    ReplicationSnapshot m_replication;

    std::array<float, kHistorySize> m_rttHistory{};
    std::array<float, kHistorySize> m_lossHistory{};
    int m_historyOffset = 0;
};

} // namespace SagaSandbox