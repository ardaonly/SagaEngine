/// @file ResourceTracker.hpp
/// @brief Tracks explicit non-memory resources for diagnostics.

#pragma once

#include <SagaEngine/Diagnostics/Resource/ResourceSnapshot.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace SagaEngine::Diagnostics
{

/// Thread-safe explicit non-memory resource tracker.
class ResourceTracker
{
public:
    /// Register a resource and return its diagnostics-local id.
    [[nodiscard]] std::uint64_t RegisterResource(ResourceType type,
                                                 std::string ownerSystem,
                                                 std::string debugName,
                                                 std::uint64_t tick = 0);
    /// Release a resource by diagnostics-local id.
    bool ReleaseResource(std::uint64_t resourceId, std::uint64_t tick = 0);

    [[nodiscard]] ResourceSnapshot SnapshotActive() const;
    [[nodiscard]] ResourceSnapshot SnapshotAll() const;
    [[nodiscard]] ResourceLeakSummary BuildResourceLeakSummary() const;
    void Clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, ResourceRecord> records_;
    std::uint64_t nextResourceId_ = 1;
};

} // namespace SagaEngine::Diagnostics
