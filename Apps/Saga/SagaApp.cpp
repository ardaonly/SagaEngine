/// @file SagaApp.cpp
/// @brief Saga product orchestration lifecycle implementation.

#include "SagaApp.h"

#include "SagaEditorModule.h"
#include "SagaProjectSystem.h"

#include <QApplication>
#include <QByteArray>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

namespace SagaProduct
{
namespace
{

constexpr int kExitOk = 0;
constexpr int kExitStartupFailure = 1;

void ConfigureBundledRuntimeEnvironment(const SagaAppConfig& config)
{
#if defined(Q_OS_LINUX)
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
    {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
#endif

    if (!qgetenv("FONTCONFIG_PATH").isEmpty())
    {
        return;
    }

    const std::filesystem::path fontconfigRoot =
        config.executablePath.parent_path().parent_path() /
        "config" / "fontconfig";
    if (std::filesystem::exists(fontconfigRoot / "fonts.conf"))
    {
        qputenv("FONTCONFIG_PATH",
            QByteArray(fontconfigRoot.string().c_str()));
    }
}

/// Product shell widget with real project/session actions.
class SagaProductShellWidget final : public QWidget
{
public:
    SagaProductShellWidget(SagaProjectSystem& projectSystem,
                           QWidget* parent = nullptr)
        : QWidget(parent)
        , m_projectSystem(projectSystem)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(18, 18, 18, 18);
        root->setSpacing(12);

        auto* title = new QLabel("Saga", this);
        title->setObjectName("SagaProductTitle");
        root->addWidget(title);

        auto* subtitle = new QLabel(
            "Select or create a project to enter editor mode.", this);
        root->addWidget(subtitle);

        m_recent = new QListWidget(this);
        root->addWidget(m_recent, 1);

        auto* projectButtons = new QHBoxLayout();
        auto* createButton = new QPushButton("Create Project", this);
        auto* openButton = new QPushButton("Open Project", this);
        projectButtons->addWidget(createButton);
        projectButtons->addWidget(openButton);
        root->addLayout(projectButtons);

        auto* sessionRow = new QHBoxLayout();
        auto* hostButton = new QPushButton("Host Session", this);
        m_roomCode = new QLineEdit(this);
        m_roomCode->setPlaceholderText("ROOM-1234");
        auto* joinButton = new QPushButton("Join Room Code", this);
        sessionRow->addWidget(hostButton);
        sessionRow->addWidget(m_roomCode, 1);
        sessionRow->addWidget(joinButton);
        root->addLayout(sessionRow);

        m_status = new QLabel("Ready", this);
        m_status->setWordWrap(true);
        root->addWidget(m_status);

        auto* footer = new QHBoxLayout();
        auto* settingsButton = new QPushButton("Settings", this);
        auto* aboutButton = new QPushButton("About", this);
        footer->addStretch();
        footer->addWidget(settingsButton);
        footer->addWidget(aboutButton);
        root->addLayout(footer);

        QObject::connect(createButton, &QPushButton::clicked,
            [this]() { CreateProject(); });
        QObject::connect(openButton, &QPushButton::clicked,
            [this]() { OpenProject(); });
        QObject::connect(hostButton, &QPushButton::clicked,
            [this]() { HostSession(); });
        QObject::connect(joinButton, &QPushButton::clicked,
            [this]() { JoinRoom(); });
        QObject::connect(settingsButton, &QPushButton::clicked,
            [this]()
            {
                QMessageBox::information(this, "Saga Settings",
                    QString::fromStdString(
                        "Recent projects: " +
                        m_projectSystem.RecentProjectsPath().string()));
            });
        QObject::connect(aboutButton, &QPushButton::clicked,
            [this]()
            {
                QMessageBox::about(this, "About Saga",
                    "Saga is the primary production executable.");
            });
        QObject::connect(m_recent, &QListWidget::itemDoubleClicked,
            [this](QListWidgetItem* item)
            {
                if (item == nullptr ||
                    item->data(Qt::UserRole + 1).toBool() == false)
                {
                    return;
                }
                OpenProjectPath(item->data(Qt::UserRole).toString());
            });

        RefreshRecentProjects();
    }

    std::function<void(SagaProjectManifest, std::string)> onEnterEditor;

    void RefreshRecentProjects()
    {
        m_recent->clear();
        for (const SagaRecentProject& project :
             m_projectSystem.LoadRecentProjects())
        {
            auto* item = new QListWidgetItem(
                QString::fromStdString(project.displayName + "\n" +
                    project.root.string()));
            item->setData(Qt::UserRole,
                QString::fromStdString(project.root.string()));
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
            auto* empty = new QListWidgetItem(
                "No recent projects. Create or open a project.");
            empty->setFlags(empty->flags() & ~Qt::ItemIsEnabled);
            m_recent->addItem(empty);
        }
    }

    void SetStatus(const QString& text)
    {
        m_status->setText(text);
    }

private:
    void CreateProject()
    {
        const QString parent = QFileDialog::getExistingDirectory(
            this, "Choose Project Parent Directory");
        if (parent.isEmpty())
        {
            return;
        }

        bool ok = false;
        const QString name = QInputDialog::getText(
            this, "Create Saga Project", "Project name:",
            QLineEdit::Normal, "NewSagaProject", &ok);
        if (!ok || name.trimmed().isEmpty())
        {
            return;
        }

        SagaProjectResult result = m_projectSystem.CreateProject(
            parent.toStdString(), name.trimmed().toStdString());
        if (!result.ok)
        {
            QMessageBox::critical(this, "Create Project Failed",
                QString::fromStdString(result.error));
            SetStatus(QString::fromStdString(result.error));
            return;
        }
        RefreshRecentProjects();
        EnterEditor(std::move(result.manifest), {});
    }

    void OpenProject()
    {
        const QString path = QFileDialog::getExistingDirectory(
            this, "Open Saga Project");
        if (path.isEmpty())
        {
            return;
        }
        OpenProjectPath(path);
    }

    void OpenProjectPath(const QString& path)
    {
        SagaProjectResult result =
            m_projectSystem.OpenProject(path.toStdString());
        if (!result.ok)
        {
            QMessageBox::critical(this, "Open Project Failed",
                QString::fromStdString(result.error));
            SetStatus(QString::fromStdString(result.error));
            return;
        }
        RefreshRecentProjects();
        EnterEditor(std::move(result.manifest), {});
    }

    void HostSession()
    {
        m_localSession = m_projectSystem.StartLocalSession();
        SetStatus(QString::fromStdString(
            m_localSession.status + " Room code: " + m_localSession.roomCode));
    }

    void JoinRoom()
    {
        const std::string roomCode = m_roomCode->text().trimmed().toStdString();
        if (const std::optional<std::string> error =
                SagaProjectSystem::ValidateRoomCode(roomCode))
        {
            QMessageBox::warning(this, "Invalid Room Code",
                QString::fromStdString(*error));
            SetStatus(QString::fromStdString(*error));
            return;
        }
        const std::string message =
            "Join flow accepted the room code format, but network backend is unavailable.";
        QMessageBox::information(this, "Network Backend Unavailable",
            QString::fromStdString(message));
        SetStatus(QString::fromStdString(message));
    }

    void EnterEditor(SagaProjectManifest manifest, std::string sessionLabel)
    {
        if (sessionLabel.empty() && m_localSession.hosting)
        {
            sessionLabel = m_localSession.status + " Room code: " +
                m_localSession.roomCode;
        }
        if (onEnterEditor)
        {
            onEnterEditor(std::move(manifest), std::move(sessionLabel));
        }
    }

    SagaProjectSystem& m_projectSystem;
    QListWidget* m_recent = nullptr;
    QLineEdit* m_roomCode = nullptr;
    QLabel* m_status = nullptr;
    SagaLocalSession m_localSession;
};

/// Saga-owned primary window. It swaps product/editor views in one process.
class SagaMainWindow final : public QMainWindow
{
public:
    SagaMainWindow()
    {
        setWindowTitle("Saga");
        resize(1500, 900);

        m_stack = new QStackedWidget(this);
        setCentralWidget(m_stack);
        m_productShell = new SagaProductShellWidget(m_projectSystem, this);
        m_stack->addWidget(m_productShell);
        m_stack->setCurrentWidget(m_productShell);

        m_productShell->onEnterEditor =
            [this](SagaProjectManifest manifest, std::string sessionLabel)
            {
                EnterEditorMode(std::move(manifest), std::move(sessionLabel));
            };

        BuildProductMenu();
        statusBar()->showMessage("Ready");
    }

