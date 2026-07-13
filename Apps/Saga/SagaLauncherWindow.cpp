/// @file SagaLauncherWindow.cpp
/// @brief Renders launcher state and dispatches typed controller actions.

#include "SagaLauncherWindow.h"

#include "SagaLauncherController.h"

#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <memory>
#include <utility>

namespace SagaProduct
{
namespace
{

[[nodiscard]] std::filesystem::path StandardPath(QStandardPaths::StandardLocation location,
                                                 const std::filesystem::path& fallback)
{
    const QString path = QStandardPaths::writableLocation(location);
    return path.isEmpty() ? fallback : std::filesystem::path(path.toStdString());
}

[[nodiscard]] QString DisplayPath(const std::filesystem::path& path)
{
    return path.empty() ? QStringLiteral("Not configured") : QString::fromStdString(path.string());
}

[[nodiscard]] std::unique_ptr<SagaLauncherController> MakeController(
    const std::filesystem::path& productExecutable, const std::filesystem::path& distributionReport)
{
    const auto configRoot = StandardPath(QStandardPaths::AppConfigLocation,
                                         std::filesystem::temp_directory_path() / "Saga" /
                                             "config");
    const auto dataRoot = StandardPath(QStandardPaths::AppLocalDataLocation,
                                       std::filesystem::temp_directory_path() / "Saga" / "data");
    SagaLauncherPathPolicy policy;
    policy.reportsRoot = dataRoot / "reports";
    policy.evidenceRoot = dataRoot / "evidence";
    const auto bridge = productExecutable.parent_path().parent_path() / "Managed" /
                        "SagaScript.RuntimeBridge" / "SagaScript.RuntimeBridge.dll";

    return std::make_unique<SagaLauncherController>(
        std::make_unique<SagaProjectCatalog>(),
        std::make_unique<SagaRecentProjectsStore>(configRoot / "recent_projects.json"),
        std::make_unique<SagaTargetResolver>(SagaExecutableCatalog(productExecutable), bridge),
        std::make_unique<SagaLauncherActionExecutor>(
            SagaExecutableCatalog(productExecutable), policy, bridge),
        std::make_unique<SagaReportCatalog>(policy.reportsRoot, policy.evidenceRoot),
        std::make_unique<SagaDistributionStatusReader>(productExecutable, distributionReport),
        std::make_unique<SagaLauncherTaskRunner>());
}

} // namespace

class SagaLauncherWindow::Impl
{
public:
    Impl(SagaLauncherWindow& window,
         std::filesystem::path productExecutable,
         std::filesystem::path distributionReport)
        : m_window(window),
          m_controller(MakeController(std::move(productExecutable), distributionReport))
    {
        auto* scroll = new QScrollArea(&m_window);
        scroll->setWidgetResizable(true);
        scroll->setObjectName("SagaLauncherScrollArea");
        auto* shell = new QWidget(scroll);
        auto* root = new QVBoxLayout(shell);
        root->setContentsMargins(18, 18, 18, 18);
        root->setSpacing(12);

        auto* title = new QLabel("Saga Product Launcher", shell);
        title->setObjectName("SagaProductTitle");
        root->addWidget(title);

        BuildProjectSection(root, shell);
        BuildTargetsSection(root, shell);
        BuildReportsSection(root, shell);
        BuildDistributionSection(root, shell);
        BuildLimitationsSection(root, shell);
        BuildDiagnosticsSection(root, shell);

        auto* progressRow = new QHBoxLayout();
        m_progress = new QProgressBar(shell);
        m_progress->setObjectName("SagaLauncherActionProgress");
        m_progress->setRange(0, 1);
        m_progress->setValue(1);
        m_cancel = new QPushButton("Cancel", shell);
        m_cancel->setObjectName("SagaLauncherCancelActionButton");
        m_cancel->setEnabled(false);
        progressRow->addWidget(m_progress, 1);
        progressRow->addWidget(m_cancel);
        root->addLayout(progressRow);

        QObject::connect(m_cancel, &QPushButton::clicked, [this]() {
            const auto state = m_controller->GetState();
            if (state.runningActionId.has_value())
                (void)m_controller->CancelAction(*state.runningActionId);
        });

        scroll->setWidget(shell);
        m_window.setCentralWidget(scroll);
        m_window.setWindowTitle("Saga Product Launcher");
        m_window.resize(1120, 820);

        QMenu* fileMenu = m_window.menuBar()->addMenu("File");
        QAction* refresh = fileMenu->addAction("Refresh Reports");
        QObject::connect(refresh, &QAction::triggered, [this]() {
            (void)m_controller->RunAction(SagaLauncherActionId::RefreshReports);
        });

        m_controller->SetStateChangedCallback([this](const SagaLauncherState& state) {
            QMetaObject::invokeMethod(
                &m_window, [this, state]() { Render(state); }, Qt::QueuedConnection);
        });
        m_controller->LoadInitialState();
        Render(m_controller->GetState());
    }

private:
    void BuildProjectSection(QVBoxLayout* root, QWidget* parent)
    {
        auto* group = new QGroupBox("Project", parent);
        group->setObjectName("SagaLauncherProjectSection");
        auto* layout = new QVBoxLayout(group);
        m_project = new QLabel("No project selected", group);
        m_project->setObjectName("SagaLauncherSelectedProject");
        m_project->setWordWrap(true);
        layout->addWidget(m_project);
        m_recent = new QListWidget(group);
        m_recent->setObjectName("SagaLauncherRecentProjects");
        layout->addWidget(m_recent);
        auto* buttons = new QHBoxLayout();
        auto* open = new QPushButton("Open Project", group);
        open->setObjectName("SagaLauncherOpenProjectButton");
        m_validate = new QPushButton("Validate Project", group);
        m_validate->setObjectName("SagaLauncherValidateProjectButton");
        buttons->addWidget(open);
        buttons->addWidget(m_validate);
        layout->addLayout(buttons);
        QObject::connect(open, &QPushButton::clicked, [this]() {
            const QString path = QFileDialog::getOpenFileName(
                &m_window, "Open Saga Project", {}, "Saga Project (*.sagaproj)");
            if (!path.isEmpty())
                (void)m_controller->OpenProject(path.toStdString());
        });
        QObject::connect(m_validate, &QPushButton::clicked, [this]() {
            (void)m_controller->ValidateProject();
        });
        QObject::connect(m_recent, &QListWidget::itemActivated, [this](QListWidgetItem* item) {
            if (item != nullptr && item->data(Qt::UserRole + 1).toBool())
                (void)m_controller->OpenProject(item->data(Qt::UserRole).toString().toStdString());
        });
        root->addWidget(group);
    }

