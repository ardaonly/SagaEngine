// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Panels/CodeBoxPanel.h"

#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Documents/SourceDocumentTransaction.h"
#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"
#include "SagaEditor/Scripting/ScriptProjectionSession.h"
#include "SagaEditor/Scripting/SagaScriptLanguageServiceSession.h"
#include "SagaEditor/VisualBlocks/ControlledBlockEditController.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditorQt/Panels/ReadOnlyBlocksWidget.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QCompleter>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QKeyEvent>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QSyntaxHighlighter>
#include <QStringListModel>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTabWidget>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include <atomic>

namespace SagaEditor
{
namespace
{
class LanguageCodeEditor final : public QPlainTextEdit
{
public:
    explicit LanguageCodeEditor(QWidget* parent = nullptr) : QPlainTextEdit(parent)
    {
        completionModel_ = new QStringListModel(this);
        completer_ = new QCompleter(completionModel_, this);
        completer_->setWidget(this);
        completer_->setCompletionMode(QCompleter::PopupCompletion);
        completer_->setCaseSensitivity(Qt::CaseInsensitive);
        completer_->popup()->setAccessibleName(QStringLiteral("Code Box completion popup"));
        QObject::connect(completer_, qOverload<const QString&>(&QCompleter::activated),
            this, [this](const QString& value)
            {
                auto cursor = textCursor();
                cursor.select(QTextCursor::WordUnderCursor);
                cursor.insertText(value);
                setTextCursor(cursor);
            });
    }
    std::function<void()> completionRequested;
    void ShowCompletions(const QStringList& values)
    {
        completionModel_->setStringList(values);
        if (values.isEmpty()) { completer_->popup()->hide(); return; }
        const auto cursor = textCursor();
        const auto prefixCursor = [&]
        {
            auto copy = cursor;
            copy.select(QTextCursor::WordUnderCursor);
            return copy;
        }();
        completer_->setCompletionPrefix(prefixCursor.selectedText());
        auto rectangle = cursorRect();
        rectangle.setWidth(completer_->popup()->sizeHintForColumn(0) +
            completer_->popup()->verticalScrollBar()->sizeHint().width());
        completer_->complete(rectangle);
    }
protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        const bool explicitRequest = event->key() == Qt::Key_Space &&
            event->modifiers().testFlag(Qt::ControlModifier);
        QPlainTextEdit::keyPressEvent(event);
        if ((explicitRequest || event->text() == QStringLiteral(".")) && completionRequested)
            completionRequested();
    }
private:
    QStringListModel* completionModel_ = nullptr;
    QCompleter* completer_ = nullptr;
};

class CSharpHighlighter final : public QSyntaxHighlighter
{
public:
    explicit CSharpHighlighter(QTextDocument* document) : QSyntaxHighlighter(document)
    {
        keyword_.setForeground(QColor("#569cd6"));
        comment_.setForeground(QColor("#6a9955"));
        string_.setForeground(QColor("#ce9178"));
    }
    void SetSemantic(std::vector<std::tuple<int, int, QString>> spans)
    { semantic_ = std::move(spans); semanticCurrent_ = true; rehighlight(); }
    void ClearSemantic() { semantic_.clear(); semanticCurrent_ = false; rehighlight(); }
protected:
    void highlightBlock(const QString& text) override
    {
        if (semanticCurrent_)
        {
            const auto blockStart = currentBlock().position();
            const auto blockEnd = blockStart + text.size();
            for (const auto& [start, length, kind] : semantic_)
            {
                const auto end = start + length;
                if (end <= blockStart || start >= blockEnd) continue;
                QTextCharFormat format;
                if (kind.contains(QStringLiteral("comment"))) format.setForeground(QColor("#6a9955"));
                else if (kind.contains(QStringLiteral("string"))) format.setForeground(QColor("#ce9178"));
                else if (kind.contains(QStringLiteral("keyword"))) format.setForeground(QColor("#569cd6"));
                else if (kind.contains(QStringLiteral("class")) || kind.contains(QStringLiteral("struct"))) format.setForeground(QColor("#4ec9b0"));
                else if (kind.contains(QStringLiteral("method"))) format.setForeground(QColor("#dcdcaa"));
                else format.setForeground(QColor("#9cdcfe"));
                const auto clippedStart = std::max<qsizetype>(start, blockStart);
                const auto clippedEnd = std::min<qsizetype>(end, blockEnd);
                setFormat(static_cast<int>(clippedStart - blockStart),
                    static_cast<int>(clippedEnd - clippedStart), format);
            }
            return;
        }
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
    std::vector<std::tuple<int, int, QString>> semantic_;
    bool semanticCurrent_ = false;
};

class ScriptSourceMutationPort final : public IScriptSourceMutationPort
{
public:
    [[nodiscard]] bool ReplaceText(CSharpSourceDocument& document,
        UndoRedoStack& journal, std::string text) override
    {
        CSharpDocumentTransaction transaction(document, journal);
        return transaction.ReplaceText(std::move(text));
    }
};
}

struct CodeBoxPanel::Impl
{
    QWidget* widget = nullptr;
    QLabel* identity = nullptr;
    QLabel* state = nullptr;
    QComboBox* documentSelector = nullptr;
    LanguageCodeEditor* editor = nullptr;
    CSharpHighlighter* highlighter = nullptr;
    QListWidget* diagnostics = nullptr;
    QListWidget* languageResults = nullptr;
    QLabel* languageResultTitle = nullptr;
    QPushButton* save = nullptr;
    QPushButton* analyze = nullptr;
    QPushButton* recover = nullptr;
    QPushButton* discardRecovery = nullptr;
    QTabWidget* tabs = nullptr;
    QWidget* scriptingTab = nullptr;
    ReadOnlyBlocksWidget* blocks = nullptr;
    EditorDocumentWorkspace* workspace = nullptr;
    UndoRedoStack* journal = nullptr;
    CommandDispatcher* dispatcher = nullptr;
    ScriptAnalysisCoordinator* coordinator = nullptr;
    ScriptProjectionSession* projectionSession = nullptr;
    SagaScriptLanguageServiceSession* languageSession = nullptr;
    std::unique_ptr<ControlledBlockEditController> controlledController;
    SagaScriptLanguageServiceSession::PublicationSubscription languageSubscription =
        SagaScriptLanguageServiceSession::InvalidPublicationSubscription;
    ScriptProjectionSession::ChangeSubscription projectionSubscription =
        ScriptProjectionSession::InvalidChangeSubscription;
    QFutureWatcher<ScriptAnalysisResult>* analysisWatcher = nullptr;
    QFutureWatcher<ControlledProjectionResult>* projectionWatcher = nullptr;
    QTimer* hoverTimer = nullptr;
    QTimer* serviceStateTimer = nullptr;
    QTimer* projectionTimer = nullptr;
    std::shared_ptr<std::atomic_bool> cancellation;
    EditorDocumentWorkspace::ChangeSubscription workspaceSubscription =
        EditorDocumentWorkspace::InvalidChangeSubscription;
    CSharpSourceDocument* document = nullptr;
    CSharpSourceDocument::ChangeSubscription documentSubscription =
        CSharpSourceDocument::InvalidChangeSubscription;
    bool synchronizing = false;
    bool projectionRefreshPending = false;
    ScriptSourceMutationPort defaultMutationPort;
    IScriptSourceMutationPort* mutationPort = &defaultMutationPort;

