/// @file EditorCustomizationCatalog.cpp
/// @brief Editor customisation catalog implementation.

#include "SagaEditor/Customization/EditorCustomizationCatalog.h"

#include <cstdlib>
#include <filesystem>

namespace SagaEditor
{

namespace
{

[[nodiscard]] std::filesystem::path ResolveWorkspaceRoot(
    const std::filesystem::path& workspaceRoot)
{
    if (!workspaceRoot.empty())
    {
        return workspaceRoot;
    }
    return std::filesystem::current_path();
}

[[nodiscard]] std::filesystem::path CustomizationRoot(
    const std::filesystem::path& workspaceRoot)
{
    return workspaceRoot / "EditorCustomization";
}

[[nodiscard]] std::filesystem::path UserConfigRoot()
{
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
    {
        return std::filesystem::path(xdg) / "Saga" / "editor";
    }
    if (const char* home = std::getenv("HOME"))
    {
        return std::filesystem::path(home) / ".config" / "Saga" / "editor";
    }
    return std::filesystem::temp_directory_path() / "Saga" / "editor";
}

} // namespace

EditorCustomizationCatalog::EditorCustomizationCatalog() = default;
EditorCustomizationCatalog::~EditorCustomizationCatalog() = default;

bool EditorCustomizationCatalog::Init(const std::filesystem::path& workspaceRoot)
{
    m_status = {};
    m_snapshot = {};
    const std::filesystem::path resolvedWorkspace =
        ResolveWorkspaceRoot(workspaceRoot);
    m_status.sourceRoot = CustomizationRoot(resolvedWorkspace);
    m_status.userConfigRoot = UserConfigRoot();
    m_status.loaded = false;
    m_status.message =
        "Project customization unavailable in this build; using built-in profiles";
    return true;
}

void EditorCustomizationCatalog::Shutdown()
{
    m_status.loaded = false;
    m_snapshot = {};
}

const EditorCustomizationStatus& EditorCustomizationCatalog::Status() const noexcept
{
    return m_status;
}

const EditorCustomizationSnapshot& EditorCustomizationCatalog::Snapshot() const noexcept
{
    return m_snapshot;
}

} // namespace SagaEditor
