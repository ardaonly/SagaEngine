// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Panels/PackagesPanel.h"

#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Packages/PackageAuthoring.h"
#include "SagaEditor/Packages/PackageWorkflowController.h"
#include "SagaEditor/Packages/PackagesPanelViewModel.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace SagaEditor
{
namespace
{
QString OperationName(PackageModuleOperation operation)
{
    return operation == PackageModuleOperation::Enable
        ? QStringLiteral("Enable") : QStringLiteral("Disable");
}

QString Lines(const std::vector<std::string>& values)
{
    QString result;
    for (const auto& value : values)
        result += QStringLiteral("• ") + QString::fromStdString(value) + '\n';
    return result.isEmpty() ? QStringLiteral("None\n") : result;
}

class PackageReviewDialog final : public QDialog
{
public:
    PackageReviewDialog(const PackageTransactionReview& review, bool canApply,
                        QWidget* parent) : QDialog(parent)
    {
        setObjectName(QStringLiteral("PackageReviewDialog"));
        setAccessibleName(QStringLiteral("Package transaction review"));
        setWindowTitle(QStringLiteral("Review Package Changes"));
        resize(720, 640);
        auto* layout = new QVBoxLayout(this);
        auto* title = new QLabel(
            QStringLiteral("%1 %2 / %3 (%4)")
                .arg(OperationName(review.operation),
                     QString::fromStdString(review.packageId),
                     QString::fromStdString(review.module),
                     QString::fromStdString(review.version)), this);
        title->setAccessibleName(QStringLiteral("Package review identity"));
        layout->addWidget(title);
        auto* details = new QTextEdit(this);
        details->setAccessibleName(QStringLiteral("Package review details"));
        details->setReadOnly(true);
        details->setPlainText(
            QStringLiteral("Intent\n%1 → %2\n\nGraph\n%3 → %4\n\n"
                           "Module changes\n%5\nSurfaces\n%6\nArtifacts\n%7\n"
                           "Capabilities\n%8\nLicense\n%9\nTrust / provenance\n%10\n"
                           "Offline\n%11\nUsage impact\n%12\nRefresh plan\n%13")
                .arg(QString::fromStdString(review.baseIntentHash),
                     QString::fromStdString(review.candidateIntentHash),
                     QString::fromStdString(review.baseGraphHash),
                     QString::fromStdString(review.candidateGraphHash),
                     Lines(review.moduleChanges), Lines(review.surfaceChanges),
                     Lines(review.artifactChanges), Lines(review.capabilities),
                     QString::fromStdString(review.license),
                     QString::fromStdString(review.trust),
                     review.offline ? QStringLiteral("Available offline")
                                    : QStringLiteral("Not available offline"),
                     Lines(review.usageImpact), Lines(review.refreshPlan)));
        layout->addWidget(details, 1);
        buttons_ = new QDialogButtonBox(
            QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
        buttons_->button(QDialogButtonBox::Apply)->setAccessibleName(
            QStringLiteral("Apply package transaction"));
        buttons_->button(QDialogButtonBox::Apply)->setEnabled(canApply &&
            !review.blocked && !review.stale);
        connect(buttons_->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &QDialog::accept);
        connect(buttons_->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
            this, &QDialog::reject);
        layout->addWidget(buttons_);
    }

private:
    QDialogButtonBox* buttons_ = nullptr;
};

class PackageRecoveryDialog final : public QDialog
{
public:
    PackageRecoveryDialog(const std::vector<PackageRecoveryInspection>& recovery,
                          QWidget* parent) : QDialog(parent)
    {
        setObjectName(QStringLiteral("PackageRecoveryDialog"));
        setAccessibleName(QStringLiteral("Package transaction recovery"));
        setWindowTitle(QStringLiteral("Inspect Package Recovery"));
        resize(760, 480);
        auto* layout = new QVBoxLayout(this);
        tree_ = new QTreeWidget(this);
        tree_->setAccessibleName(QStringLiteral("Package recovery journals"));
        tree_->setHeaderLabels({"Journal", "Transaction", "Phase", "Marker",
            "File", "Old hash", "New hash"});
        for (const auto& item : recovery)
        {
            if (item.files.empty())
                new QTreeWidgetItem(tree_, {QString::fromStdString(
                    item.journalPath.generic_string()),
                    QString::fromStdString(item.transactionId),
                    QString::fromStdString(item.phase),
                    item.commitMarker ? QStringLiteral("Committed")
                                      : QStringLiteral("Unmarked")});
            for (const auto& file : item.files)
                new QTreeWidgetItem(tree_, {QString::fromStdString(
                    item.journalPath.generic_string()),
                    QString::fromStdString(item.transactionId),
                    QString::fromStdString(item.phase),
                    item.commitMarker ? QStringLiteral("Committed")
                                      : QStringLiteral("Unmarked"),
                    QString::fromStdString(file.relativeTarget.generic_string()),
                    QString::fromStdString(file.oldHash),
                    QString::fromStdString(file.newHash)});
        }
        layout->addWidget(tree_, 1);
        buttons_ = new QDialogButtonBox(QDialogButtonBox::Retry |
            QDialogButtonBox::Close, this);
        buttons_->button(QDialogButtonBox::Retry)->setAccessibleName(
            QStringLiteral("Retry safe package recovery"));
        buttons_->button(QDialogButtonBox::Retry)->setEnabled(!recovery.empty());
        connect(buttons_->button(QDialogButtonBox::Retry), &QPushButton::clicked,
            this, &QDialog::accept);
        connect(buttons_->button(QDialogButtonBox::Close), &QPushButton::clicked,
            this, &QDialog::reject);
        layout->addWidget(buttons_);
    }

private:
    QTreeWidget* tree_ = nullptr;
    QDialogButtonBox* buttons_ = nullptr;
};
}

struct PackagesPanel::Impl
{
    QWidget* widget = nullptr;
    QLabel* status = nullptr;
    QTreeWidget* rows = nullptr;
    QPushButton* enable = nullptr;
    QPushButton* disable = nullptr;
    QPushButton* retry = nullptr;
    QPushButton* recover = nullptr;
    QPushButton* undo = nullptr;
    QPushButton* redo = nullptr;
    PackageStateCatalog* catalog = nullptr;
    ProjectReadinessSession* readiness = nullptr;
    PackageModuleWorkflow* workflow = nullptr;
    PackageWorkflowController* controller = nullptr;
    PackageWorkflowController::ChangeSubscription subscription =
        PackageWorkflowController::InvalidChangeSubscription;
    CommandDispatcher* dispatcher = nullptr;
    PackagesPanelViewModel model;
    QFutureWatcher<PackageWorkflowResult>* watcher = nullptr;
    QPointer<PackageReviewDialog> reviewDialog;
    QPointer<PackageRecoveryDialog> recoveryDialog;

    Impl()
    {
        widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        status = new QLabel(QStringLiteral("Package state unavailable"), widget);
        status->setAccessibleName(QStringLiteral("Package workflow status"));
        layout->addWidget(status);
        rows = new QTreeWidget(widget);
        rows->setAccessibleName(QStringLiteral("Packages"));
        rows->setHeaderLabels({"Package", "Version", "Module", "State",
            "Runtime", "Capabilities", "License", "Trust"});
        layout->addWidget(rows);
        auto* actions = new QHBoxLayout();
        enable = new QPushButton("Enable module…", widget);
        disable = new QPushButton("Disable module…", widget);
        retry = new QPushButton("Retry Package Refresh", widget);
        recover = new QPushButton("Recover Transaction…", widget);
        undo = new QPushButton("Package Undo…", widget);
        redo = new QPushButton("Package Redo…", widget);
        for (auto* action : {enable, disable, retry, recover, undo, redo})
            actions->addWidget(action);
        layout->addLayout(actions);
        const auto dispatch = [this](const char* id)
        { if (dispatcher) (void)dispatcher->Dispatch(id); };
        QObject::connect(enable, &QPushButton::clicked,
            [dispatch] { dispatch("saga.command.package.enable_module"); });
        QObject::connect(disable, &QPushButton::clicked,
            [dispatch] { dispatch("saga.command.package.disable_module"); });
        QObject::connect(retry, &QPushButton::clicked,
            [dispatch] { dispatch("saga.command.package.retry_refresh"); });
        QObject::connect(recover, &QPushButton::clicked,
            [dispatch] { dispatch("saga.command.package.recover_transaction"); });
        QObject::connect(undo, &QPushButton::clicked,
            [dispatch] { dispatch("saga.command.package.undo"); });
        QObject::connect(redo, &QPushButton::clicked,
            [dispatch] { dispatch("saga.command.package.redo"); });
        QObject::connect(rows, &QTreeWidget::itemSelectionChanged, widget,
            [this]
            {
                const auto selected = rows->selectedItems();
                const auto index = selected.empty() ? -1
                    : rows->indexOfTopLevelItem(selected.front());
                if (controller && index >= 0 &&
                    static_cast<std::size_t>(index) < model.Rows().size())
                {
                    const auto& row = model.Rows()[static_cast<std::size_t>(index)];
                    controller->Select({row.packageId, row.module});
                }
                SyncActions();
            });
        watcher = new QFutureWatcher<PackageWorkflowResult>(widget);
        QObject::connect(watcher, &QFutureWatcher<PackageWorkflowResult>::finished,
            widget, [this]
            {
                const auto result = watcher->result();
                Refresh();
                if (result.status == PackageWorkflowStatus::Prepared &&
                    result.review && controller)
                    ShowReview(*result.review);
            });
    }

    ~Impl()
    {
        DetachController();
        StopAndWait();
    }

    void Start(std::function<PackageWorkflowResult()> operation)
    {
        if (!controller || watcher->isRunning()) return;
        SyncActions();
        watcher->setFuture(QtConcurrent::run(std::move(operation)));
    }

    void ShowReview(const PackageTransactionReview& review)
    {
        if (reviewDialog) reviewDialog->close();
        const auto snapshot = controller->Snapshot();
        auto* dialog = new PackageReviewDialog(review, snapshot.canApply, widget);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        reviewDialog = dialog;
        QObject::connect(dialog, &QDialog::accepted, widget, [this]
        { Start([controller = controller] { return controller->Apply(); }); });
        QObject::connect(dialog, &QDialog::rejected, widget, [this]
        { Start([controller = controller] { return controller->Cancel(); }); });
        dialog->show();
    }

    void ShowRecovery()
    {
        if (!controller) return;
        const auto recovery = controller->InspectRecovery();
        if (recoveryDialog) recoveryDialog->close();
        auto* dialog = new PackageRecoveryDialog(recovery, widget);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        recoveryDialog = dialog;
        if (!recovery.empty())
        {
            const auto journal = recovery.front().journalPath;
            QObject::connect(dialog, &QDialog::accepted, widget,
                [this, journal]
                { Start([controller = controller, journal]
                    { return controller->RetryRecovery(journal); }); });
        }
        dialog->show();
    }

    void SyncActions()
    {
        const auto selected = rows->selectedItems();
        const auto index = selected.empty() ? -1
            : rows->indexOfTopLevelItem(selected.front());
        const auto snapshot = controller ? controller->Snapshot()
            : PackageWorkflowControllerSnapshot{};
        const bool idle = !watcher->isRunning() && !snapshot.active &&
            snapshot.state == PackageWorkflowControllerState::Ready;
        enable->setEnabled(idle && index >= 0 &&
            model.Rows()[static_cast<std::size_t>(index)].canEnable);
        disable->setEnabled(idle && index >= 0 &&
            model.Rows()[static_cast<std::size_t>(index)].canDisable);
        retry->setEnabled(!watcher->isRunning() && snapshot.canRetry);
        recover->setEnabled(!watcher->isRunning() &&
            snapshot.state == PackageWorkflowControllerState::RecoveryRequired);
        undo->setEnabled(!watcher->isRunning() && snapshot.canUndo);
        redo->setEnabled(!watcher->isRunning() && snapshot.canRedo);
        rows->setEnabled(idle);
        status->setText(snapshot.progress.empty()
            ? QString::fromStdString(snapshot.message.empty()
                ? "Ready" : snapshot.message)
            : QString::fromStdString(snapshot.progress));
    }

    void Refresh()
    {
        const auto selected = controller ? controller->Snapshot().selection
                                         : PackageWorkflowSelection{};
        rows->clear();
        if (!catalog || !readiness) { SyncActions(); return; }
        model.Refresh(*catalog, *readiness);
        for (const auto& row : model.Rows())
        {
            auto* item = new QTreeWidgetItem({
                QString::fromStdString(row.packageId),
                QString::fromStdString(row.version),
                QString::fromStdString(row.module),
                QString::fromStdString(row.state),
                QString::fromStdString(row.runtimeReadiness),
                QString::fromStdString(row.capabilities),
                QString::fromStdString(row.license),
                QString::fromStdString(row.trust)});
            rows->addTopLevelItem(item);
            if (row.packageId == selected.packageId && row.module == selected.module)
                item->setSelected(true);
        }
        SyncActions();
    }

    void AttachController(PackageWorkflowController* next)
    {
        DetachController();
        controller = next;
        if (!controller) return;
        QPointer<QWidget> target = widget;
        subscription = controller->Subscribe([this, target](const auto&)
        {
            if (!target) return;
            QMetaObject::invokeMethod(target, [this, target]
            { if (target) Refresh(); }, Qt::QueuedConnection);
        });
    }

    void DetachController()
    {
        if (controller && subscription !=
            PackageWorkflowController::InvalidChangeSubscription)
            controller->Unsubscribe(subscription);
        subscription = PackageWorkflowController::InvalidChangeSubscription;
        controller = nullptr;
    }

    void StopAndWait()
    {
        if (!watcher || !watcher->isRunning()) return;
        if (controller) controller->RequestCancellation();
        watcher->waitForFinished();
    }
};

PackagesPanel::PackagesPanel() : impl_(std::make_unique<Impl>()) {}
PackagesPanel::~PackagesPanel() = default;
PanelId PackagesPanel::GetPanelId() const { return "saga.panel.packages"; }
std::string PackagesPanel::GetTitle() const { return "Packages"; }
QWidget* PackagesPanel::Widget() const noexcept { return impl_->widget; }
void PackagesPanel::OnInit() { Refresh(); }
void PackagesPanel::OnShutdown() { impl_->StopAndWait(); }
void PackagesPanel::SetCatalog(PackageStateCatalog* catalog) noexcept
{ impl_->catalog = catalog; }
void PackagesPanel::SetReadiness(ProjectReadinessSession* readiness) noexcept
{ impl_->readiness = readiness; }
void PackagesPanel::SetWorkflow(PackageModuleWorkflow* workflow) noexcept
{ impl_->workflow = workflow; }
void PackagesPanel::SetController(PackageWorkflowController* controller) noexcept
{ impl_->AttachController(controller); }
void PackagesPanel::SetCommandDispatcher(CommandDispatcher* dispatcher) noexcept
{ impl_->dispatcher = dispatcher; }
void PackagesPanel::OpenOrRefresh()
{ impl_->Start([controller = impl_->controller]
    { return controller->OpenOrRefresh(); }); }
void PackagesPanel::EnableSelected()
{ impl_->Start([controller = impl_->controller]
    { return controller->PrepareSelected(PackageModuleOperation::Enable); }); }
void PackagesPanel::DisableSelected()
{ impl_->Start([controller = impl_->controller]
    { return controller->PrepareSelected(PackageModuleOperation::Disable); }); }
void PackagesPanel::RetryRefresh()
{ impl_->Start([controller = impl_->controller]
    { return controller->RetryRefresh(); }); }
void PackagesPanel::InspectRecovery() { impl_->ShowRecovery(); }
void PackagesPanel::Undo()
{ impl_->Start([controller = impl_->controller]
    { return controller->PrepareUndo(); }); }
void PackagesPanel::Redo()
{ impl_->Start([controller = impl_->controller]
    { return controller->PrepareRedo(); }); }
void PackagesPanel::Refresh() { impl_->Refresh(); }
}
