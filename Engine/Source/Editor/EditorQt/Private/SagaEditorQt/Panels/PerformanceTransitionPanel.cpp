// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Panels/PerformanceTransitionPanel.h"

#include "SagaEditor/Performance/BuildProfiles.h"
#include "SagaEditor/Performance/CollectibleConversionController.h"
#include "SagaEditor/Standalone/StandaloneBuildCoordinator.h"
#include "SagaEditor/Play/PlaySourceSnapshot.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFutureWatcher>
#include <QtConcurrent>

#include <algorithm>
#include <atomic>
#include <chrono>

namespace SagaEditor
{
struct PerformanceTransitionPanel::Impl
{
    QWidget* widget = new QWidget();
    QComboBox* profiles = nullptr;
    QLineEdit* standaloneOutput = nullptr;
    QLabel* status = nullptr;
    QPlainTextEdit* output = nullptr;
    EditorDocumentWorkspace* workspace = nullptr;
    BuildProfilesDocument document;
    CollectibleConversionController conversionController;
    std::filesystem::path runtimeExecutable;
    QFutureWatcher<StandaloneBuildResult>* buildWatcher = nullptr;
    std::shared_ptr<std::atomic_bool> cancelBuild = std::make_shared<std::atomic_bool>();

    Impl()
    {
        widget->setObjectName(QStringLiteral("PerformanceTransitionPanel"));
        widget->setAccessibleName(QStringLiteral("Gameplay performance transition"));
        auto* layout = new QVBoxLayout(widget);
        auto* instrumentation = new QGroupBox(QStringLiteral("Instrumentation"), widget);
        auto* instrumentationLayout = new QVBoxLayout(instrumentation);
        instrumentationLayout->addWidget(new QLabel(
            QStringLiteral("Lifecycle transitions and steady-clock update time are supported. "
                           "Allocation and memory counters remain unavailable."), instrumentation));
        layout->addWidget(instrumentation);
        profiles = new QComboBox(widget);
        profiles->setObjectName(QStringLiteral("BenchmarkProfileSelection"));
        profiles->setAccessibleName(QStringLiteral("Benchmark profile"));
        layout->addWidget(profiles);
        auto* refresh = new QPushButton(QStringLiteral("Validate Profiles"), widget);
        refresh->setObjectName(QStringLiteral("ValidateBuildProfilesButton"));
        auto* run = new QPushButton(QStringLiteral("Create Verified Run Input"), widget);
        run->setObjectName(QStringLiteral("CreateBenchmarkRunInputButton"));
        auto* results = new QPushButton(QStringLiteral("Load Raw Results"), widget);
        results->setObjectName(QStringLiteral("LoadBenchmarkResultsButton"));
        auto* conversion = new QPushButton(QStringLiteral("Review High to Low Conversion"), widget);
        conversion->setObjectName(QStringLiteral("ReviewCollectibleConversionButton"));
        auto* compare = new QPushButton(QStringLiteral("Compare Supported Sources"), widget);
        compare->setObjectName(QStringLiteral("CompareCollectibleConversionButton"));
        auto* reverse = new QPushButton(QStringLiteral("Reverse Applied Conversion"), widget);
        reverse->setObjectName(QStringLiteral("ReverseCollectibleConversionButton"));
        auto* standalone = new QPushButton(QStringLiteral("Build Standalone"), widget);
        standalone->setObjectName(QStringLiteral("BuildStandaloneButton"));
        auto* cancel = new QPushButton(QStringLiteral("Cancel Standalone Build"), widget);
        cancel->setObjectName(QStringLiteral("CancelStandaloneBuildButton"));
        standaloneOutput = new QLineEdit(widget);
        standaloneOutput->setObjectName(QStringLiteral("StandaloneOutputDirectory"));
        standaloneOutput->setPlaceholderText(
            QStringLiteral("Build/Standalone/<profile>"));
        layout->addWidget(refresh);
        layout->addWidget(run);
        layout->addWidget(results);
        layout->addWidget(conversion);
        layout->addWidget(compare);
        layout->addWidget(reverse);
        layout->addWidget(standaloneOutput);
        layout->addWidget(standalone);
        layout->addWidget(cancel);
        status = new QLabel(QStringLiteral("Open a project to inspect performance profiles."), widget);
        status->setObjectName(QStringLiteral("PerformanceTransitionStatus"));
        status->setAccessibleName(QStringLiteral("Performance transition status"));
        layout->addWidget(status);
        output = new QPlainTextEdit(widget);
        output->setObjectName(QStringLiteral("PerformanceRawResults"));
        output->setAccessibleName(QStringLiteral("Benchmark raw results"));
        output->setReadOnly(true);
        layout->addWidget(output, 1);
        QObject::connect(refresh, &QPushButton::clicked, widget, [this] { Refresh(); });
        QObject::connect(run, &QPushButton::clicked, widget, [this] { RunInput(); });
        QObject::connect(results, &QPushButton::clicked, widget, [this] { Results(); });
        QObject::connect(conversion, &QPushButton::clicked, widget, [this] { Conversion(); });
        QObject::connect(compare, &QPushButton::clicked, widget, [this] { Compare(); });
        QObject::connect(reverse, &QPushButton::clicked, widget, [this] { Reverse(); });
        QObject::connect(standalone, &QPushButton::clicked, widget, [this] { Standalone(); });
        QObject::connect(cancel, &QPushButton::clicked, widget,
            [this] { cancelBuild->store(true); status->setText(QStringLiteral("Cancelling standalone build...")); });
        buildWatcher = new QFutureWatcher<StandaloneBuildResult>(widget);
        QObject::connect(buildWatcher, &QFutureWatcher<StandaloneBuildResult>::finished,
            widget, [this]
            {
                const auto result = buildWatcher->result();
                if (result.Succeeded())
                {
                    status->setText(QStringLiteral("Standalone published: %1")
                        .arg(QString::fromStdString(result.outputDirectory.string())));
                    output->setPlainText(QString::fromStdString(
                        result.snapshot.canonicalJson));
                }
                else status->setText(QString::fromStdString(result.diagnostics.empty()
                    ? (result.state == StandaloneBuildState::Cancelled
                        ? "Standalone build cancelled." : "Standalone build failed.")
                    : result.diagnostics.front().code));
            });
    }

