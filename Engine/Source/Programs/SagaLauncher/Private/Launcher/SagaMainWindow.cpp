// SPDX-License-Identifier: MPL-2.0
#include "Launcher/SagaMainWindow.h"

#include "Projects/SagaProjectCatalog.h"
#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Project/ProjectManager.h"
#include "SagaEditor/Project/SagaProjectKitClient.h"
#include "SagaEditor/Shell/EditorWorkspaceSession.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditorQt/QtSettingsStore.h"
#include "SagaEditorQt/QtUIMainWindow.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMenuBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

namespace SagaProduct
{
namespace
{
QString Bounded(QString text, qsizetype limit)
{
    if (text.size() <= limit) return text;
    text.truncate(limit);
    return text + "\n…";
}

void ShowProjectCreationFailure(
    QWidget* parent, const SagaEditor::ProjectToolResult& result)
{
    QMessageBox message(QMessageBox::Warning, "New Project",
        "Project creation failed.", QMessageBox::Ok, parent);
    if (!result.diagnostics.empty())
    {
        const auto& diagnostic = result.diagnostics.front();
        message.setInformativeText(Bounded(
            QString::fromStdString(diagnostic.message), 512));
        QString details = QString::fromStdString(
            diagnostic.code + "\nPath: " + diagnostic.path.string());
        if (result.process.exitCode >= 0)
            details += "\nExit code: " + QString::number(result.process.exitCode);
        if (!result.process.diagnosticMessage.empty())
            details += "\n" + QString::fromStdString(
                result.process.diagnosticMessage);
        if (!result.process.standardError.empty())
            details += "\n\n" + QString::fromStdString(
                result.process.standardError);
        message.setDetailedText(Bounded(details, 4096));
    }
    message.exec();
}

std::filesystem::path RecentPath()
{
    const QString root = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    return std::filesystem::path(root.toStdString()) / "recent_projects.json";
}

SagaEditor::ProjectWorkflowPrompts MakeProjectWorkflowPrompts(QMainWindow* parent)
{
    SagaEditor::ProjectWorkflowPrompts prompts;
    prompts.chooseExistingProject = [parent]()
        -> std::optional<std::filesystem::path>
    {
        const QString path = QFileDialog::getOpenFileName(
            parent, "Open Saga Project", {}, "Saga Project (*.sagaproj)");
        if (path.isEmpty())
            return std::nullopt;
        return std::filesystem::path(path.toStdString());
    };
    prompts.requestNewProject = [parent]()
        -> std::optional<SagaEditor::ProjectCreationPrompt>
    {
        const QString destination = QFileDialog::getExistingDirectory(
            parent, "Choose New Project Parent Folder");
        if (destination.isEmpty())
            return std::nullopt;
        bool accepted = false;
        const QString displayName = QInputDialog::getText(
            parent, "New Project", "Project name:", QLineEdit::Normal,
            "StarterArena", &accepted).trimmed();
        if (!accepted || displayName.isEmpty())
            return std::nullopt;
        return SagaEditor::ProjectCreationPrompt{
            displayName.toStdString(),
            std::filesystem::path(destination.toStdString()) /
                displayName.toStdString()};
    };
    prompts.confirmDirtyDocuments =
        [parent](const std::vector<std::string>& documentIds)
    {
        QString detail;
        for (const auto& id : documentIds)
            detail += QString::fromStdString(id) + QLatin1Char('\n');
        QMessageBox box(
            QMessageBox::Warning,
            "Unsaved source documents",
            "Save canonical source changes before continuing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            parent);
        box.setDetailedText(detail.trimmed());
        box.setDefaultButton(QMessageBox::Save);
        const auto decision = box.exec();
        if (decision == QMessageBox::Save)
            return SagaEditor::DirtyDocumentDecision::SaveAll;
        if (decision == QMessageBox::Discard)
            return SagaEditor::DirtyDocumentDecision::Discard;
        return SagaEditor::DirtyDocumentDecision::Cancel;
    };
    return prompts;
}
}

SagaMainWindow::SagaMainWindow(std::filesystem::path productExecutable,
                               std::filesystem::path initialProject,
                               std::string initialAuthoringMode,
                               QWidget* parent)
    : QMainWindow(parent), executable_(std::move(productExecutable)),
      initialProject_(std::move(initialProject)),
      authoringMode_(std::move(initialAuthoringMode))
{
    setWindowTitle("Saga");
    resize(1600, 900);
    settings_ = std::make_unique<SagaEditor::QtSettingsStore>();
    authoringModes_ = std::make_unique<SagaEditor::AuthoringModeService>();
    if (!authoringModes_->Init(*settings_, authoringMode_))
        (void)authoringModes_->Init(*settings_);
    authoringMode_ = authoringModes_->Active().id;
    stack_ = new QStackedWidget(this);
    setCentralWidget(stack_);
    BuildHome();
    if (!initialProject_.empty())
        (void)OpenProject(initialProject_);
}

SagaMainWindow::~SagaMainWindow()
{
    if (editor_)
        editor_->Unmount();
}

SagaMainWindowState SagaMainWindow::State() const noexcept { return state_; }

void SagaMainWindow::BuildHome()
{
    home_ = new QWidget(stack_);
    home_->setObjectName("SagaHomePage");
    auto* layout = new QVBoxLayout(home_);
    auto* title = new QLabel("Saga", home_);
    title->setObjectName("SagaHomeTitle");
    layout->addWidget(title);
    mode_ = new QComboBox(home_);
    mode_->setObjectName("SagaHomeAuthoringMode");
    mode_->addItem("Level 1", "level1");
    mode_->addItem("Level 2", "level2");
    mode_->addItem("Level 3", "level3");
    const int selected = mode_->findData(QString::fromStdString(authoringMode_));
    mode_->setCurrentIndex(selected < 0 ? 0 : selected);
    QObject::connect(mode_, &QComboBox::currentIndexChanged, this, [this]
    {
        authoringMode_ = mode_->currentData().toString().toStdString();
        (void)authoringModes_->SetActive(authoringMode_);
    });
    layout->addWidget(mode_);
    auto* newProject = new QPushButton("New Project", home_);
    newProject->setObjectName("SagaHomeNewProject");
    auto* open = new QPushButton("Open Project", home_);
    open->setObjectName("SagaHomeOpenProject");
    layout->addWidget(newProject);
    layout->addWidget(open);
    recents_ = new QListWidget(home_);
    recents_->setObjectName("SagaHomeRecentProjects");
    layout->addWidget(recents_, 1);
    QObject::connect(open, &QPushButton::clicked, this, [this]
    {
        const QString path = QFileDialog::getOpenFileName(
            this, "Open Saga Project", {}, "Saga Project (*.sagaproj)");
        if (!path.isEmpty())
            (void)OpenProject(path.toStdString());
    });
    QObject::connect(newProject, &QPushButton::clicked, this, [this]
    {
        const QString destination = QFileDialog::getExistingDirectory(
            this, "Choose New Project Parent Folder");
        if (destination.isEmpty())
            return;
        bool accepted = false;
        const QString displayName = QInputDialog::getText(
            this, "New Project", "Project name:", QLineEdit::Normal,
            "StarterArena", &accepted).trimmed();
        if (!accepted || displayName.isEmpty())
            return;
        const auto root = std::filesystem::path(destination.toStdString()) /
            displayName.toStdString();
        const auto result = CreateProject(root, displayName.toStdString());
        if (!result.passed) ShowProjectCreationFailure(this, result);
    });
    QObject::connect(recents_, &QListWidget::itemActivated, this,
        [this](QListWidgetItem* item)
        {
            if (item)
                (void)OpenProject(item->data(Qt::UserRole).toString().toStdString());
        });
    stack_->addWidget(home_);
    stack_->setCurrentWidget(home_);
    RefreshRecents();
}

SagaEditor::ProjectToolResult SagaMainWindow::CreateProject(
    const std::filesystem::path& destination,
    const std::string& displayName)
{
    const auto toolchain = SagaEditor::ResolveEditorToolchain(executable_);
    auto client = std::make_shared<SagaEditor::SagaProjectKitClient>(
        toolchain.sagaProjectKitExecutable);
    SagaEditor::EditorDocumentWorkspace workspace(std::move(client));
    auto result = workspace.CreateStarterArena(
        SagaEditor::MakeStarterArenaTemplateRequest(
            destination, displayName, executable_));
    if (!result.passed) return result;
    const auto manifest = result.project.manifestPath;
    workspace.Close();
    if (!OpenProject(manifest))
    {
        result.passed = false;
        result.diagnostics.push_back({
            "Project.Home.EditorTransitionFailed",
            "The project was created, but Saga could not open its Editor workspace.",
            manifest});
    }
    return result;
}

void SagaMainWindow::RefreshRecents()
{
    recents_->clear();
    SagaRecentProjectsStore store(RecentPath());
    for (const auto& recent : store.Load())
    {
        auto* item = new QListWidgetItem(
            QString::fromStdString(recent.displayName + "\n" +
                                   recent.canonicalManifestPath.string()), recents_);
        item->setData(Qt::UserRole,
            QString::fromStdString(recent.canonicalManifestPath.string()));
        if (!recent.exists)
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
}

bool SagaMainWindow::OpenProject(const std::filesystem::path& project)
{
    if (state_ == SagaMainWindowState::OpeningProject)
        return false;
    state_ = SagaMainWindowState::OpeningProject;
    authoringMode_ = mode_->currentData().toString().toStdString();
    auto candidate = std::make_unique<SagaEditor::EditorWorkspaceSession>();
    SagaEditor::EditorWorkspaceSessionConfig config;
    config.windowTitle = "Saga";
    config.projectPath = project.string();
    std::error_code pathError;
    config.workspacePath =
        (std::filesystem::is_directory(project, pathError)
             ? project
             : project.parent_path()).string();
    config.authoringModeId = authoringMode_;
    config.runtimeExecutable = executable_.parent_path() / "SagaRuntime";
    config.showOnMount = false;
    config.returnHome = [this] { ReturnHome(); };
    config.workspaceChanged = [this]
    {
        RefreshEditorProjectPresentation();
    };
    config.projectWorkflowPrompts = MakeProjectWorkflowPrompts(this);
    auto window = std::make_unique<SagaEditor::QtUIMainWindow>(*this, *stack_);
    if (!candidate->Mount(std::move(window),
                          std::make_unique<SagaEditor::QtSettingsStore>(), config))
    {
        state_ = SagaMainWindowState::Home;
        stack_->setCurrentWidget(home_);
        statusBar()->showMessage("Project open failed validation; Home is unchanged.");
        return false;
    }

    const auto* info = candidate->GetHost()->GetDocumentWorkspace().Project();
    if (!info)
    {
        candidate->Unmount();
        state_ = SagaMainWindowState::Home;
        return false;
    }
    editor_ = std::move(candidate);
    RefreshEditorProjectPresentation();
    state_ = SagaMainWindowState::Editor;
    return true;
}

void SagaMainWindow::RefreshEditorProjectPresentation()
{
    if (!editor_ || !editor_->GetHost())
        return;
    const auto* info = editor_->GetHost()->GetDocumentWorkspace().Project();
    if (!info)
        return;
    setWindowTitle(QString::fromStdString("Saga — " + info->name));
    SagaLauncherProjectSummary summary;
    summary.projectId = info->projectId;
    summary.displayName = info->name;
    summary.canonicalRoot = info->rootPath;
    summary.canonicalManifestPath = info->manifestPath;
    summary.schemaVersion = static_cast<int>(info->schemaVersion);
    summary.valid = true;
    SagaRecentProjectsStore store(RecentPath());
    std::string error;
    (void)store.Remember(summary, error);
    RefreshRecents();
}

void SagaMainWindow::ReturnHome()
{
    if (!editor_)
        return;
    state_ = SagaMainWindowState::ReturningHome;
    editor_->Unmount();
    editor_.reset();
    menuBar()->clear();
    setWindowTitle("Saga");
    (void)authoringModes_->Init(*settings_);
    authoringMode_ = authoringModes_->Active().id;
    const int selected = mode_->findData(QString::fromStdString(authoringMode_));
    mode_->setCurrentIndex(selected < 0 ? 0 : selected);
    stack_->setCurrentWidget(home_);
    RefreshRecents();
    statusBar()->showMessage("Home");
    state_ = SagaMainWindowState::Home;
}

void SagaMainWindow::closeEvent(QCloseEvent* event)
{
    if (!editor_ || editor_->CanCloseWorkspace())
        event->accept();
    else
        event->ignore();
}
} // namespace SagaProduct
