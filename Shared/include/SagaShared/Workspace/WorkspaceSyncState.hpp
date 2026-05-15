/// @file WorkspaceSyncState.hpp
/// @brief Shared workspace synchronization state vocabulary.

#pragma once

#include <cstdint>

namespace SagaShared::Workspace
{

/// Describes whether a workspace is synchronized with its authoritative source.
enum class WorkspaceSyncState : std::uint8_t
{
    Unknown,
    Clean,
    Dirty,
    Syncing,
    Conflict,
    Offline,
};

} // namespace SagaShared::Workspace
