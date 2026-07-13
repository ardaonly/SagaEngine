/// @file SagaLauncherWindow.cpp
/// @brief Implements project discovery and bounded external Editor launch.

#include "SagaLauncherWindow.h"

#include "SagaProcessLauncher.h"
#include "SagaProjectSystem.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include <sstream>
#include <utility>

namespace SagaProduct
{

class SagaLauncherWindow::Impl
{
public:
    Impl(SagaLauncherWindow& window,
         std::filesystem::path productExecutable,
         ISagaProcessLauncher& processLauncher)
        : m_window(window)
        , m_productExecutable(std::move(productExecutable))
        , m_processLauncher(processLauncher)
    {
        auto* shell = new QWidget(&m_window);
        auto* root = new QVBoxLayout(shell);
        root->setContentsMargins(18, 18, 18, 18);
        root->setSpacing(12);

        auto* title = new QLabel("Saga", shell);
        title->setObjectName("SagaProductTitle");
        root->addWidget(title);
        root->addWidget(new QLabel(
            "Select or create a project to open it in SagaEditor.", shell));

        m_recent = new QListWidget(shell);
        root->addWidget(m_recent, 1);

        auto* buttons = new QHBoxLayout();
        auto* createButton = new QPushButton("Create Project", shell);
        auto* openButton = new QPushButton("Open Project", shell);
        buttons->addWidget(createButton);
        buttons->addWidget(openButton);
        root->addLayout(buttons);

        m_status = new QLabel(
            "Generic Runtime project mode and dedicated server launch are unavailable.",
            shell);
        m_status->setWordWrap(true);
        root->addWidget(m_status);

        QObject::connect(createButton, &QPushButton::clicked,
            [this]() { CreateProject(); });
        QObject::connect(openButton, &QPushButton::clicked,
            [this]() { OpenProject(); });
        QObject::connect(m_recent, &QListWidget::itemDoubleClicked,
            [this](QListWidgetItem* item)
            {
                if (item != nullptr && item->data(Qt::UserRole + 1).toBool())
                {
                    OpenProjectPath(item->data(Qt::UserRole).toString());
                }
            });

        m_window.setWindowTitle("Saga");
        m_window.resize(1100, 720);
        m_window.setCentralWidget(shell);

        QMenu* fileMenu = m_window.menuBar()->addMenu("File");
        QAction* refresh = fileMenu->addAction("Refresh Recent Projects");
        QObject::connect(refresh, &QAction::triggered,
            [this]() { RefreshRecentProjects(); });

        QMenu* helpMenu = m_window.menuBar()->addMenu("Help");
        QAction* about = helpMenu->addAction("About Saga");
        QObject::connect(about, &QAction::triggered,
            [this]()
            {
                QMessageBox::about(&m_window, "About Saga",
                    "Saga is the bounded Product Shell. Editor and Runtime "
                    "remain separate application hosts.");
            });
        RefreshRecentProjects();
    }

private:
    void RefreshRecentProjects()
    {
        m_recent->clear();
        for (const SagaRecentProject& project : m_projectSystem.LoadRecentProjects())
        {
            auto* item = new QListWidgetItem(QString::fromStdString(
                project.displayName + "\n" + project.root.string()));
            item->setData(Qt::UserRole, QString::fromStdString(project.root.string()));
            item->setData(Qt::UserRole + 1, project.exists);
            if (!project.exists)
            {
                item->setText(item->text() + "\nMissing project manifest");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            }
            m_recent->addItem(item);
        }
        if (m_recent->count() == 0)
        {
            auto* item = new QListWidgetItem(
                "No recent projects. Create or open a project.");
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            m_recent->addItem(item);
        }
    }

    void CreateProject()
    {
        const QString parent = QFileDialog::getExistingDirectory(
            &m_window, "Choose Project Parent Directory");
        if (parent.isEmpty()) return;
        bool accepted = false;
        const QString name = QInputDialog::getText(
            &m_window, "Create Saga Project", "Project name:",
            QLineEdit::Normal, "NewSagaProject", &accepted);
        if (!accepted || name.trimmed().isEmpty()) return;

        SagaProjectResult result = m_projectSystem.CreateProject(
            parent.toStdString(), name.trimmed().toStdString());
        if (!result.ok)
        {
            ReportFailure("Create Project Failed", result.error);
            return;
        }
        RefreshRecentProjects();
        LaunchEditor(result.manifest);
    }

    void OpenProject()
    {
        const QString path = QFileDialog::getExistingDirectory(
            &m_window, "Open Saga Project");
        if (!path.isEmpty()) OpenProjectPath(path);
    }

    void OpenProjectPath(const QString& path)
    {
        SagaProjectResult result = m_projectSystem.OpenProject(path.toStdString());
        if (!result.ok)
        {
            ReportFailure("Open Project Failed", result.error);
            return;
        }
        RefreshRecentProjects();
        LaunchEditor(result.manifest);
    }

    void LaunchEditor(const SagaProjectManifest& project)
    {
        SagaProcessLaunchRequest request;
        request.target = SagaProductTargetKind::Editor;
        request.executablePath = m_productExecutable.parent_path() / "SagaEditor";
#if defined(_WIN32)
        if (!std::filesystem::exists(request.executablePath))
        {
            request.executablePath += ".exe";
        }
#endif
        request.arguments = {"--workspace", project.root.string()};
        request.workingDirectory = request.executablePath.parent_path();
        request.mode = SagaProcessExecutionMode::Detached;

        std::ostringstream childOutput;
        std::ostringstream childError;
        const SagaProcessLaunchResult result = m_processLauncher.Launch(
            request, childOutput, childError);
        if (!result.ok)
        {
            ReportFailure("Open Editor Failed",
                result.diagnostics.empty() ? "SagaEditor could not be started." :
                    result.diagnostics.front().message);
            return;
        }
        m_status->setText(QString::fromStdString(
            "SagaEditor opened project: " + project.displayName));
        m_window.statusBar()->showMessage("Editor handoff completed.");
    }

    void ReportFailure(const char* title, const std::string& message)
    {
        QMessageBox::critical(
            &m_window, title, QString::fromStdString(message));
        m_status->setText(QString::fromStdString(message));
    }

    SagaLauncherWindow& m_window;
    std::filesystem::path m_productExecutable;
    ISagaProcessLauncher& m_processLauncher;
    SagaProjectSystem m_projectSystem;
    QListWidget* m_recent = nullptr;
    QLabel* m_status = nullptr;
};

SagaLauncherWindow::SagaLauncherWindow(
    std::filesystem::path productExecutable,
    ISagaProcessLauncher& processLauncher,
    QWidget* parent)
    : QMainWindow(parent)
    , m_impl(new Impl(*this, std::move(productExecutable), processLauncher))
{}

SagaLauncherWindow::~SagaLauncherWindow()
{
    delete m_impl;
}

} // namespace SagaProduct
