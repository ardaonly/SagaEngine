/// @file ResourceSnapshot.hpp
/// @brief Captures deterministic diagnostics resource snapshots.

#pragma once

#include <SagaEngine/Diagnostics/Resource/ResourceRecord.hpp>

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace SagaEngine::Diagnostics
{

/// Deterministic summary of active resource leak candidates.
struct ResourceLeakSummary
{
    std::size_t totalActive = 0;                  ///< Count of active resources.
    std::map<std::string, std::size_t> byType;    ///< Active count by resource type.
    std::map<std::string, std::size_t> byOwnerSystem; ///< Active count by owner.
};

/// Immutable resource snapshot with stable record ordering.
class ResourceSnapshot
{
public:
    ResourceSnapshot();
    /// Capture resource records in deterministic id order.
    explicit ResourceSnapshot(std::vector<ResourceRecord> records);

    [[nodiscard]] const std::vector<ResourceRecord>& Records() const noexcept;
    [[nodiscard]] ResourceLeakSummary BuildActiveSummary() const;

private:
    std::vector<ResourceRecord> records_;
};

} // namespace SagaEngine::Diagnostics
