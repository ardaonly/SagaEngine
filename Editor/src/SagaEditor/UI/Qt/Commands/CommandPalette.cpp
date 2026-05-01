/// @file CommandPalette.cpp
/// @brief Qt backend implementation. The public header lives outside
///        UI/Qt/ (Commands/CommandPalette.h);
///        Qt headers are permitted only inside this folder. If you
///        need to use Qt for something new, add a sibling here.

#include "SagaEditor/Commands/CommandPalette.h"

#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/CommandRegistry.h"

#include <QApplication>
#include <QDialog>
#include <QFrame>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QScreen>
#include <QString>
#include <QVBoxLayout>

namespace SagaEditor
{

// ─── Pimpl: Qt-side dialog ────────────────────────────────────────────────────

class CommandPalette::Impl : public QDialog
{
public:
    Impl(CommandRegistry& registry, CommandDispatcher& dispatcher)
        : QDialog(nullptr, Qt::Popup | Qt::FramelessWindowHint)
        , m_registry(registry)
        , m_dispatcher(dispatcher)
    {
        BuildLayout();
        setMinimumWidth(480);
        setMaximumHeight(400);
    }

    void OpenPalette()
    {
        m_searchBox->clear();
        RebuildList(QString{});

        // Centre near the top of the active screen since this dialog has
        // no real "parent window" — the popup is detached on purpose so it
        // floats over whatever the editor was doing.
        if (auto* screen = QApplication::primaryScreen())
        {
            const QRect geom = screen->availableGeometry();
            move(geom.left() + (geom.width() - width()) / 2,
                 geom.top()  + 80);
        }

        show();
        raise();
        m_searchBox->setFocus();
    }

    void HidePalette()
    {
        hide();
    }

    [[nodiscard]] bool IsPaletteVisible() const
    {
        return isVisible();
    }

protected:
    // ─── Event Handling ───────────────────────────────────────────────────────

    void keyPressEvent(QKeyEvent* event) override
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
            {
                m_resultList->setCurrentRow(next);
            }
            return;
        }
        if (event->key() == Qt::Key_Up)
        {
            const int prev = m_resultList->currentRow() - 1;
            if (prev >= 0)
            {
                m_resultList->setCurrentRow(prev);
            }
            return;
        }
        QDialog::keyPressEvent(event);
    }

private:
    // ─── Layout & Wiring ──────────────────────────────────────────────────────

    void BuildLayout()
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
                this, [this](const QString& text) { RebuildList(text); });
        connect(m_resultList, &QListWidget::itemActivated,
                this, [this](QListWidgetItem* item) { OnItemActivated(item); });
    }

    void RebuildList(const QString& filter)
    {
        m_resultList->clear();

        const auto commands = m_registry.GetAll();
        for (const auto* desc : commands)
        {
            if (desc == nullptr || !desc->enabled)
            {
                continue;
            }

            const QString label    = QString::fromStdString(desc->label);
            const QString category = QString::fromStdString(desc->category);

            if (!filter.isEmpty() &&
                !label.contains(filter, Qt::CaseInsensitive) &&
                !category.contains(filter, Qt::CaseInsensitive))
            {
                continue;
            }

            auto* item = new QListWidgetItem(
                QString("%1  —  %2").arg(label, category));
            item->setData(Qt::UserRole, QString::fromStdString(desc->id));
            m_resultList->addItem(item);
        }

        if (m_resultList->count() > 0)
        {
            m_resultList->setCurrentRow(0);
        }
    }

    void OnItemActivated(QListWidgetItem* item)
    {
        if (item == nullptr)
        {
            return;
        }
        const QString commandId = item->data(Qt::UserRole).toString();
        hide();
        (void)m_dispatcher.Dispatch(commandId.toStdString());
    }

    void InvokeSelected()
    {
        auto* item = m_resultList->currentItem();
        if (item == nullptr)
        {
            return;
        }
        const QString commandId = item->data(Qt::UserRole).toString();
        hide();
        (void)m_dispatcher.Dispatch(commandId.toStdString());
    }

    CommandRegistry&   m_registry;
    CommandDispatcher& m_dispatcher;
    QLineEdit*         m_searchBox  = nullptr; ///< Owned by Qt (set as child).
    QListWidget*       m_resultList = nullptr; ///< Owned by Qt (set as child).
};

// ─── Outer Wrapper ────────────────────────────────────────────────────────────

CommandPalette::CommandPalette(CommandRegistry&   registry,
                               CommandDispatcher& dispatcher)
    : m_impl(std::make_unique<Impl>(registry, dispatcher))
{}

CommandPalette::~CommandPalette() = default;

void CommandPalette::Show()  { m_impl->OpenPalette(); }
void CommandPalette::Hide()  { m_impl->HidePalette(); }
bool CommandPalette::IsVisible() const { return m_impl->IsPaletteVisible(); }

} // namespace SagaEditor