    void BuildTargetsSection(QVBoxLayout* root, QWidget* parent)
    {
        auto* group = new QGroupBox("Targets and Actions", parent);
        group->setObjectName("SagaLauncherTargetsSection");
        auto* layout = new QVBoxLayout(group);
        auto* row = new QHBoxLayout();
        m_editor = ActionButton("Open SagaEditor",
                                "SagaLauncherOpenEditorButton",
                                SagaLauncherActionId::OpenEditor,
                                group);
        m_smoke = ActionButton("Runtime smoke",
                               "SagaLauncherRuntimeSmokeButton",
                               SagaLauncherActionId::RuntimeStarterArenaSmoke,
                               group);
        m_playable = ActionButton("Runtime playable",
                                  "SagaLauncherRuntimePlayableButton",
                                  SagaLauncherActionId::RuntimeStarterArenaPlayable,
                                  group);
        m_firstPlayable = ActionButton("First-playable check",
                                       "SagaLauncherFirstPlayableButton",
                                       SagaLauncherActionId::FirstPlayableCheck,
                                       group);
        row->addWidget(m_editor);
        row->addWidget(m_smoke);
        row->addWidget(m_playable);
        row->addWidget(m_firstPlayable);
        layout->addLayout(row);
        m_targets = new QTreeWidget(group);
        m_targets->setObjectName("SagaLauncherTargets");
        m_targets->setHeaderLabels({"Target", "Status", "Owner", "Diagnostic"});
        layout->addWidget(m_targets);
        root->addWidget(group);
    }

    QPushButton* ActionButton(const char* label,
                              const char* objectName,
                              SagaLauncherActionId action,
                              QWidget* parent)
    {
        auto* button = new QPushButton(label, parent);
        button->setObjectName(objectName);
        QObject::connect(button, &QPushButton::clicked, [this, action]() {
            (void)m_controller->RunAction(action);
        });
        return button;
    }

