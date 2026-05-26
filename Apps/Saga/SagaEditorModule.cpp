/// @file SagaEditorModule.cpp
/// @brief Same-process Saga editor mode facade implementation.

#include "SagaEditorModule.h"

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Host/EditorWorkspaceDefinition.h"
#include "SagaEditor/Panels/AssetBrowserPanel.h"
#include "SagaEditor/Panels/ConsolePanel.h"
#include "SagaEditor/Panels/IPanel.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/Shell/ShellLayout.h"
#include "SagaEditor/UI/Qt/QtUIMainWindow.h"
#include "SagaEditor/Commands/CommandRegistry.h"

#include <filesystem>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace SagaProduct
{
namespace
{

void InsertFileCloseProject(SagaEditor::ShellLayout& layout)
{
    for (SagaEditor::MenuDescriptor& menu : layout.menus)
    {
        if (menu.title != "File")
        {
            continue;
        }

        auto insertAt = menu.items.end();
        for (auto it = menu.items.begin(); it != menu.items.end(); ++it)
        {
            if (it->commandId == "saga.command.file.exit")
            {
                insertAt = it;
                break;
            }
        }

        menu.items.insert(insertAt, {
            "Close Project",
            "saga.command.file.close_project",
            "",
            false
        });
        return;
    }
}

void AddProductProjectMenu(SagaEditor::ShellLayout& layout)
{
    SagaEditor::MenuDescriptor project;
    project.title = "Project";
    project.items = {
        {
            "Validate/Compile Project Data",
            "saga.command.build",
            "",
            false
        },
    };

    auto insertAt = layout.menus.end();
    if (!layout.menus.empty())
    {
        insertAt = std::next(layout.menus.begin());
    }

    layout.menus.insert(insertAt, std::move(project));
}

[[nodiscard]] std::string FormatCompileSummary(const SagaSdeCompileResult& result)
{
    if (result.ok && result.hashes)
    {
        return "SDE clean | artifact=" + result.hashes->artifactHash;
    }

    return result.message;
}

[[nodiscard]] std::vector<EditorModePanelProvider>& EditorModePanelProviders()
{
    static std::vector<EditorModePanelProvider> providers;
    return providers;
}

void ApplyEditorModePanelProviders(SagaEditor::EditorShell& shell)
{
    for (const EditorModePanelProvider& provider : EditorModePanelProviders())
    {
        if (provider)
        {
            provider(shell);
        }
    }
}

} // namespace

struct SagaEditorModule::Impl
{
    SagaPreparedProjectSession session;
    bool active = false;
    CloseProjectCallback onCloseProject;
    SagaSdeCompiler sdeCompiler;
    std::unique_ptr<SagaEditor::EditorHost> host;
    std::unique_ptr<SagaEditor::EditorShell> shell;

    void Log(SagaEditor::LogSeverity severity, const std::string& message)
    {
        if (!shell)
        {
            return;
        }

        if (auto* console = dynamic_cast<SagaEditor::ConsolePanel*>(
                shell->FindPanel("saga.panel.console")))
        {
            console->Log(severity, message);
        }
    }

    void ConfigureProjectPanels()
    {
        if (!shell)
        {
            return;
        }

        if (auto* assets = dynamic_cast<SagaEditor::AssetBrowserPanel*>(
                shell->FindPanel("saga.panel.assetbrowser")))
        {
            assets->SetWorkspaceRoot(session.project.root.string());
        }
    }

    void RegisterProductCommands()
    {
        auto& registry = host->GetCommandRegistry();
        registry.Register({
            "saga.command.file.close_project",
            "Close Project",
            "File",
            [this]()
            {
                if (onCloseProject)
                {
                    onCloseProject();
                }
            },
            true
        });
        registry.Register({
            "saga.command.file.exit",
            "Close Project",
            "File",
            [this]()
            {
                if (onCloseProject)
                {
                    onCloseProject();
                }
            },
            true
        });
        registry.Register({
            "saga.command.build",
            "Validate/Compile Project Data",
            "Project",
            [this]()
            {
                (void)CompileProjectData();
            },
            true
        });
    }

    void ApplyProductShellLayout()
    {
        SagaEditor::ShellLayout layout = shell->GetShellLayout();
        InsertFileCloseProject(layout);
        AddProductProjectMenu(layout);
        shell->SetShellLayout(std::move(layout));
    }

    [[nodiscard]] SagaSdeCompileResult CompileProjectData()
    {
        const std::filesystem::path compileRoot =
            session.project.sdeRoot.empty()
                ? session.project.root
                : session.project.sdeRoot;
        SagaSdeCompileResult result =
            sdeCompiler.Compile(compileRoot);

        Log(result.ok ? SagaEditor::LogSeverity::Info
                      : SagaEditor::LogSeverity::Error,
            result.message);
        Log(SagaEditor::LogSeverity::Info, "state=" + result.state);
        if (result.hashes)
        {
            Log(SagaEditor::LogSeverity::Info,
                "schemaHash=" + result.hashes->schemaHash);
            Log(SagaEditor::LogSeverity::Info,
                "dataHash=" + result.hashes->dataHash);
            Log(SagaEditor::LogSeverity::Info,
                "artifactHash=" + result.hashes->artifactHash);
        }
        for (const std::string& diagnostic : result.diagnostics)
        {
            Log(result.ok ? SagaEditor::LogSeverity::Info
                          : SagaEditor::LogSeverity::Error,
                diagnostic);
        }

        if (shell)
        {
            shell->GetMainWindow().SetStatusMessage(
                FormatCompileSummary(result));
        }
        return result;
    }
};

SagaEditorModule::SagaEditorModule()
    : m_impl(std::make_unique<Impl>())
{}

SagaEditorModule::~SagaEditorModule() = default;

bool SagaEditorModule::Activate(SagaEditorNativeMount mount,
                                SagaPreparedProjectSession session,
                                CloseProjectCallback onCloseProject,
                                std::string& outError)
{
    if (m_impl->active)
    {
        outError = "Editor mode is already active.";
        return false;
    }

    if (mount.mainWindow == nullptr || mount.centralStack == nullptr)
    {
        outError = "Editor mode requires a native product window mount.";
        return false;
    }

    m_impl->session = std::move(session);
    m_impl->onCloseProject = std::move(onCloseProject);
    m_impl->host = std::make_unique<SagaEditor::EditorHost>();
    m_impl->shell = std::make_unique<SagaEditor::EditorShell>();

    SagaEditor::EditorWorkspaceDefinition workspace;
    workspace.id = m_impl->session.project.projectId;
    workspace.root = m_impl->session.project.root;
    workspace.initialProfileId = "saga.profile.basic";
    workspace.layoutPreset = "Default";
    workspace.sdeValidated = true;

    if (!m_impl->host->Init(
            std::make_unique<SagaEditor::MemoryEditorSettingsStore>(),
            workspace))
    {
        outError = "Editor services failed to initialize.";
        return false;
    }

    if (!m_impl->host->GetEditorProfileRegistry().SetActive(
            workspace.initialProfileId))
    {
        outError = "Editor profile failed to activate.";
        m_impl->host->Shutdown();
        m_impl->host.reset();
        m_impl->shell.reset();
        return false;
    }

    SagaEditor::EditorShellConfig shellConfig;
    shellConfig.windowTitle = "Saga - " + m_impl->session.project.displayName;
    shellConfig.workspacePath = m_impl->session.project.root.string();
    shellConfig.layoutPreset = workspace.layoutPreset;
    shellConfig.maximized = false;
    shellConfig.showOnInit = false;

    auto window = std::make_unique<SagaEditor::QtUIMainWindow>(
        mount.mainWindow,
        mount.centralStack);
    if (!m_impl->shell->Init(*m_impl->host, std::move(window), shellConfig))
    {
        outError = "Editor shell failed to mount into Saga.";
        m_impl->host->Shutdown();
        m_impl->host.reset();
        m_impl->shell.reset();
        return false;
    }

    m_impl->ConfigureProjectPanels();
    ApplyEditorModePanelProviders(*m_impl->shell);
    m_impl->RegisterProductCommands();
    m_impl->ApplyProductShellLayout();
    m_impl->shell->GetMainWindow().SetStatusMessage(
        "Project: " + m_impl->session.project.displayName +
        " | Runtime preview inactive");

    m_impl->Log(SagaEditor::LogSeverity::Info,
        "Editor mode entered in the Saga process.");
    m_impl->Log(SagaEditor::LogSeverity::Warning,
        "Runtime preview is inactive in this build.");
    if (!m_impl->session.sessionLabel.empty())
    {
        m_impl->Log(SagaEditor::LogSeverity::Info,
            m_impl->session.sessionLabel);
    }

    m_impl->active = true;
    return true;
}

void SagaEditorModule::Shutdown()
{
    if (!m_impl->active)
    {
        return;
    }

    if (m_impl->shell)
    {
        m_impl->shell->Shutdown();
    }

    if (m_impl->host)
    {
        m_impl->host->Shutdown();
    }
    m_impl->host.reset();
    m_impl->shell.reset();
    m_impl->active = false;
}

SagaSdeCompileResult SagaEditorModule::ValidateAndCompile()
{
    return m_impl->CompileProjectData();
}

bool SagaEditorModule::IsActive() const noexcept
{
    return m_impl->active;
}

const SagaPreparedProjectSession& SagaEditorModule::Session() const noexcept
{
    return m_impl->session;
}

std::vector<std::string> SagaEditorModule::VisiblePanelList() const
{
    std::vector<std::string> panels;
    if (!m_impl->active)
    {
        return panels;
    }
    if (m_impl->shell)
    {
        panels = m_impl->shell->GetRegisteredPanelTitles();
    }
    return panels;
}

void RegisterEditorModePanelProvider(EditorModePanelProvider provider)
{
    if (provider)
    {
        EditorModePanelProviders().push_back(std::move(provider));
    }
}

void ClearEditorModePanelProviders()
{
    EditorModePanelProviders().clear();
}

} // namespace SagaProduct
