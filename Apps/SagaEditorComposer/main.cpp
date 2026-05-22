/// @file main.cpp
/// @brief SagaEditorComposer visual editor-composition authoring application.

#include "SagaEditor/Composer/ComposerArtifactSummary.h"
#include "SagaEditor/Composer/ComposerBuildPlan.h"
#include "SagaEditor/Composer/ComposerDiagnosticsModel.h"
#include "SagaEditor/Composer/ComposerEditCommand.h"
#include "SagaEditor/Composer/ComposerSourceIndex.h"
#include "SagaEditor/Composer/ComposerWorkspace.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace
{

constexpr int kExitBadArguments = 2;

/// Parsed command-line options for SagaEditorComposer.
struct ComposerCliOptions
{
    std::filesystem::path workspace = std::filesystem::current_path();
    bool help = false;
    bool smoke = false;
    bool show = true;
    int smokeTimeoutMs = 1000;
};

[[nodiscard]] bool HasValue(int index, int argc) noexcept
{
    return index + 1 < argc;
}

[[nodiscard]] bool ParseInt(const char* text, int& out) noexcept
{
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || value <= 0)
    {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

void PrintHelp()
{
    std::cout
        << "Usage:\n"
        << "  SagaEditorComposer --workspace <repo-or-project-root>\n"
        << "  SagaEditorComposer --smoke --workspace <path> --no-show "
           "--smoke-timeout-ms <n>\n"
        << "  SagaEditorComposer --help\n";
}

[[nodiscard]] bool ParseCli(int argc,
                            char* argv[],
                            ComposerCliOptions& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--help")
        {
            options.help = true;
        }
        else if (arg == "--workspace")
        {
            if (!HasValue(i, argc))
            {
                std::cerr << "SagaEditorComposer: --workspace requires a path\n";
                return false;
            }
            options.workspace = argv[++i];
        }
        else if (arg == "--smoke")
        {
            options.smoke = true;
            options.show = false;
        }
        else if (arg == "--no-show")
        {
            options.show = false;
        }
        else if (arg == "--smoke-timeout-ms")
        {
            if (!HasValue(i, argc) ||
                !ParseInt(argv[++i], options.smokeTimeoutMs))
            {
                std::cerr << "SagaEditorComposer: --smoke-timeout-ms requires "
                          << "a positive integer\n";
                return false;
            }
        }
        else
        {
            std::cerr << "SagaEditorComposer: unknown argument '" << arg
                      << "'\n";
            return false;
        }
    }
    return true;
}

[[nodiscard]] QString ToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.string());
}

[[nodiscard]] QString ToQString(const std::string& text)
{
    return QString::fromStdString(text);
}

[[nodiscard]] std::string ItemId(QTreeWidgetItem* item)
{
    if (item == nullptr)
    {
        return {};
    }
    return item->data(0, Qt::UserRole).toString().toStdString();
}