    void BuildReportsSection(QVBoxLayout* root, QWidget* parent)
    {
        auto* group = new QGroupBox("Reports", parent);
        group->setObjectName("SagaLauncherReportsSection");
        auto* layout = new QVBoxLayout(group);
        m_reports = new QTreeWidget(group);
        m_reports->setObjectName("SagaLauncherReports");
        m_reports->setHeaderLabels({"Report", "Raw status", "Launcher status", "Verified", "Path"});
        layout->addWidget(m_reports);
        root->addWidget(group);
    }

    void BuildDistributionSection(QVBoxLayout* root, QWidget* parent)
    {
        auto* group = new QGroupBox("Distribution", parent);
        group->setObjectName("SagaLauncherDistributionSection");
        auto* layout = new QVBoxLayout(group);
        m_distribution = new QPlainTextEdit(group);
        m_distribution->setObjectName("SagaLauncherDistributionStatus");
        m_distribution->setReadOnly(true);
        m_distribution->setMaximumBlockCount(100);
        layout->addWidget(m_distribution);
        root->addWidget(group);
    }

    void BuildLimitationsSection(QVBoxLayout* root, QWidget* parent)
    {
        auto* group = new QGroupBox("Limitations / Non-Claims", parent);
        group->setObjectName("SagaLauncherLimitationsSection");
        auto* layout = new QVBoxLayout(group);
        m_limitations = new QListWidget(group);
        m_limitations->setObjectName("SagaLauncherLimitations");
        layout->addWidget(m_limitations);
        root->addWidget(group);
    }

    void BuildDiagnosticsSection(QVBoxLayout* root, QWidget* parent)
    {
        auto* group = new QGroupBox("Diagnostics", parent);
        group->setObjectName("SagaLauncherDiagnosticsSection");
        auto* layout = new QVBoxLayout(group);
        m_diagnostics = new QTreeWidget(group);
        m_diagnostics->setObjectName("SagaLauncherDiagnostics");
        m_diagnostics->setHeaderLabels({"ID", "Severity", "Message", "Path"});
        layout->addWidget(m_diagnostics);
        root->addWidget(group);
    }

