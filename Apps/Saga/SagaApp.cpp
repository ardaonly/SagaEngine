/// @file SagaApp.cpp
/// @brief Saga product orchestration lifecycle implementation.

#include "SagaApp.h"

#include "SagaEditorModule.h"
#include "SagaPackageStaging.h"
#include "SagaProjectSystem.h"
#include "SagaPublishReadiness.h"
#include "SagaScriptGate.h"

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
#include <string>
#include <utility>

namespace SagaProduct
{
namespace
{

constexpr int kExitOk = 0;
constexpr int kExitStartupFailure = 1;

[[nodiscard]] std::filesystem::path ResolvePreparedExecutablePath(
    const SagaAppConfig& config,
    const SagaPreparedTarget& prepared)
{
    std::filesystem::path executablePath = prepared.executableName;
    if (executablePath.is_relative() && !config.executablePath.empty())
    {
        executablePath = config.executablePath.parent_path() / executablePath;
    }
#if defined(_WIN32)
    if (executablePath.extension().empty() &&
        !std::filesystem::exists(executablePath))
    {
        executablePath += ".exe";
    }
#endif
    return executablePath;
}

[[nodiscard]] SagaProcessLaunchRequest MakeLaunchRequest(
    const SagaAppConfig& config,
    const SagaPreparedTarget& prepared)
{
    SagaProcessLaunchRequest request;
    request.target = prepared.kind;
    request.executablePath = ResolvePreparedExecutablePath(config, prepared);
    request.arguments = prepared.arguments;
    request.workingDirectory = request.executablePath.parent_path();
    return request;
}

[[nodiscard]] bool ShouldUsePreparationFlow(
    const SagaAppConfig& config) noexcept
{
    return config.publishCheck ||
        config.stagePackages ||
        config.validateSagaScript ||
        config.prepareOnly ||
        config.target == SagaProductTargetKind::Runtime ||
        config.target == SagaProductTargetKind::Server;
}

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

void WriteProductDiagnostic(std::ostream& output,
                            const SagaProductDiagnostic& diagnostic)
{
    output << "diagnostic.id=" << diagnostic.diagnosticId << '\n';
    output << "diagnostic.target=" << ToString(diagnostic.target) << '\n';
    output << "diagnostic.phase=" << ToString(diagnostic.phase) << '\n';
    output << "diagnostic.message=" << diagnostic.message << '\n';
    if (diagnostic.path.has_value())
    {
        output << "diagnostic.path=" << diagnostic.path->string() << '\n';
    }
}

} // namespace

SagaApp::SagaApp()
    : SagaApp(std::make_unique<SagaProcessLauncher>())
{
}

SagaApp::SagaApp(std::unique_ptr<ISagaProcessLauncher> processLauncher)
    : m_processLauncher(processLauncher ?
          std::move(processLauncher) : std::make_unique<SagaProcessLauncher>())
{
}

int SagaApp::Run(int argc,
                 char* argv[],
                 const SagaAppConfig& config,
                 std::ostream& out,
                 std::ostream& err)
{
    if (ShouldUsePreparationFlow(config))
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
    if (config.validateSagaScript)
    {
        SagaScriptGateRequest request;
        request.projectRoot = config.sagaScriptProjectRoot;
        request.forgeExecutable = config.forgeExecutable;
        request.sagaScriptExecutable = config.sagaScriptExecutable;

        SagaScriptGate gate;
        const SagaScriptGateResult result =
            gate.ValidateProject(request, out, err);
        for (const SagaProductDiagnostic& diagnostic : result.diagnostics)
        {
            WriteProductDiagnostic(err, diagnostic);
        }

        out << "sagascript.source=" << result.paths.sourceRoot.string() << '\n';
        out << "sagascript.manifests="
            << result.paths.manifestOutputDirectory.string() << '\n';
        out << "sagascript.diagnostics="
            << result.paths.diagnosticsOutputPath.string() << '\n';
        if (result.started)
        {
            out << "sagascript.exitCode=" << result.exitCode << '\n';
        }

        if (result.ok)
        {
            return kExitOk;
        }
        return result.started && result.exitCode != 0 ?
            result.exitCode : kExitStartupFailure;
    }

    if (config.stagePackages)
    {
        SagaPackageStagingRequest request;
        request.projectRoot = config.packageStageProjectRoot;
        request.profile = config.packageProfile;
        request.targetPlatform = config.targetPlatform;
        request.runtimeCompatibilityVersion =
            config.runtimeCompatibilityVersion;
        request.reportPath = config.packageStageReportPath;

        SagaPackageStagingService service;
        const SagaPackageStagingResult result = service.Stage(request);

        out << "package.report=" << result.paths.reportPath.string() << '\n';
        out << "package.clientManifest="
            << result.paths.clientPackageManifest.string() << '\n';
        out << "package.serverManifest="
            << result.paths.serverPackageManifest.string() << '\n';
        out << "package.status=" << (result.ok ? "staged" : "blocked")
            << '\n';
        for (const SagaProductDiagnostic& diagnostic : result.diagnostics)
        {
            WriteProductDiagnostic(err, diagnostic);
        }

        return result.ok ? kExitOk : kExitStartupFailure;
    }

    if (config.publishCheck)
    {
        std::string diagnosticsError;
        SagaPublishReadinessRequest request;
        request.projectRoot = config.publishProjectRoot;
        request.profile = config.publishProfile;
        request.reportPath = config.publishReportPath;
        request.diagnosticsInputs =
            SagaPublishReadinessService::ParseDiagnosticsInputs(
                config.publishDiagnostics,
                diagnosticsError);
        if (!diagnosticsError.empty())
        {
            err << diagnosticsError << '\n';
            return kExitStartupFailure;
        }

        SagaPublishReadinessService service;
        const SagaPublishReadinessResult result = service.Check(request);

        out << "publish.report=" << result.reportPath.string() << '\n';
        out << "publish.status="
            << ToString(result.report.readiness.status) << '\n';
        out << "publish.blockers="
            << result.report.readiness.blockers.size() << '\n';
        for (const auto& blocker : result.report.readiness.blockers)
        {
            err << "publish.blocker.kind=" << ToString(blocker.kind) << '\n';
            err << "publish.blocker.message=" << blocker.message << '\n';
        }

        return result.ok ? kExitOk : kExitStartupFailure;
    }

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
    session.packageManifestPath = config.packageManifestPath;

    const SagaTargetPreparationResult preparation =
        m_productHost.PrepareTargetWithDiagnostics(session);
    if (!preparation.ok)
    {
        for (const SagaProductDiagnostic& diagnostic : preparation.diagnostics)
        {
            WriteProductDiagnostic(err, diagnostic);
        }
        return kExitStartupFailure;
    }

    const SagaPreparedTarget& prepared = preparation.target;
    out << productInfo.productName << " " << productInfo.buildVersion << '\n';
    out << "workspace=" << prepared.workspace.id << '\n';
    out << "target=" << ToString(prepared.kind) << '\n';
    out << "executable=" << prepared.executableName << '\n';
    out << "module=" << prepared.moduleName << '\n';
    out << "sameProcess=" << (prepared.sameProcess ? "true" : "false") << '\n';
    for (const std::string& argument : prepared.arguments)
    {
        out << "argument=" << argument << '\n';
    }

    if (config.prepareOnly || prepared.kind == SagaProductTargetKind::Editor)
    {
        return kExitOk;
    }

    const SagaProcessLaunchRequest launchRequest =
        MakeLaunchRequest(config, prepared);
    const SagaProcessLaunchResult launchResult =
        m_processLauncher->Launch(launchRequest, out, err);
    for (const SagaProductDiagnostic& diagnostic : launchResult.diagnostics)
    {
        WriteProductDiagnostic(err, diagnostic);
    }
    if (launchResult.started)
    {
        out << "launch.exitCode=" << launchResult.exitCode << '\n';
    }

    if (!launchResult.started)
    {
        return kExitStartupFailure;
    }
    if (!launchResult.ok)
    {
        return launchResult.exitCode == 0 ?
            kExitStartupFailure : launchResult.exitCode;
    }
    return kExitOk;
}

} // namespace SagaProduct