    Impl()
    {
        widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        auto* toolbar = new QHBoxLayout();
        identity = new QLabel("No C# source open", widget);
        documentSelector = new QComboBox(widget);
        documentSelector->setAccessibleName(QStringLiteral("C# source document"));
        state = new QLabel(widget);
        save = new QPushButton("Save", widget);
        analyze = new QPushButton("Analyze", widget);
        auto* definition = new QPushButton("Definition", widget);
        auto* references = new QPushButton("References", widget);
        auto* symbols = new QPushButton("Symbols", widget);
        auto* restartService = new QPushButton("Restart service", widget);
        recover = new QPushButton("Recover", widget);
        discardRecovery = new QPushButton("Discard recovery", widget);
        recover->setVisible(false);
        discardRecovery->setVisible(false);
        toolbar->addWidget(identity); toolbar->addWidget(documentSelector, 1); toolbar->addWidget(state);
        toolbar->addWidget(recover); toolbar->addWidget(discardRecovery);
        toolbar->addWidget(save); toolbar->addWidget(analyze);
        toolbar->addWidget(definition); toolbar->addWidget(references);
        toolbar->addWidget(symbols); toolbar->addWidget(restartService);
        layout->addLayout(toolbar);
        tabs = new QTabWidget(widget);
        tabs->setAccessibleName(QStringLiteral("Code Box"));
        scriptingTab = new QWidget(tabs);
        auto* scriptingLayout = new QVBoxLayout(scriptingTab);
        scriptingLayout->setContentsMargins(0, 0, 0, 0);
        editor = new LanguageCodeEditor(scriptingTab);
        editor->setObjectName(QStringLiteral("CodeBoxCSharpEditor"));
        editor->setAccessibleName(QStringLiteral("Code Box C# editor"));
        editor->setUndoRedoEnabled(false);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        highlighter = new CSharpHighlighter(editor->document());
        scriptingLayout->addWidget(editor, 3);
        diagnostics = new QListWidget(scriptingTab);
        diagnostics->setAccessibleName("SagaScript diagnostics");
        scriptingLayout->addWidget(diagnostics, 1);
        languageResultTitle = new QLabel(QStringLiteral("Language results"), scriptingTab);
        languageResultTitle->setAccessibleName(QStringLiteral("Code Box result type"));
        scriptingLayout->addWidget(languageResultTitle);
        languageResults = new QListWidget(scriptingTab);
        languageResults->setAccessibleName(QStringLiteral("Code Box language results"));
        scriptingLayout->addWidget(languageResults, 1);
        blocks = new ReadOnlyBlocksWidget(tabs);
        tabs->addTab(scriptingTab, QStringLiteral("C#"));
        tabs->addTab(blocks, QStringLiteral("Blocks - Controlled C# authoring"));
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
            if (synchronizing || !document || !journal || !mutationPort) return;
            (void)mutationPort->ReplaceText(
                *document, *journal, editor->toPlainText().toStdString());
        });
        editor->completionRequested = [this] { StartLanguageRequest("completion", true); };
        auto* definitionShortcut = new QShortcut(QKeySequence(Qt::Key_F12), widget);
        definitionShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(definitionShortcut, &QShortcut::activated, [this]
        { if (dispatcher) (void)dispatcher->Dispatch("saga.command.script.definition"); });
        auto* referencesShortcut = new QShortcut(
            QKeySequence(Qt::SHIFT | Qt::Key_F12), widget);
        referencesShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(referencesShortcut, &QShortcut::activated, [this]
        { if (dispatcher) (void)dispatcher->Dispatch("saga.command.script.references"); });
        auto* symbolsShortcut = new QShortcut(
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), widget);
        symbolsShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(symbolsShortcut, &QShortcut::activated, [this]
        { if (dispatcher) (void)dispatcher->Dispatch("saga.command.script.symbols"); });
        auto* restartShortcut = new QShortcut(
            QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R), widget);
        restartShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(restartShortcut, &QShortcut::activated, [this]
        { if (dispatcher) (void)dispatcher->Dispatch(
            "saga.command.script.restart_language_service"); });
        hoverTimer = new QTimer(widget);
        hoverTimer->setSingleShot(true);
        hoverTimer->setInterval(500);
        QObject::connect(editor, &QPlainTextEdit::cursorPositionChanged, [this]
        { hoverTimer->start(); });
        QObject::connect(hoverTimer, &QTimer::timeout, [this]
        { StartLanguageRequest("hover", true); });
        serviceStateTimer = new QTimer(widget);
        serviceStateTimer->setInterval(250);
        QObject::connect(serviceStateTimer, &QTimer::timeout, [this]
        {
            if (!languageSession) return;
            switch (languageSession->State())
            {
            case LanguageServiceState::Starting: state->setText(QStringLiteral("Language service starting")); break;
            case LanguageServiceState::Recovering: state->setText(QStringLiteral("Language service recovering")); break;
            case LanguageServiceState::Unavailable: state->setText(QStringLiteral("Language service unavailable")); break;
            default:
                if (projectionRefreshPending) StartControlledProjection();
                break;
            }
        });
        serviceStateTimer->start();
        projectionTimer = new QTimer(widget);
        projectionTimer->setSingleShot(true);
        projectionTimer->setInterval(100);
        QObject::connect(projectionTimer, &QTimer::timeout, [this]
        { StartControlledProjection(); });
        QObject::connect(documentSelector, &QComboBox::currentIndexChanged, [this](int index)
        {
            if (synchronizing || !workspace || index < 0 ||
                static_cast<std::size_t>(index) >= workspace->Scripts().size()) return;
            AttachDocument(workspace->Scripts()[static_cast<std::size_t>(index)].get());
            RefreshText();
        });
        QObject::connect(save, &QPushButton::clicked, [this]
        { if (dispatcher) (void)dispatcher->Dispatch("saga.command.file.save"); });
        QObject::connect(analyze, &QPushButton::clicked, [this]
        { if (dispatcher) (void)dispatcher->Dispatch("saga.command.script.analyze"); });
        QObject::connect(definition, &QPushButton::clicked, [this]
        { if (dispatcher) (void)dispatcher->Dispatch("saga.command.script.definition"); });
        QObject::connect(references, &QPushButton::clicked, [this]
        { if (dispatcher) (void)dispatcher->Dispatch("saga.command.script.references"); });
        QObject::connect(symbols, &QPushButton::clicked, [this]
        { if (dispatcher) (void)dispatcher->Dispatch("saga.command.script.symbols"); });
        QObject::connect(restartService, &QPushButton::clicked, [this]
        { if (dispatcher) (void)dispatcher->Dispatch(
            "saga.command.script.restart_language_service"); });
        QObject::connect(recover, &QPushButton::clicked, [this]
        { if (document) (void)document->RecoverAsDirtyBuffer(); });
        QObject::connect(discardRecovery, &QPushButton::clicked, [this]
        { if (document) (void)document->DiscardRecovery(); });
        analysisWatcher = new QFutureWatcher<ScriptAnalysisResult>(widget);
        projectionWatcher = new QFutureWatcher<ControlledProjectionResult>(widget);
        QObject::connect(projectionWatcher,
            &QFutureWatcher<ControlledProjectionResult>::finished, widget, [this]
        {
            const auto result = projectionWatcher->result();
            if (document && result.documentId == document->Snapshot().documentId &&
                result.documentVersion == document->Snapshot().inMemoryRevision &&
                result.sourceHash == HashDocumentBytes(document->Text()) && projectionSession)
            {
                if (!result.Succeeded())
                    blocks->SetStatus(QString::fromStdString(result.diagnostic));
                else
                    (void)projectionSession->PublishUnsaved(result.documentVersion,
                        result.sourceHash, result.projection);
            }
            if (document && document->Snapshot().dirty &&
                (result.documentId != document->Snapshot().documentId ||
                    result.documentVersion != document->Snapshot().inMemoryRevision))
                projectionRefreshPending = true;
            if (projectionRefreshPending) projectionTimer->start();
        });
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
            ScriptProjectionSnapshot snapshot;
            snapshot.state = result.passed ? ScriptProjectionState::Current :
                result.cancelled ? ScriptProjectionState::Cancelled :
                result.stale ? ScriptProjectionState::Stale : ScriptProjectionState::Failed;
            snapshot.projection = result.projection;
            if (!result.diagnostics.empty())
                snapshot.diagnostic = result.diagnostics.front().code + ": " +
                    result.diagnostics.front().message;
            blocks->PresentSnapshot(snapshot);
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
        blocks->SetControlledEditing(controlledController.get(), nullptr);
    }

    void AttachDocument(CSharpSourceDocument* next)
    {
        DetachDocument(); document = next;
        if (document)
            documentSubscription = document->SubscribeChanges([this] { RefreshText(); });
        blocks->SetControlledEditing(controlledController.get(), document);
    }

    void RefreshControlledController()
    {
        if (projectionWatcher && projectionWatcher->isRunning())
            projectionWatcher->waitForFinished();
        controlledController.reset();
        if (languageSession && journal)
            controlledController = std::make_unique<ControlledBlockEditController>(
                *languageSession, *journal);
        blocks->SetControlledEditing(controlledController.get(), document);
    }

    void RefreshText()
    {
        synchronizing = true;
        if (!document)
        {
            identity->setText("No C# source open"); state->clear(); editor->clear(); editor->setEnabled(false);
            highlighter->ClearSemantic();
            recover->setVisible(false); discardRecovery->setVisible(false);
            ScriptProjectionSnapshot snapshot;
            snapshot.state = ScriptProjectionState::NoDocument;
            blocks->PresentSnapshot(snapshot);
            if (projectionSession) projectionSession->NoDocument();
        }
        else
        {
            identity->setText(QString::fromStdString(document->Snapshot().canonicalSourcePath.filename().string()));
            state->setText(document->Snapshot().dirty ? "Modified" : "Saved");
            const auto recovery = document->Recovery().status;
            const auto canRecover = recovery == DocumentRecoveryStatus::Recoverable &&
                !document->Snapshot().dirty;
            recover->setVisible(canRecover);
            discardRecovery->setVisible(recovery != DocumentRecoveryStatus::None &&
                !document->Snapshot().dirty);
            if (recovery == DocumentRecoveryStatus::StaleBase)
                state->setText(QStringLiteral("Stale recovery requires discard"));
            else if (recovery == DocumentRecoveryStatus::Invalid)
                state->setText(QStringLiteral("Invalid recovery requires discard"));
            const auto revision = document->Snapshot().inMemoryRevision;
            const auto sourceHash = HashDocumentBytes(document->Text());
            if (document->Snapshot().dirty)
            {
                ScriptProjectionSnapshot snapshot;
                snapshot.state = ScriptProjectionState::DirtyNeedsSave;
                blocks->PresentSnapshot(snapshot);
            }
            if (languageSession) languageSession->Refresh();
            if (projectionSession) projectionSession->Dirty(revision, sourceHash);
            if (controlledController)
            {
                projectionRefreshPending = true;
                if (!projectionWatcher->isRunning() && languageSession &&
                    languageSession->State() == LanguageServiceState::Ready)
                    StartControlledProjection();
                else
                    projectionTimer->start();
            }
            highlighter->ClearSemantic();
            const auto cursor = editor->textCursor();
            editor->setPlainText(QString::fromStdString(document->Text()));
            editor->setTextCursor(cursor);
            editor->setEnabled(true);
        }
        synchronizing = false;
    }

    void RefreshWorkspace()
    {
        const auto prior = document ? document->Snapshot().documentId : std::string{};
        DetachDocument(); synchronizing = true; documentSelector->clear();
        int selected = 0;
        if (workspace)
        {
            for (std::size_t index = 0; index < workspace->Scripts().size(); ++index)
            {
                const auto& script = workspace->Scripts()[index];
                documentSelector->addItem(QString::fromStdString(
                    script->Snapshot().canonicalSourcePath.filename().string()));
                if (script->Snapshot().documentId == prior) selected = static_cast<int>(index);
            }
            if (!workspace->Scripts().empty()) AttachDocument(workspace->Scripts()[selected].get());
        }
        documentSelector->setCurrentIndex(selected); synchronizing = false;
        RefreshText();
    }

    void StartControlledProjection()
    {
        if (!controlledController || !document || !projectionSession ||
            projectionWatcher->isRunning() ||
            languageSession->State() != LanguageServiceState::Ready)
            return;
        projectionRefreshPending = false;
        auto* controller = controlledController.get();
        auto source = document->Snapshot();
        auto sourceText = document->Text();
        projectionWatcher->setFuture(QtConcurrent::run(
            [controller, source = std::move(source),
             sourceText = std::move(sourceText)]() mutable
            { return controller->AnalyzeProjection(
                std::move(source), std::move(sourceText)); }));
    }

    void StartLanguageRequest(std::string method, bool needsPosition)
    {
        if (!languageSession || !document || languageSession->State() != LanguageServiceState::Ready) return;
        std::optional<LanguageServicePosition> position;
        if (needsPosition)
        {
            const auto cursor = editor->textCursor();
            position = LanguageServicePosition{static_cast<std::uint32_t>(cursor.blockNumber()),
                static_cast<std::uint32_t>(cursor.positionInBlock())};
        }
        const auto expectedId = document->Snapshot().documentId;
        const auto expectedVersion = document->Snapshot().inMemoryRevision;
        const auto requestedMethod = method;
        auto future = languageSession->Request(std::move(method), *document, position);
        auto* watcher = new QFutureWatcher<LanguageServiceResponse>(widget);
        QObject::connect(watcher, &QFutureWatcher<LanguageServiceResponse>::finished,
            widget, [this, watcher, expectedId, expectedVersion, requestedMethod]
        {
            const auto response = watcher->result(); watcher->deleteLater();
            if (!document || document->Snapshot().documentId != expectedId ||
                document->Snapshot().inMemoryRevision != expectedVersion ||
                response.documentId != expectedId || response.documentVersion != expectedVersion) return;
            languageResults->clear();
            languageResultTitle->setText(QString::fromStdString(
                requestedMethod == "definition" ? "Definition results" :
                requestedMethod == "references" ? "Reference results" :
                requestedMethod == "symbols" ? "Symbol results" :
                requestedMethod == "hover" ? "Hover information" :
                "Completion results"));
            QStringList completionValues;
            for (const auto& item : response.completions)
            {
                languageResults->addItem(QString::fromStdString(item.label));
                completionValues.push_back(QString::fromStdString(item.label));
            }
            for (const auto& location : response.locations)
                languageResults->addItem(QString::fromStdString(location.path.generic_string()) +
                    QStringLiteral(":%1:%2").arg(location.range.start.line + 1).arg(location.range.start.character + 1));
            for (const auto& symbol : response.symbols)
                languageResults->addItem(QString::fromStdString(symbol.kind + " " + symbol.name));
            if (!response.hoverText.empty())
            {
                languageResults->addItem(QString::fromStdString(response.hoverText));
                QToolTip::showText(editor->mapToGlobal(
                    editor->cursorRect().bottomRight()),
                    QString::fromStdString(response.hoverText), editor);
            }
            if (requestedMethod == "completion")
                editor->ShowCompletions(completionValues);
        });
        watcher->setFuture(QtConcurrent::run([future = std::move(future)]() mutable
        { return future.get(); }));
    }

    void PresentLanguagePublication(LanguageServiceResponse publication)
    {
        if (!document || publication.documentId != document->Snapshot().documentId ||
            publication.documentVersion != document->Snapshot().inMemoryRevision) return;
        std::vector<std::tuple<int, int, QString>> spans;
        for (const auto& item : publication.classifications)
        {
            const auto startBlock = editor->document()->findBlockByNumber(static_cast<int>(item.range.start.line));
            const auto endBlock = editor->document()->findBlockByNumber(static_cast<int>(item.range.end.line));
            if (!startBlock.isValid() || !endBlock.isValid()) continue;
            const auto start = startBlock.position() + static_cast<int>(item.range.start.character);
            const auto end = endBlock.position() + static_cast<int>(item.range.end.character);
            if (end >= start) spans.emplace_back(start, end - start, QString::fromStdString(item.kind));
        }
        highlighter->SetSemantic(std::move(spans));
        diagnostics->clear();
        QList<QTextEdit::ExtraSelection> diagnosticSelections;
        for (const auto& item : publication.diagnostics)
        {
            diagnostics->addItem(QString::fromStdString(item.id + ": " + item.message));
            const auto block = editor->document()->findBlockByNumber(
                static_cast<int>(item.range.start.line));
            if (!block.isValid()) continue;
            QTextEdit::ExtraSelection selection;
            selection.cursor = QTextCursor(block);
            selection.cursor.setPosition(block.position() +
                static_cast<int>(item.range.start.character));
            const auto end = item.range.end.line == item.range.start.line ?
                block.position() + static_cast<int>(item.range.end.character) :
                block.position() + block.length() - 1;
            selection.cursor.setPosition(end, QTextCursor::KeepAnchor);
            selection.format.setUnderlineColor(QColor("#f14c4c"));
            selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
            selection.format.setProperty(QTextFormat::FullWidthSelection, false);
            diagnosticSelections.push_back(selection);
        }
        editor->setExtraSelections(diagnosticSelections);
        state->setText(QStringLiteral("Language service ready"));
    }

    void AttachLanguageSession(SagaScriptLanguageServiceSession* session)
    {
        DetachLanguageSession(); languageSession = session;
        if (!languageSession) return;
        QPointer<QWidget> target = widget;
        languageSubscription = languageSession->Subscribe(
            [this, target](LanguageServiceResponse publication)
            {
                if (!target) return;
                QMetaObject::invokeMethod(target, [this, target, publication = std::move(publication)]() mutable
                { if (target) PresentLanguagePublication(std::move(publication)); }, Qt::QueuedConnection);
            });
        RefreshControlledController();
    }

    void DetachLanguageSession()
    {
        if (languageSession && languageSubscription !=
            SagaScriptLanguageServiceSession::InvalidPublicationSubscription)
            languageSession->Unsubscribe(languageSubscription);
        languageSubscription = SagaScriptLanguageServiceSession::InvalidPublicationSubscription;
        languageSession = nullptr;
        RefreshControlledController();
    }

    void DetachWorkspace()
    {
        DetachDocument();
        if (workspace && workspaceSubscription != EditorDocumentWorkspace::InvalidChangeSubscription)
            workspace->UnsubscribeChanges(workspaceSubscription);
        workspaceSubscription = EditorDocumentWorkspace::InvalidChangeSubscription;
        workspace = nullptr;
    }

    void AttachProjectionSession(ScriptProjectionSession* session)
    {
        if (projectionSession && projectionSubscription !=
            ScriptProjectionSession::InvalidChangeSubscription)
            projectionSession->Unsubscribe(projectionSubscription);
        projectionSession = session;
        projectionSubscription = ScriptProjectionSession::InvalidChangeSubscription;
        if (!projectionSession) return;
        QPointer<ReadOnlyBlocksWidget> target = blocks;
        projectionSubscription = projectionSession->Subscribe(
            [this, target](const ScriptProjectionSnapshot& snapshot)
            {
                if (!target) return;
                QMetaObject::invokeMethod(target, [this, target, snapshot]
                {
                    if (!target) return;
                    if (document && snapshot.identity.documentRevision != 0 &&
                        snapshot.identity.documentRevision !=
                            document->Snapshot().inMemoryRevision)
                        return;
                    target->PresentSnapshot(snapshot);
                },
                    Qt::QueuedConnection);
            });
    }

    void DetachProjectionSession()
    {
        if (projectionSession && projectionSubscription !=
            ScriptProjectionSession::InvalidChangeSubscription)
            projectionSession->Unsubscribe(projectionSubscription);
        projectionSubscription = ScriptProjectionSession::InvalidChangeSubscription;
        projectionSession = nullptr;
    }

    void StartAnalysis()
    {
        if (!coordinator || !workspace || analysisWatcher->isRunning()) return;
        diagnostics->clear();
        state->setText(QStringLiteral("Preparing analysis…"));
        ScriptProjectionSnapshot snapshot;
        snapshot.state = ScriptProjectionState::Analyzing;
        blocks->PresentSnapshot(snapshot);
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
        if (projectionWatcher && projectionWatcher->isRunning()) projectionWatcher->waitForFinished();
    }
};

