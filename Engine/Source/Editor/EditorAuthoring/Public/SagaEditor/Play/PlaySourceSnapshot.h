// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{
class EditorDocumentWorkspace;

struct PlaySourceFileRevision
{
    std::string kind;
    std::string documentId;
    std::filesystem::path path;
    std::filesystem::path projectRelativePath;
    std::string hash;
};

/// Immutable, saved-disk-only authority captured on the caller thread before
/// Play preparation starts. The shared fence outlives the workspace and makes
/// close/replacement/destruction observable without exposing mutable objects.
struct PlaySourceSnapshot
{
    std::string playSessionId;
    std::string projectId;
    std::filesystem::path projectRoot;
    std::filesystem::path projectPath;
    std::string projectHash;
    std::filesystem::path trackedLockPath;
    std::string trackedLockHash;
    std::vector<PlaySourceFileRevision> sources;
    std::vector<std::string> requiredDocumentIds;
    std::uint64_t workspaceGeneration = 0;
    std::shared_ptr<const std::atomic_uint64_t> workspaceFence;
};

struct PlaySourceSnapshotResult
{
    std::optional<PlaySourceSnapshot> snapshot;
    std::vector<DocumentDiagnostic> diagnostics;
    [[nodiscard]] bool Succeeded() const noexcept
    { return snapshot.has_value() && diagnostics.empty(); }
};

[[nodiscard]] PlaySourceSnapshotResult CapturePlaySourceSnapshot(
    const EditorDocumentWorkspace& workspace, std::string playSessionId);

[[nodiscard]] std::vector<DocumentDiagnostic> VerifyPlaySourceSnapshot(
    const PlaySourceSnapshot& snapshot);
} // namespace SagaEditor
