/// @file LifetimeTracker.hpp
/// @brief Tracks logical object lifetime and ownership for runtime diagnostics.

#pragma once

#include <SagaEngine/Diagnostics/Lifetime/LifetimeHandle.hpp>
#include <SagaEngine/Diagnostics/Lifetime/LifetimeRecord.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Diagnostics
{

/// Thread-safe registry for detecting logical lifetime leaks.
class LifetimeTracker
{
public:
    /// Register a logical object and return its diagnostics handle.
    [[nodiscard]] LifetimeHandle Register(std::string typeName,
                                          std::string ownerSystem,
                                          std::uint64_t externalId = 0,
                                          std::uint64_t createdTick = 0);

    /// Mark a tracked logical object as destroyed.
    bool MarkDestroyed(LifetimeHandle handle, std::uint64_t destroyedTick = 0);
    /// Assign a tracked owner object to a tracked child object.
    bool SetOwner(LifetimeHandle child, LifetimeHandle owner);

    [[nodiscard]] std::vector<LifetimeRecord> SnapshotAll() const;
    [[nodiscard]] std::vector<LifetimeRecord> SnapshotActive() const;
    [[nodiscard]] std::vector<LifetimeRecord> FindUndestroyedByOwner(
        std::uint64_t ownerObjectId) const;

    void Clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, LifetimeRecord> records_;
    std::uint64_t nextObjectId_ = 1;
};

} // namespace SagaEngine::Diagnostics
