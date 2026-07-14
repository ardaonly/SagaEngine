/// @file WorkspaceDescriptor.hpp
/// @brief Neutral workspace descriptor used by editor, runtime, and collaboration layers.

#pragma once

#include "SagaShared/Workspace/WorkspaceSyncState.hpp"

#include <string>
#include <vector>

namespace SagaShared::Workspace
{

/// Local or collaborative workspace state without lifecycle ownership.
struct WorkspaceDescriptor
{
    std::string workspaceId;       ///< Stable workspace identifier.
    std::string projectId;         ///< Owning project identifier.
    std::string localRoot;         ///< Local filesystem root when available.
    std::string activeUserId;      ///< Current actor identifier.
    WorkspaceSyncState syncState = WorkspaceSyncState::Unknown; ///< Current sync state.
    std::vector<std::string> dirtyResources; ///< Resources with local unpublished edits.
    std::string activeModeHint;    ///< Optional editor/runtime/server mode hint.
};

} // namespace SagaShared::Workspace