    void Render(const SagaLauncherState& state)
    {
        if (state.selectedProject.has_value())
        {
            m_project->setText(QString::fromStdString(
                state.selectedProject->displayName + " [" + state.selectedProject->projectId +
                "]\n" + state.selectedProject->canonicalManifestPath.string()));
        }
        else
            m_project->setText(
                "No project selected. Opening a project does not launch SagaEditor.");

        m_recent->clear();
        for (const auto& recent : state.recentProjects)
        {
            auto* item = new QListWidgetItem(QString::fromStdString(
                                                 recent.displayName + "\n" +
                                                 recent.canonicalManifestPath.string()),
                                             m_recent);
            const auto selectionPath = recent.canonicalManifestPath.filename() ==
                                               "saga.project.json"
                                           ? recent.canonicalRoot
                                           : recent.canonicalManifestPath;
            item->setData(Qt::UserRole, QString::fromStdString(selectionPath.string()));
            item->setData(Qt::UserRole + 1, recent.exists);
            if (!recent.exists)
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }

        m_targets->clear();
        m_limitations->clear();
        for (const auto& target : state.targets)
        {
            const QString diagnostic = target.diagnostics.empty()
                                           ? QString{}
                                           : QString::fromStdString(
                                                 target.diagnostics.front().diagnosticId);
            auto* item = new QTreeWidgetItem(m_targets,
                                             {QString::fromStdString(target.displayName),
                                              QString::fromLatin1(ToString(target.status)),
                                              QString::fromStdString(target.owner),
                                              diagnostic});
            item->setData(0, Qt::UserRole, QString::fromLatin1(ToString(target.targetKind)));
            if (target.availability != SagaLauncherActionAvailability::Available)
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            for (const auto& nonClaim : target.nonClaims)
                m_limitations->addItem(QString::fromStdString(nonClaim));
        }
        for (const auto& limitation : state.distribution.limitations)
            m_limitations->addItem(QString::fromStdString(limitation));

        m_reports->clear();
        for (const auto& report : state.reports)
        {
            new QTreeWidgetItem(m_reports,
                                {QString::fromStdString(report.reportType),
                                 QString::fromStdString(
                                     report.rawStatus.empty() ? "NotConfigured" : report.rawStatus),
                                 QString::fromLatin1(ToString(report.mappedStatus)),
                                 report.verified.has_value() ? (*report.verified ? "true" : "false")
                                                             : "unknown",
                                 DisplayPath(report.path)});
        }

        const auto& distribution = state.distribution;
        m_distribution->setPlainText(QString::fromStdString(
            "status: " + std::string(ToString(distribution.mappedStatus)) +
            "\nverified: " + (distribution.verified ? "true" : "false") +
            "\npackage: " + distribution.packageName + "\nversion: " + distribution.version +
            "\ncommit: " + distribution.gitCommit + "\nplatform: " + distribution.platform +
            "\napplications: " + Join(distribution.includedApplications) +
            "\npublic tools: " + Join(distribution.includedPublicTools) +
            "\nsource: " + distribution.sourcePath.string()));

        m_diagnostics->clear();
        for (const auto& diagnostic : state.diagnostics)
        {
            new QTreeWidgetItem(m_diagnostics,
                                {QString::fromStdString(diagnostic.diagnosticId),
                                 QString::fromLatin1(ToString(diagnostic.severity)),
                                 QString::fromStdString(diagnostic.message),
                                 diagnostic.path.has_value() ? DisplayPath(*diagnostic.path)
                                                             : QString{}});
        }

        const bool running = state.runningActionId.has_value();
        m_progress->setRange(0, running ? 0 : 1);
        if (!running)
            m_progress->setValue(1);
        m_cancel->setEnabled(state.canCancelRunningAction);
        SetActionEnabled(state, SagaLauncherActionId::ValidateProject, m_validate);
        SetActionEnabled(state, SagaLauncherActionId::OpenEditor, m_editor);
        SetActionEnabled(state, SagaLauncherActionId::RuntimeStarterArenaSmoke, m_smoke);
        SetActionEnabled(state, SagaLauncherActionId::RuntimeStarterArenaPlayable, m_playable);
        SetActionEnabled(state, SagaLauncherActionId::FirstPlayableCheck, m_firstPlayable);
        m_window.statusBar()->showMessage(
            running ? "Launcher action running…"
                    : QString("Launcher state revision %1").arg(state.revision));
    }

    static void SetActionEnabled(const SagaLauncherState& state,
                                 SagaLauncherActionId id,
                                 QPushButton* button)
    {
        const auto action = std::find_if(state.actions.begin(),
                                         state.actions.end(),
                                         [id](const auto& value) { return value.id == id; });
        button->setEnabled(action != state.actions.end() && action->canRun &&
                           !state.runningActionId.has_value());
    }

    static std::string Join(const std::vector<std::string>& values)
    {
        std::string result;
        for (const auto& value : values)
        {
            if (!result.empty())
                result += ", ";
            result += value;
        }
        return result;
    }

    SagaLauncherWindow& m_window;
    std::unique_ptr<SagaLauncherController> m_controller;
    QLabel* m_project = nullptr;
    QListWidget* m_recent = nullptr;
    QPushButton* m_validate = nullptr;
    QPushButton* m_editor = nullptr;
    QPushButton* m_smoke = nullptr;
    QPushButton* m_playable = nullptr;
    QPushButton* m_firstPlayable = nullptr;
    QTreeWidget* m_targets = nullptr;
    QTreeWidget* m_reports = nullptr;
    QPlainTextEdit* m_distribution = nullptr;
    QListWidget* m_limitations = nullptr;
    QTreeWidget* m_diagnostics = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_cancel = nullptr;
};

SagaLauncherWindow::SagaLauncherWindow(std::filesystem::path productExecutable,
                                       std::filesystem::path distributionReport,
                                       QWidget* parent)
    : QMainWindow(parent),
      m_impl(new Impl(*this, std::move(productExecutable), std::move(distributionReport)))
{}

SagaLauncherWindow::~SagaLauncherWindow()
{
    delete m_impl;
}

} // namespace SagaProduct
