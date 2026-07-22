// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Panels/ScriptingPanel.h"

#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Documents/SourceDocumentTransaction.h"
#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditorQt/Panels/ReadOnlyBlocksWidget.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <atomic>

namespace SagaEditor
{
namespace
{
class CSharpHighlighter final : public QSyntaxHighlighter
{
public:
    explicit CSharpHighlighter(QTextDocument* document) : QSyntaxHighlighter(document)
    {
        keyword_.setForeground(QColor("#569cd6"));
        comment_.setForeground(QColor("#6a9955"));
        string_.setForeground(QColor("#ce9178"));
    }
protected:
    void highlightBlock(const QString& text) override
    {
        static const QRegularExpression keywords(
            QStringLiteral("\\b(class|namespace|public|private|protected|void|bool|float|double|int|string|return|if|else|new|using|sealed|override)\\b"));
        auto matches = keywords.globalMatch(text);
        while (matches.hasNext())
        {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), keyword_);
        }
        const auto comment = text.indexOf(QStringLiteral("//"));
        if (comment >= 0) setFormat(comment, text.size() - comment, comment_);
        static const QRegularExpression strings(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""));
        matches = strings.globalMatch(text);
        while (matches.hasNext())
        {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), string_);
        }
    }
private:
    QTextCharFormat keyword_;
    QTextCharFormat comment_;
    QTextCharFormat string_;
};
}

struct ScriptingPanel::Impl
{
    QWidget* widget = nullptr;
    QLabel* identity = nullptr;
    QLabel* state = nullptr;
    QPlainTextEdit* editor = nullptr;
    QListWidget* diagnostics = nullptr;
    QPushButton* save = nullptr;
    QPushButton* analyze = nullptr;
    QTabWidget* tabs = nullptr;
    QWidget* scriptingTab = nullptr;
    ReadOnlyBlocksWidget* blocks = nullptr;
    EditorDocumentWorkspace* workspace = nullptr;
    UndoRedoStack* journal = nullptr;
    CommandDispatcher* dispatcher = nullptr;
    ScriptAnalysisCoordinator* coordinator = nullptr;
    QFutureWatcher<ScriptAnalysisResult>* analysisWatcher = nullptr;
    std::shared_ptr<std::atomic_bool> cancellation;
    EditorDocumentWorkspace::ChangeSubscription workspaceSubscription =
        EditorDocumentWorkspace::InvalidChangeSubscription;
    CSharpSourceDocument* document = nullptr;
    CSharpSourceDocument::ChangeSubscription documentSubscription =
        CSharpSourceDocument::InvalidChangeSubscription;
    bool synchronizing = false;

