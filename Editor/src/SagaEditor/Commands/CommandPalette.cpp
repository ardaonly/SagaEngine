/// @file CommandPalette.cpp
/// @brief Searchable command palette dialog — Ctrl+Shift+P to open.

#include "SagaEditor/Commands/CommandPalette.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/CommandDispatcher.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

CommandPalette::CommandPalette(CommandRegistry&   registry,
                               CommandDispatcher& dispatcher,
                               QWidget*           parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_registry(registry)
    , m_dispatcher(dispatcher)
{
    BuildLayout();
    setMinimumWidth(480);
    setMaximumHeight(400);
}

// ─── Open ─────────────────────────────────────────────────────────────────────

void CommandPalette::Open()
{
    m_searchBox->clear();
    RebuildList(QString{});

    // Center horizontally near the top of the parent window.
    if (parentWidget())
    {
        const QRect pw = parentWidget()->geometry();
        move(pw.left() + (pw.width() - width()) / 2,
             pw.top() + 80);
    }

    show();
    raise();
    m_searchBox->setFocus();
}

// ─── Event Handling ───────────────────────────────────────────────────────────

bool CommandPalette::eventFilter(QObject* obj, QEvent* event)
{
    return QDialog::eventFilter(obj, event);
}

void CommandPalette::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        hide();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        InvokeSelected();
        return;
    }
    if (event->key() == Qt::Key_Down)
    {
        const int next = m_resultList->currentRow() + 1;
        if (next < m_resultList->count())
            m_resultList->setCurrentRow(next);
        return;
    }
    if (event->key() == Qt::Key_Up)
    {
        const int prev = m_resultList->currentRow() - 1;
        if (prev >= 0)
            m_resultList->setCurrentRow(prev);
        return;
    }
    QDialog::keyPressEvent(event);
}

// ─── Slots ────────────────────────────────────────────────────────────────────

void CommandPalette::OnSearchTextChanged(const QString& text)
{
    RebuildList(text);
}

void CommandPalette::OnItemActivated(QListWidgetItem* item)
{
    if (!item)
        return;

    const QString commandId = item->data(Qt::UserRole).toString();
    hide();
    m_dispatcher.Dispatch(commandId.toStdString());
}

// ─── Internals ────────────────────────────────────────────────────────────────

void CommandPalette::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Type a command..."));
    m_searchBox->setFrame(false);
    layout->addWidget(m_searchBox);

    m_resultList = new QListWidget(this);
    m_resultList->setFrameShape(QFrame::NoFrame);
    m_resultList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_resultList);

    connect(m_searchBox, &QLineEdit::textChanged,
            this,        &CommandPalette::OnSearchTextChanged);
    connect(m_resultList, &QListWidget::itemActivated,
            this,         &CommandPalette::OnItemActivated);
}

void CommandPalette::RebuildList(const QString& filter)
{
    m_resultList->clear();

    const auto commands = m_registry.GetAll();
    for (const auto* desc : commands)
    {
        if (!desc->enabled)
            continue;

        const QString label    = QString::fromStdString(desc->label);
        const QString category = QString::fromStdString(desc->category);

        if (!filter.isEmpty() &&
            !label.contains(filter, Qt::CaseInsensitive) &&
            !category.contains(filter, Qt::CaseInsensitive))
            continue;

        auto* item = new QListWidgetItem(
            QString("%1  —  %2").arg(label, category));
        item->setData(Qt::UserRole, QString::fromStdString(desc->id));
        m_resultList->addItem(item);
    }

    if (m_resultList->count() > 0)
        m_resultList->setCurrentRow(0);
}

void CommandPalette::InvokeSelected()
{
    auto* item = m_resultList->currentItem();
    if (!item)
        return;

    const QString commandId = item->data(Qt::UserRole).toString();
    hide();
    m_dispatcher.Dispatch(commandId.toStdString());
}

} // namespace SagaEditor