    void BuildProductMenu()
    {
        menuBar()->clear();
        QMenu* fileMenu = menuBar()->addMenu("File");
        QAction* refresh = fileMenu->addAction("Refresh Recent Projects");
        QObject::connect(refresh, &QAction::triggered,
            [this]()
            {
                m_productShell->RefreshRecentProjects();
                statusBar()->showMessage("Recent projects refreshed.");
            });

        QMenu* helpMenu = menuBar()->addMenu("Help");
        QAction* about = helpMenu->addAction("About Saga");
        QObject::connect(about, &QAction::triggered,
            [this]()
            {
                QMessageBox::about(this, "About Saga",
                    "Saga is the primary production executable.");
            });
    }

    void EnterEditorMode(SagaProjectManifest manifest, std::string sessionLabel)
    {
        SagaPreparedProjectSession session;
        session.project = std::move(manifest);
        session.sessionLabel = std::move(sessionLabel);

        std::string error;
        if (!m_editorModule.Activate(
                *this, *m_stack, std::move(session),
                [this]() { ReturnToProductShell(); }, error))
        {
            QMessageBox::critical(this, "Editor Mode Failed",
                QString::fromStdString(error));
            statusBar()->showMessage(QString::fromStdString(error));
            return;
        }

        statusBar()->showMessage("Editor mode active.");
    }