    Impl()
    {
        widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        auto* toolbar = new QHBoxLayout();
        identity = new QLabel("No C# source open", widget);
        state = new QLabel(widget);
        save = new QPushButton("Save", widget);
        analyze = new QPushButton("Analyze", widget);
        toolbar->addWidget(identity, 1); toolbar->addWidget(state);
        toolbar->addWidget(save); toolbar->addWidget(analyze);
        layout->addLayout(toolbar);
        tabs = new QTabWidget(widget);
        tabs->setAccessibleName(QStringLiteral("Scripting and Blocks"));
        scriptingTab = new QWidget(tabs);
        auto* scriptingLayout = new QVBoxLayout(scriptingTab);
        scriptingLayout->setContentsMargins(0, 0, 0, 0);
        editor = new QPlainTextEdit(scriptingTab);
        editor->setUndoRedoEnabled(false);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        new CSharpHighlighter(editor->document());
        scriptingLayout->addWidget(editor, 3);
        diagnostics = new QListWidget(scriptingTab);
        diagnostics->setAccessibleName("SagaScript diagnostics");
        scriptingLayout->addWidget(diagnostics, 1);
        blocks = new ReadOnlyBlocksWidget(tabs);
        tabs->addTab(scriptingTab, QStringLiteral("Scripting"));
        tabs->addTab(blocks, QStringLiteral("Blocks"));
        layout->addWidget(tabs, 1);
        blocks->SetSourceActivated([this](const VisualBlocks::ProjectionSourceSpan& source)
        {
            if (!document || source.sourcePath.empty()) return;
            const auto current = document->Snapshot().canonicalSourcePath.generic_string();
            if (!current.ends_with(source.sourcePath)) return;
            const auto text = document->Text();
            const auto startByte = std::min<std::size_t>(source.startByte, text.size());
            const auto endByte = std::min<std::size_t>(source.endByte, text.size());
            QTextCursor cursor(editor->document());
            cursor.setPosition(QString::fromUtf8(text.data(), static_cast<qsizetype>(startByte)).size());
            cursor.setPosition(QString::fromUtf8(text.data(), static_cast<qsizetype>(endByte)).size(),
                QTextCursor::KeepAnchor);
            editor->setTextCursor(cursor);
            tabs->setCurrentWidget(scriptingTab);
            editor->setFocus();
        });
        QObject::connect(editor, &QPlainTextEdit::textChanged, [this]
        {
            if (synchronizing || !document || !journal) return;
            CSharpDocumentTransaction transaction(*document, *journal);
            (void)transaction.ReplaceText(editor->toPlainText().toStdString());
        });
        QObject::connect(save, &QPushButton::clicked, [this]
        { if (dispatcher) (void)dispatcher->Dispatch("saga.command.file.save"); });
        QObject::connect(analyze, &QPushButton::clicked, [this]
        { if (dispatcher) (void)dispatcher->Dispatch("saga.command.script.analyze"); });
        analysisWatcher = new QFutureWatcher<ScriptAnalysisResult>(widget);
        QObject::connect(analysisWatcher, &QFutureWatcher<ScriptAnalysisResult>::finished,
            widget, [this]
        {
            const auto result = analysisWatcher->result();
            diagnostics->clear();
            for (const auto& diagnostic : result.diagnostics)
            {
                QString text = QString::fromStdString(diagnostic.code + ": " + diagnostic.message);
                if (!diagnostic.path.empty())
                    text += QStringLiteral(" [") +
                        QString::fromStdString(diagnostic.path.generic_string()) + QStringLiteral("]");
                diagnostics->addItem(text);
            }
            state->setText(result.passed ? QStringLiteral("Analysis passed") :
                result.cancelled ? QStringLiteral("Analysis cancelled") :
                result.stale ? QStringLiteral("Stale result rejected") :
                QStringLiteral("Analysis failed"));
            blocks->Present(result.passed ? result.projection : nullptr);
            if (!result.passed)
                blocks->SetStatus(result.cancelled ? QStringLiteral("Analysis cancelled") :
                    result.stale ? QStringLiteral("Stale projection rejected") :
                    QStringLiteral("Projection failed"));
            analyze->setEnabled(document != nullptr);
            cancellation.reset();
        });
    }

    void DetachDocument()
    {
        if (document && documentSubscription != CSharpSourceDocument::InvalidChangeSubscription)
            document->UnsubscribeChanges(documentSubscription);
        documentSubscription = CSharpSourceDocument::InvalidChangeSubscription;
        document = nullptr;
    }

    void RefreshText()
    {
        synchronizing = true;
        if (!document)
        {
            identity->setText("No C# source open"); state->clear(); editor->clear(); editor->setEnabled(false);
            blocks->Present(nullptr);
        }
        else
        {
            identity->setText(QString::fromStdString(document->Snapshot().canonicalSourcePath.filename().string()));
            state->setText(document->Snapshot().dirty ? "Modified" : "Saved");
            if (document->Snapshot().dirty)
            {
                blocks->Present(nullptr);
                blocks->SetStatus(QStringLiteral("Save changes before analysis"));
            }
            const auto cursor = editor->textCursor();
            editor->setPlainText(QString::fromStdString(document->Text()));
            editor->setTextCursor(cursor);
            editor->setEnabled(true);
        }
        synchronizing = false;
    }