    std::filesystem::path Root() const
    {
        return workspace && workspace->Project()
            ? std::filesystem::path(workspace->Project()->rootPath) : std::filesystem::path{};
    }

    void Refresh()
    {
        profiles->clear();
        const auto root = Root();
        if (root.empty())
        {
            status->setText(QStringLiteral("No project is open."));
            return;
        }
        const auto loaded = BuildProfiles::Load(root / "BuildProfiles.saga.json", root);
        if (!loaded.succeeded)
        {
            document = {};
            status->setText(QStringLiteral("Profile validation failed: %1")
                .arg(loaded.diagnostics.empty() ? QStringLiteral("unknown error")
                    : QString::fromStdString(loaded.diagnostics.front().code)));
            return;
        }
        document = loaded.document;
        for (const auto& profile : document.profiles)
            if (profile.kind == BuildProfileKind::Benchmark)
                profiles->addItem(QString::fromStdString(profile.id));
        status->setText(QStringLiteral("Validated %1 profiles (%2).")
            .arg(document.profiles.size()).arg(QString::fromStdString(document.contentHash)));
    }

    void RunInput()
    {
        if (document.profiles.empty()) Refresh();
        const auto [input, diagnostics] = BuildProfiles::CreateRunInput(
            document, profiles->currentText().toStdString());
        if (!diagnostics.empty())
        {
            status->setText(QString::fromStdString(diagnostics.front().code));
            return;
        }
        output->setPlainText(QString::fromStdString(input.canonicalJson));
        status->setText(QStringLiteral("Verified runtime input created without changing project files."));
    }

    void Results()
    {
        if (document.profiles.empty()) Refresh();
        const auto selected = profiles->currentText().toStdString();
        const auto iterator = std::find_if(document.profiles.begin(), document.profiles.end(),
            [&selected](const auto& profile) { return profile.id == selected; });
        if (iterator == document.profiles.end()) return;
        QFile file(QString::fromStdString((Root() / iterator->evidencePath).string()));
        if (!file.open(QIODevice::ReadOnly))
        {
            status->setText(QStringLiteral("No raw result exists for the selected profile."));
            return;
        }
        output->setPlainText(QString::fromUtf8(file.readAll()));
        status->setText(QStringLiteral("Raw benchmark result loaded."));
    }

    void Conversion()
    {
        QFile provenance(QString::fromStdString(
            (Root() / "CollectibleConversion.saga.json").string()));
        if (!provenance.open(QIODevice::ReadOnly))
        {
            status->setText(QStringLiteral("Conversion provenance is unavailable."));
            return;
        }
        output->setPlainText(QString::fromUtf8(provenance.readAll()));
        status->setText(QStringLiteral(
            "Review requires verified package, generation, source, reverse patch, and provenance identities."));
    }

    void Compare()
    {
        QFile high(QString::fromStdString(
            (Root() / "Scripts/ArenaRushCollectibleStore.cs").string()));
        QFile low(QString::fromStdString(
            (Root() / "Conversions/ArenaRushCollectibleStore.lowlevel.cs.txt").string()));
        if (!high.open(QIODevice::ReadOnly) || !low.open(QIODevice::ReadOnly))
        {
            status->setText(QStringLiteral("Supported conversion sources are unavailable."));
            return;
        }
        output->setPlainText(QStringLiteral("HIGH-LEVEL SOURCE\n%1\nLOW-LEVEL CANDIDATE\n%2")
            .arg(QString::fromUtf8(high.readAll()), QString::fromUtf8(low.readAll())));
        status->setText(QStringLiteral("Supported high-level and low-level sources compared."));
    }