CodeBoxPanel::CodeBoxPanel() : impl_(std::make_unique<Impl>()) {}
CodeBoxPanel::~CodeBoxPanel()
{ impl_->StopAndWait(); impl_->DetachLanguageSession(); impl_->DetachProjectionSession(); impl_->DetachWorkspace(); }
PanelId CodeBoxPanel::GetPanelId() const { return "saga.panel.code_box"; }
std::string CodeBoxPanel::GetTitle() const { return "Code Box"; }
QWidget* CodeBoxPanel::Widget() const noexcept { return impl_->widget; }
void CodeBoxPanel::OnInit() { impl_->RefreshWorkspace(); }
void CodeBoxPanel::OnShutdown()
{ impl_->StopAndWait(); impl_->DetachLanguageSession(); impl_->DetachProjectionSession(); impl_->DetachWorkspace(); }
void CodeBoxPanel::SetDocumentWorkspace(EditorDocumentWorkspace* workspace)
{
    impl_->DetachWorkspace(); impl_->workspace = workspace;
    if (workspace)
        impl_->workspaceSubscription = workspace->SubscribeChanges(
            [impl = impl_.get()] { impl->RefreshWorkspace(); });
    impl_->RefreshWorkspace();
}
void CodeBoxPanel::SetUndoRedoStack(UndoRedoStack* journal) noexcept
{ impl_->journal = journal; impl_->RefreshControlledController(); }
void CodeBoxPanel::SetCommandDispatcher(CommandDispatcher* dispatcher) noexcept { impl_->dispatcher = dispatcher; }
void CodeBoxPanel::SetAnalysisCoordinator(ScriptAnalysisCoordinator* coordinator) noexcept
{ impl_->coordinator = coordinator; }
void CodeBoxPanel::SetProjectionSession(ScriptProjectionSession* session) noexcept
{ impl_->AttachProjectionSession(session); }
void CodeBoxPanel::SetLanguageServiceSession(SagaScriptLanguageServiceSession* session) noexcept
{ impl_->AttachLanguageSession(session); }
void CodeBoxPanel::SetScriptSourceMutationPort(IScriptSourceMutationPort* port) noexcept
{ impl_->mutationPort = port ? port : &impl_->defaultMutationPort; }
void CodeBoxPanel::Analyze() { impl_->StartAnalysis(); }
void CodeBoxPanel::CancelAnalysis() { impl_->Cancel(); }
void CodeBoxPanel::RequestDefinition() { impl_->StartLanguageRequest("definition", true); }
void CodeBoxPanel::RequestReferences() { impl_->StartLanguageRequest("references", true); }
void CodeBoxPanel::RequestSymbols() { impl_->StartLanguageRequest("symbols", false); }

} // namespace SagaEditor