    void RefreshWorkspace()
    {
        DetachDocument();
        if (workspace && !workspace->Scripts().empty())
        {
            document = workspace->Scripts().front().get();
            documentSubscription = document->SubscribeChanges([this] { RefreshText(); });
        }
        RefreshText();
    }

    void DetachWorkspace()
    {
        DetachDocument();
        if (workspace && workspaceSubscription != EditorDocumentWorkspace::InvalidChangeSubscription)
            workspace->UnsubscribeChanges(workspaceSubscription);
        workspaceSubscription = EditorDocumentWorkspace::InvalidChangeSubscription;
        workspace = nullptr;
    }

    void StartAnalysis()
    {
        if (!coordinator || !workspace || analysisWatcher->isRunning()) return;
        diagnostics->clear();
        state->setText(QStringLiteral("Preparing analysis…"));
        blocks->SetStatus(QStringLiteral("Analyzing…"));
        analyze->setEnabled(false);
        cancellation = std::make_shared<std::atomic_bool>(false);
        auto cancelled = cancellation;
        auto* analysisCoordinator = coordinator;
        auto* documentWorkspace = workspace;
        QPointer<QLabel> status = state;
        ScriptAnalysisRequest analysisRequest;
        analysisRequest.cancellationRequested = [cancelled]
        { return cancelled->load(std::memory_order_relaxed); };
        analysisRequest.progress = [status](std::string_view stage)
        {
            const auto message = QString::fromStdString(std::string(stage));
            if (!status) return;
            QMetaObject::invokeMethod(status, [status, message]
            {
                if (status) status->setText(message);
            }, Qt::QueuedConnection);
        };
        analysisWatcher->setFuture(QtConcurrent::run(
            [analysisCoordinator, documentWorkspace, analysisRequest]
            { return analysisCoordinator->Analyze(*documentWorkspace, analysisRequest); }));
    }

    void Cancel()
    {
        if (cancellation) cancellation->store(true, std::memory_order_relaxed);
    }

    void StopAndWait()
    {
        Cancel();
        if (analysisWatcher && analysisWatcher->isRunning()) analysisWatcher->waitForFinished();
    }
};

ScriptingPanel::ScriptingPanel() : impl_(std::make_unique<Impl>()) {}
ScriptingPanel::~ScriptingPanel() { impl_->StopAndWait(); impl_->DetachWorkspace(); }
PanelId ScriptingPanel::GetPanelId() const { return "saga.panel.scripting"; }
std::string ScriptingPanel::GetTitle() const { return "Scripting"; }
void* ScriptingPanel::GetNativeWidget() const noexcept { return impl_->widget; }
void ScriptingPanel::OnInit() { impl_->RefreshWorkspace(); }
void ScriptingPanel::OnShutdown() { impl_->StopAndWait(); impl_->DetachWorkspace(); }
void ScriptingPanel::SetDocumentWorkspace(EditorDocumentWorkspace* workspace)
{
    impl_->DetachWorkspace(); impl_->workspace = workspace;
    if (workspace)
        impl_->workspaceSubscription = workspace->SubscribeChanges(
            [impl = impl_.get()] { impl->RefreshWorkspace(); });
    impl_->RefreshWorkspace();
}
void ScriptingPanel::SetUndoRedoStack(UndoRedoStack* journal) noexcept { impl_->journal = journal; }
void ScriptingPanel::SetCommandDispatcher(CommandDispatcher* dispatcher) noexcept { impl_->dispatcher = dispatcher; }
void ScriptingPanel::SetAnalysisCoordinator(ScriptAnalysisCoordinator* coordinator) noexcept
{ impl_->coordinator = coordinator; }
void ScriptingPanel::Analyze() { impl_->StartAnalysis(); }
void ScriptingPanel::CancelAnalysis() { impl_->Cancel(); }

} // namespace SagaEditor
