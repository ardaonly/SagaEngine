/// @file EditorMode.cpp
/// @brief EditorMode implementation.

#include "SagaEditor/Shell/EditorMode.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Settings/IEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include <utility>

namespace SagaEditor
{

EditorMode::EditorMode() = default;

EditorMode::~EditorMode()
{
    Unmount();
}

EditorMode::EditorMode(EditorMode&&) noexcept = default;

EditorMode& EditorMode::operator=(EditorMode&& other) noexcept
{
    if (this != &other)
    {
        Unmount();
        m_host = std::move(other.m_host);
        m_shell = std::move(other.m_shell);
    }

    return *this;
}

bool EditorMode::Mount(std::unique_ptr<IUIMainWindow> window,
                       std::unique_ptr<IEditorSettingsStore> settingsStore,
                       const EditorModeConfig& config)
{
    if (IsMounted() || !window || !settingsStore)
        return false;

    auto host = std::make_unique<EditorHost>();
    if (!host->Init(std::move(settingsStore), config.workspacePath))
        return false;

    if (!config.initialProfileId.empty() &&
        !host->GetEditorProfileRegistry().SetActive(config.initialProfileId))
    {
        host->Shutdown();
        return false;
    }

    EditorShellConfig shellConfig;
    shellConfig.windowTitle = config.windowTitle;
    shellConfig.windowWidth = config.windowWidth;
    shellConfig.windowHeight = config.windowHeight;
    shellConfig.maximized = config.maximized;
    shellConfig.workspacePath = config.workspacePath;
    shellConfig.layoutPreset = config.layoutPreset;
    shellConfig.registerDefaultPanels = config.registerDefaultPanels;
    shellConfig.applyActivePersona = config.applyActivePersona;
    shellConfig.showOnInit = config.showOnMount;

    auto shell = std::make_unique<EditorShell>();
    if (!shell->Init(*host, std::move(window), shellConfig))
    {
        host->Shutdown();
        return false;
    }

    m_host = std::move(host);
    m_shell = std::move(shell);
    return true;
}

void EditorMode::Unmount()
{
    if (m_shell)
        m_shell->Shutdown();
    if (m_host)
        m_host->Shutdown();

    m_shell.reset();
    m_host.reset();
}

bool EditorMode::IsMounted() const noexcept
{
    return m_host != nullptr && m_shell != nullptr;
}

EditorHost* EditorMode::GetHost() noexcept
{
    return m_host.get();
}

const EditorHost* EditorMode::GetHost() const noexcept
{
    return m_host.get();
}

EditorShell* EditorMode::GetShell() noexcept
{
    return m_shell.get();
}

const EditorShell* EditorMode::GetShell() const noexcept
{
    return m_shell.get();
}

} // namespace SagaEditor
