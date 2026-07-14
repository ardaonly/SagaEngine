/// @file LockService.hpp
/// @brief Hard resource lock service boundary for collaboration workflows.

#pragma once

#include "SagaShared/Collaboration/ResourceLock.hpp"

#include <string>
#include <vector>

namespace SagaCollaboration
{

/// Tracks blocking resource locks for collaborative editing.
class LockService
{
public:
    /// Try to acquire a lock; returns false when the resource is already locked.
    [[nodiscard]] bool TryLock(SagaShared::Collaboration::ResourceLock lock);

    /// Release a lock by stable lock id.
    void Release(const std::string& lockId);

    /// Return all active locks.
    [[nodiscard]] const std::vector<SagaShared::Collaboration::ResourceLock>&
    Locks() const noexcept;

private:
    std::vector<SagaShared::Collaboration::ResourceLock> m_locks; ///< Active hard locks.
};

} // namespace SagaCollaboration
