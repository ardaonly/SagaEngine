/// @file EditorCustomizationCatalog.h
/// @brief Loads editor customisation contracts without changing editor semantics.

#pragma once

#include "SagaEditor/Persona/UIPersona.h"
#include "SagaEditor/Profile/EditorProfile.h"

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
    std::vector<EditorProfile> projectProfiles;
    std::vector<UIPersona>     projectPersonas;
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