    void Reverse()
    {
        const auto root = Root();
        QFile provenance(QString::fromStdString(
            (root / "CollectibleConversion.saga.json").string()));
        if (!provenance.open(QIODevice::ReadOnly) ||
            !provenance.readAll().contains("\"state\": \"Applied\""))
        {
            status->setText(QStringLiteral("No applied conversion is available to reverse."));
            return;
        }
        CollectibleConversionCapture capture;
        capture.projectRoot = root;
        capture.projectFile = workspace && workspace->Project()
            ? std::filesystem::path(workspace->Project()->manifestPath) : std::filesystem::path{};
        capture.lockFile = root / "SagaPackage.lock";
        capture.sourceFile = root / "Scripts/ArenaRushCollectibleStore.cs";
        capture.provenanceFile = root / "CollectibleConversion.saga.json";
        const auto result = conversionController.Reverse(capture);
        if (result.state == CollectibleConversionState::Reversed)
            status->setText(QStringLiteral("Applied conversion was reversed exactly."));
        else if (result.state == CollectibleConversionState::DetachedRequiresReview)
            status->setText(QStringLiteral("Detached / Requires Review: %1")
                .arg(QString::fromStdString(result.code)));
        else status->setText(QString::fromStdString(result.code));
    }

    void Standalone()
    {
        if (buildWatcher->isRunning()) return;
        const auto root = Root();
        if (root.empty() || runtimeExecutable.empty())
        {
            status->setText(QStringLiteral("Open a project with a private SagaRuntime before building."));
            return;
        }
        if (document.profiles.empty()) Refresh();
        const auto [verified, diagnostics] = BuildProfiles::CreateRunInput(
            document, profiles->currentText().toStdString());
        if (!diagnostics.empty())
        {
            status->setText(QString::fromStdString(diagnostics.front().code));
            return;
        }
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto captured = CapturePlaySourceSnapshot(*workspace,
            "standalone-editor-" + std::to_string(ticks));
        if (!captured.Succeeded())
        {
            status->setText(QString::fromStdString(captured.diagnostics.empty()
                ? "Build.Snapshot.Failed" : captured.diagnostics.front().code));
            return;
        }
        cancelBuild = std::make_shared<std::atomic_bool>();
        StandaloneBuildRequest request;
        request.source = *captured.snapshot;
        request.buildProfilesPath = root / "BuildProfiles.saga.json";
        request.profileId = verified.profileId;
        request.profileHash = verified.profileHash;
        request.runtimeIdentifier = "host";
        request.runtimeExecutable = runtimeExecutable;
        const auto selectedOutput = std::filesystem::path(
            standaloneOutput->text().toStdString());
        request.outputDirectory = selectedOutput.empty()
            ? root / "Build/Standalone" / verified.profileId
            : (selectedOutput.is_absolute() ? selectedOutput : root / selectedOutput);
        request.toolchain = DiscoverScriptAnalysisToolchain();
        request.cancellationRequested = [flag = cancelBuild] { return flag->load(); };
        status->setText(QStringLiteral("Capturing immutable standalone snapshot..."));
        buildWatcher->setFuture(QtConcurrent::run([request = std::move(request)]
        { return StandaloneBuildCoordinator{}.Build(request); }));
    }
};

PerformanceTransitionPanel::PerformanceTransitionPanel() : impl_(std::make_unique<Impl>()) {}
PerformanceTransitionPanel::~PerformanceTransitionPanel() = default;
PanelId PerformanceTransitionPanel::GetPanelId() const { return "saga.panel.performance_transition"; }
std::string PerformanceTransitionPanel::GetTitle() const { return "Gameplay Performance"; }
QWidget* PerformanceTransitionPanel::Widget() const noexcept { return impl_->widget; }
void PerformanceTransitionPanel::OnInit() { RefreshProfiles(); }
void PerformanceTransitionPanel::SetWorkspace(EditorDocumentWorkspace* workspace) noexcept
{ impl_->workspace = workspace; }
void PerformanceTransitionPanel::SetRuntimeExecutable(std::filesystem::path runtimeExecutable)
{ impl_->runtimeExecutable = std::move(runtimeExecutable); }
void PerformanceTransitionPanel::RefreshProfiles() { impl_->Refresh(); }
void PerformanceTransitionPanel::CreateVerifiedRunInput() { impl_->RunInput(); }
void PerformanceTransitionPanel::LoadResults() { impl_->Results(); }
void PerformanceTransitionPanel::ReviewConversionState() { impl_->Conversion(); }
void PerformanceTransitionPanel::BuildStandalone() { impl_->Standalone(); }
void PerformanceTransitionPanel::CancelStandaloneBuild()
{ impl_->cancelBuild->store(true); }
}