/// Main Composer window. Qt owns child widgets through QObject parentage.
class ComposerMainWindow final : public QMainWindow
{
public:
    explicit ComposerMainWindow(std::filesystem::path workspace,
                                QWidget* parent = nullptr)
        : QMainWindow(parent)
        , m_paths(SagaEditor::ResolveComposerWorkspacePaths(
              std::move(workspace)))
    {
        setWindowTitle(QStringLiteral("SagaEditorComposer"));
        resize(1500, 900);
        BuildUi();
        ReloadWorkspace();
    }

private:
    void BuildUi()
    {
        auto* toolbar = new QToolBar(QStringLiteral("Composer"), this);
        addToolBar(toolbar);

        QAction* openAction = toolbar->addAction(QStringLiteral("Open Workspace"));
        QAction* reloadAction = toolbar->addAction(QStringLiteral("Reload"));
        QAction* saveAction = toolbar->addAction(QStringLiteral("Save"));
        QAction* buildAction =
            toolbar->addAction(QStringLiteral("Build Composition"));
        QAction* commandAction =
            toolbar->addAction(QStringLiteral("Show Command"));
        QAction* revertAction = toolbar->addAction(QStringLiteral("Revert"));

        auto* root = new QSplitter(Qt::Horizontal, this);
        setCentralWidget(root);

        m_tree = new QTreeWidget(root);
        m_tree->setHeaderLabels({QStringLiteral("Composition Source")});
        m_tree->setMinimumWidth(320);

        auto* center = new QWidget(root);
        auto* centerLayout = new QVBoxLayout(center);
        m_selectionLabel = new QLabel(center);
        m_selectionLabel->setWordWrap(true);
        centerLayout->addWidget(m_selectionLabel);

        m_fieldTree = new QTreeWidget(center);
        m_fieldTree->setHeaderLabels({
            QStringLiteral("Field"),
            QStringLiteral("Value"),
        });
        m_fieldTree->setRootIsDecorated(false);
        m_fieldTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_fieldTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        centerLayout->addWidget(m_fieldTree);

        auto* editBox = new QWidget(center);
        auto* editLayout = new QVBoxLayout(editBox);
        m_displayNameEdit = new QLineEdit(editBox);
        m_displayNameButton =
            new QPushButton(QStringLiteral("Queue displayName Edit"), editBox);
        m_defaultVisibleCheck =
            new QCheckBox(QStringLiteral("defaultVisible"), editBox);
        m_defaultVisibleButton =
            new QPushButton(QStringLiteral("Queue defaultVisible Edit"), editBox);
        editLayout->addWidget(m_displayNameEdit);
        editLayout->addWidget(m_displayNameButton);
        editLayout->addWidget(m_defaultVisibleCheck);
        editLayout->addWidget(m_defaultVisibleButton);
        centerLayout->addWidget(editBox);

        auto* right = new QSplitter(Qt::Vertical, root);
        m_diffTree = new QTreeWidget(right);
        m_diffTree->setHeaderLabels({
            QStringLiteral("File"),
            QStringLiteral("Item"),
            QStringLiteral("Field"),
            QStringLiteral("Old"),
            QStringLiteral("New"),
        });
        m_diffTree->setRootIsDecorated(false);

        m_diagnosticsTree = new QTreeWidget(right);
        m_diagnosticsTree->setHeaderLabels({
            QStringLiteral("Severity"),
            QStringLiteral("Code"),
            QStringLiteral("Message"),
            QStringLiteral("Source"),
        });
        m_diagnosticsTree->setRootIsDecorated(false);

        m_artifactSummary = new QLabel(right);
        m_artifactSummary->setWordWrap(true);
        m_buildOutput = new QPlainTextEdit(right);
        m_buildOutput->setReadOnly(true);
        root->setStretchFactor(1, 2);
        root->setStretchFactor(2, 2);

        connect(openAction, &QAction::triggered, this, [this]()
        {
            const QString path = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("Open SagaEditor Composition Workspace"),
                ToQString(m_paths.root));
            if (!path.isEmpty())
            {
                m_paths = SagaEditor::ResolveComposerWorkspacePaths(
                    path.toStdString());
                ReloadWorkspace();
            }
        });
        connect(reloadAction, &QAction::triggered, this, [this]()
        {
            ReloadWorkspace();
        });
        connect(saveAction, &QAction::triggered, this, [this]()
        {
            SavePendingEdits();
        });
        connect(buildAction, &QAction::triggered, this, [this]()
        {
            RunBuild();
        });
        connect(commandAction, &QAction::triggered, this, [this]()
        {
            ShowCommandPlan();
        });
        connect(revertAction, &QAction::triggered, this, [this]()
        {
            m_edits.Revert();
            RefreshDiff();
            statusBar()->showMessage(QStringLiteral("Pending edits reverted."));
        });
        connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]()
        {
            RefreshInspector();
        });
        connect(m_displayNameButton, &QPushButton::clicked, this, [this]()
        {
            QueueFieldEdit("displayName",
                           m_displayNameEdit->text().toStdString());
        });
        connect(m_defaultVisibleButton, &QPushButton::clicked, this, [this]()
        {
            QueueFieldEdit("defaultVisible",
                           m_defaultVisibleCheck->isChecked()
                               ? std::string{"true"}
                               : std::string{"false"});
        });
    }

    void ReloadWorkspace()
    {
        m_index = SagaEditor::BuildComposerSourceIndex(m_paths);
        m_edits.Reset(m_index);
        RefreshTree();
        RefreshInspector();
        RefreshDiff();
        RefreshGeneratedOutputs();
        statusBar()->showMessage(
            QStringLiteral("Workspace loaded: %1").arg(ToQString(m_paths.root)));
    }

    void RefreshTree()
    {
        m_tree->clear();
        std::map<SagaEditor::ComposerSourceKind, QTreeWidgetItem*> groups;
        for (const SagaEditor::ComposerSourceItem& item : m_index.items)
        {
            QTreeWidgetItem*& group = groups[item.kind];
            if (group == nullptr)
            {
                group = new QTreeWidgetItem(m_tree);
                group->setText(0, ToQString(
                    SagaEditor::ComposerSourceKindLabel(item.kind)));
            }
            auto* child = new QTreeWidgetItem(group);
            child->setText(0, QStringLiteral("%1\n%2")
                                  .arg(ToQString(item.id), ToQString(item.model)));
            child->setData(0, Qt::UserRole, ToQString(item.id));
        }
        m_tree->expandAll();
    }

    void RefreshInspector()
    {
        m_fieldTree->clear();
        m_displayNameEdit->clear();
        m_displayNameButton->setEnabled(false);
        m_defaultVisibleCheck->setEnabled(false);
        m_defaultVisibleButton->setEnabled(false);

        const std::string id = ItemId(m_tree->currentItem());
        const SagaEditor::ComposerSourceItem* item =
            SagaEditor::FindComposerSourceItem(m_index, id);
        if (item == nullptr)
        {
            m_selectionLabel->setText(
                QStringLiteral("Select a composition source item."));
            return;
        }

        m_selectionLabel->setText(QStringLiteral("%1\n%2:%3")
                                      .arg(ToQString(item->id),
                                           ToQString(item->file),
                                           QString::number(item->line)));
        for (const SagaEditor::ComposerSourceField& field : item->fields)
        {
            auto* row = new QTreeWidgetItem(m_fieldTree);
            row->setText(0, ToQString(field.name));
            row->setText(1, ToQString(field.value));
        }

        if (const auto* field =
                SagaEditor::FindComposerSourceField(*item, "displayName"))
        {
            m_displayNameEdit->setText(ToQString(field->value));
            m_displayNameButton->setEnabled(true);
        }
        if (const auto* field =
                SagaEditor::FindComposerSourceField(*item, "defaultVisible"))
        {
            m_defaultVisibleCheck->setChecked(field->value == "true");
            m_defaultVisibleCheck->setEnabled(true);
            m_defaultVisibleButton->setEnabled(true);
        }
    }

    void QueueFieldEdit(const std::string& fieldName,
                        const std::string& value)
    {
        const std::string id = ItemId(m_tree->currentItem());
        const SagaEditor::ComposerEditResult result =
            m_edits.SetField(id, fieldName, value);
        RefreshDiff();
        statusBar()->showMessage(ToQString(result.code + ": " + result.message));
    }

    void RefreshDiff()
    {
        m_diffTree->clear();
        for (const SagaEditor::ComposerPendingEdit& edit :
             m_edits.PendingEdits())
        {
            auto* row = new QTreeWidgetItem(m_diffTree);
            row->setText(0, ToQString(edit.file));
            row->setText(1, ToQString(edit.itemId));
            row->setText(2, ToQString(edit.fieldName));
            row->setText(3, ToQString(edit.oldValue));
            row->setText(4, ToQString(edit.newValue));
        }
    }

    void SavePendingEdits()
    {
        const SagaEditor::ComposerEditResult result = m_edits.Save(m_paths);
        if (result.ok)
        {
            m_index = SagaEditor::BuildComposerSourceIndex(m_paths);
            m_edits.Reset(m_index);
            RefreshTree();
            RefreshInspector();
        }
        RefreshDiff();
        statusBar()->showMessage(ToQString(result.code + ": " + result.message));
    }

    void RefreshGeneratedOutputs()
    {
        const SagaEditor::ComposerArtifactSummary summary =
            SagaEditor::LoadComposerArtifactSummary(m_paths);
        m_artifactSummary->setText(QStringLiteral(
            "Artifact Summary\n"
            "Manifest: %1\nArtifact: %2\nDiagnostics: %3\nSource Map: %4\n"
            "Dependencies: %5\nComposition: %6\nHash: %7\n"
            "Panels: %8 Actions: %9 Menus: %10 Toolbars: %11 "
            "Shortcuts: %12 Workspaces: %13 Modes: %14\n%15")
            .arg(summary.manifestFound ? QStringLiteral("found") : QStringLiteral("missing"),
                 summary.artifactFound ? QStringLiteral("found") : QStringLiteral("missing"),
                 summary.diagnosticsFound ? QStringLiteral("found") : QStringLiteral("missing"),
                 summary.sourceMapFound ? QStringLiteral("found") : QStringLiteral("missing"),
                 summary.dependenciesFound ? QStringLiteral("found") : QStringLiteral("missing"),
                 ToQString(summary.compositionId),
                 ToQString(summary.artifactHash),
                 QString::number(summary.panelCount),
                 QString::number(summary.actionCount),
                 QString::number(summary.menuCount),
                 QString::number(summary.toolbarCount),
                 QString::number(summary.shortcutCount),
                 QString::number(summary.workspaceCount),
                 QString::number(summary.modeCount),
                 ToQString(summary.message)));

        const SagaEditor::ComposerDiagnosticsModel diagnostics =
            SagaEditor::LoadComposerDiagnostics(m_paths);
        m_diagnosticsTree->clear();
        for (const SagaEditor::ComposerDiagnosticRow& row : diagnostics.rows)
        {
            auto* item = new QTreeWidgetItem(m_diagnosticsTree);
            item->setText(0, ToQString(row.severity));
            item->setText(1, ToQString(row.code));
            item->setText(2, ToQString(row.message));
            item->setText(3, QStringLiteral("%1:%2:%3")
                                  .arg(ToQString(row.file),
                                       QString::number(row.line),
                                       QString::number(row.column)));
        }
        if (diagnostics.rows.empty())
        {
            auto* item = new QTreeWidgetItem(m_diagnosticsTree);
            item->setText(0, diagnostics.reportFound
                                 ? QStringLiteral("Info")
                                 : QStringLiteral("Warning"));
            item->setText(1, QStringLiteral("Composer.Diagnostics"));
            item->setText(2, ToQString(diagnostics.message));
        }
    }

    void ShowCommandPlan()
    {
        const SagaEditor::ComposerBuildPlan plan =
            SagaEditor::MakeComposerBuildPlan(
                m_paths,
                QApplication::applicationFilePath().toStdString());
        m_buildOutput->setPlainText(ToQString(plan.displayCommand));
        statusBar()->showMessage(QStringLiteral("Build command planned."));
    }

    void RunBuild()
    {
        const SagaEditor::ComposerBuildPlan plan =
            SagaEditor::MakeComposerBuildPlan(
                m_paths,
                QApplication::applicationFilePath().toStdString());

        QStringList args;
        for (const std::string& argument : plan.arguments)
        {
            args << ToQString(argument);
        }

        QProcess process;
        process.setWorkingDirectory(ToQString(m_paths.root));
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start(ToQString(plan.executable), args);
        if (!process.waitForStarted(5000))
        {
            m_buildOutput->setPlainText(
                QStringLiteral("Failed to start: %1\n%2")
                    .arg(ToQString(plan.executable),
                         process.errorString()));
            return;
        }
        process.waitForFinished(-1);
        const QString output =
            QString::fromUtf8(process.readAllStandardOutput()) +
            QString::fromUtf8(process.readAllStandardError());
        m_buildOutput->setPlainText(QStringLiteral("exitCode=%1\n%2")
                                        .arg(process.exitCode())
                                        .arg(output));
        RefreshGeneratedOutputs();
        statusBar()->showMessage(QStringLiteral("Build Composition finished."));
    }

    SagaEditor::ComposerWorkspacePaths m_paths;
    SagaEditor::ComposerSourceIndex m_index;
    SagaEditor::ComposerEditSession m_edits;
    QTreeWidget* m_tree = nullptr;
    QTreeWidget* m_fieldTree = nullptr;
    QTreeWidget* m_diffTree = nullptr;
    QTreeWidget* m_diagnosticsTree = nullptr;
    QLabel* m_selectionLabel = nullptr;
    QLabel* m_artifactSummary = nullptr;
    QPlainTextEdit* m_buildOutput = nullptr;
    QLineEdit* m_displayNameEdit = nullptr;
    QCheckBox* m_defaultVisibleCheck = nullptr;
    QPushButton* m_displayNameButton = nullptr;
    QPushButton* m_defaultVisibleButton = nullptr;
};

} // namespace

int main(int argc, char* argv[])
{
    ComposerCliOptions options;
    if (!ParseCli(argc, argv, options))
    {
        return kExitBadArguments;
    }
    if (options.help)
    {
        PrintHelp();
        return 0;
    }

    QApplication app(argc, argv);
    ComposerMainWindow window(options.workspace);
    if (options.show)
    {
        window.show();
    }
    if (options.smoke)
    {
        QTimer::singleShot(options.smokeTimeoutMs, &app, &QApplication::quit);
    }
    return app.exec();
}