    void ReturnToProductShell()
    {
        m_editorModule.Shutdown(*this, *m_stack);
        m_stack->setCurrentWidget(m_productShell);
        setWindowTitle("Saga");
        BuildProductMenu();
        m_productShell->RefreshRecentProjects();
        statusBar()->showMessage("Project closed. Product shell active.");
    }

private:
    SagaProjectSystem m_projectSystem;
    QStackedWidget* m_stack = nullptr;
    SagaProductShellWidget* m_productShell = nullptr;
    SagaEditorModule m_editorModule;
};

} // namespace

int SagaApp::Run(int argc,
                 char* argv[],
                 const SagaAppConfig& config,
                 std::ostream& out,
                 std::ostream& err)
{
    if (config.prepareOnly)
    {
        return Run(config, out, err);
    }

    ConfigureBundledRuntimeEnvironment(config);

    QApplication qtApp(argc, argv);
    SagaMainWindow window;
    window.show();
    return qtApp.exec();
}

int SagaApp::Run(const SagaAppConfig& config,
                 std::ostream& out,
                 std::ostream& err)
{
    std::string productInfoError;
    const SagaProductInfo productInfo =
        LoadProductInfo(config, productInfoError);
    if (!productInfoError.empty())
    {
        err << productInfoError << '\n';
    }

    SagaWorkspaceResolveRequest request;
    request.selector = config.workspaceSelector;
    request.builtInBasicRoot = config.builtInWorkspaceRoot;

    const SagaWorkspaceResolveResult workspaceResult =
        m_workspaceResolver.Resolve(request);
    if (!workspaceResult.ok)
    {
        err << workspaceResult.error << '\n';
        for (const std::string& diagnostic : workspaceResult.diagnostics)
        {
            err << diagnostic << '\n';
        }
        return kExitStartupFailure;
    }

    SagaSessionModel session;
    session.workspace = workspaceResult.workspace;
    session.target = config.target;

    const SagaPreparedTarget prepared = m_productHost.PrepareTarget(session);
    out << productInfo.productName << " " << productInfo.buildVersion << '\n';
    out << "workspace=" << prepared.workspace.id << '\n';
    out << "target=" << ToString(prepared.kind) << '\n';
    out << "executable=" << prepared.executableName << '\n';
    out << "module=" << prepared.moduleName << '\n';
    out << "sameProcess=" << (prepared.sameProcess ? "true" : "false") << '\n';
    return kExitOk;
}

} // namespace SagaProduct
