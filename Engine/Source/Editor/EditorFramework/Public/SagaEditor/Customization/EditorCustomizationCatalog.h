// SPDX-License-Identifier: MPL-2.0
/// @file EditorCustomizationCatalog.h
/// @brief Loads editor customisation contracts without changing editor semantics.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor
{

struct EditorCustomizationStatus
{
    bool                     loaded = false;
    std::filesystem::path    sourceRoot;
    std::filesystem::path    userConfigRoot;
    std::string              message;
    std::vector<std::string> diagnostics;
};

/// Data-driven editor customisation snapshot. Project-authored entries come
/// from built-in editor contracts; user-private overrides live under the
/// settings store and platform user config directory.
struct EditorCustomizationSnapshot
{
    bool legacyProfileDataIgnored = true;
};

/// Editor-only customisation loader. Customisation remains limited to
/// presentation/workflow declarations and never edits engine/runtime semantics.
class EditorCustomizationCatalog
{
public:
    EditorCustomizationCatalog();
    ~EditorCustomizationCatalog();

    [[nodiscard]] bool Init(const std::filesystem::path& workspaceRoot);
    void Shutdown();

    [[nodiscard]] const EditorCustomizationStatus& Status() const noexcept;
    [[nodiscard]] const EditorCustomizationSnapshot& Snapshot() const noexcept;

private:
    EditorCustomizationStatus m_status;
    EditorCustomizationSnapshot m_snapshot;
};

} // namespace SagaEditor
