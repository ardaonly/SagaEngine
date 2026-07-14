/// @file NetworkTypes.cpp
/// @brief Formatting helpers for neutral networking contracts.

#include "SagaEngine/Networking/NetworkTypes.h"

#include <cstdio>

namespace SagaEngine::Networking {

std::string NetworkStatistics::ToString() const
{
    char buffer[512];
    std::snprintf(buffer, sizeof(buffer),
                  "NetworkStats: Sent=%llu/%llu bytes, Recv=%llu/%llu bytes, "
                  "Loss=%.2f%%, Latency=%.2fms, Jitter=%.2fms",
                  static_cast<unsigned long long>(packetsSent),
                  static_cast<unsigned long long>(bytesSent),
                  static_cast<unsigned long long>(packetsReceived),
                  static_cast<unsigned long long>(bytesReceived),
                  packetLossRate * 100.0f,
                  averageLatencyMs,
                  jitterMs);
    return std::string(buffer);
}

} // namespace SagaEngine::Networking
