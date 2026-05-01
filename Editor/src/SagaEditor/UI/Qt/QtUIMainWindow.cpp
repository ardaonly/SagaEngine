/// @file QtUIMainWindow.cpp
/// @brief Qt implementation of IUIMainWindow.

#include "SagaEditor/UI/Qt/QtUIMainWindow.h"
#include "SagaEditor/Shell/ShellLayout.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QWidget>
#include <QString>
#include <QCloseEvent>
#include <unordered_map>
#include <string>

namespace SagaEditor
{

// ─── Inner QMainWindow subclass ───────────────────────────────────────────────

/// Private QMainWindow subclass — forwards closeEvent to the registered callback.
class QtMainWindowWidget : public QMainWindow
{
public:
    explicit QtMainWindowWidget(QWidget* parent = nullptr)
        : QMainWindow(parent)
    {
        setDockNestingEnabled(true);
        setAnimated(false);
    }

    std::function<void()> onClose; ///< Forwarded from IUIMainWindow::SetOnClose.

protected:
    void closeEvent(QCloseEvent* event) override
    {
        if (onClose)
            onClose();
        event->accept();
    }
};

// ─── Pimpl ────────────────────────────────────────────────────────────────────

struct QtUIMainWindow::Impl
{
    QtMainWindowWidget* window = nullptr; ///< Owned — deleted by Qt parent chain.

    /// panelId → QDockWidget* (non-owning; Qt parent is the main window).
    std::unordered_map<std::string, QDockWidget*> dockMap;

    explicit Impl(const std::string& title, int w, int h)
    {
        window = new QtMainWindowWidget();
        window->setWindowTitle(QString::fromStdString(title));
        window->resize(w, h);
        window->statusBar()->showMessage("Ready");
    }

    ~Impl()
    {
        delete window;
    }

    // ── Helper: translate UIDockArea → Qt::DockWidgetArea ─────────────────────
    static Qt::DockWidgetArea ToDockArea(UIDockArea area)
    {
        switch (area)
        {
            case UIDockArea::Left:   return Qt::LeftDockWidgetArea;
            case UIDockArea::Right:  return Qt::RightDockWidgetArea;
            case UIDockArea::Top:    return Qt::TopDockWidgetArea;
            case UIDockArea::Bottom: return Qt::BottomDockWidgetArea;
            default:                 return Qt::RightDockWidgetArea;
        }
    }
};

// ─── Construction ─────────────────────────────────────────────────────────────

QtUIMainWindow::QtUIMainWindow(const std::string& title, int width, int height)
    : m_impl(std::make_unique<Impl>(title, width, height))
{}

QtUIMainWindow::~QtUIMainWindow() = default;

// ─── Visibility & Sizing ──────────────────────────────────────────────────────

void QtUIMainWindow::Show()          { m_impl->window->show(); }
void QtUIMainWindow::ShowMaximized() { m_impl->window->showMaximized(); }
void QtUIMainWindow::Hide()          { m_impl->window->hide(); }
void QtUIMainWindow::Close()         { m_impl->window->close(); }

void QtUIMainWindow::SetTitle(const std::string& title)
{
    m_impl->window->setWindowTitle(QString::fromStdString(title));
}

void QtUIMainWindow::SetSize(int width, int height)
{
    m_impl->window->resize(width, height);
}

int   QtUIMainWindow::GetWidth()        const noexcept { return m_impl->window->width(); }
int   QtUIMainWindow::GetHeight()       const noexcept { return m_impl->window->height(); }
void* QtUIMainWindow::GetNativeHandle() const noexcept { return m_impl->window; }

// ─── Chrome ───────────────────────────────────────────────────────────────────

void QtUIMainWindow::ApplyShellLayout(const ShellLayout& layout)
{
    QMenuBar* bar = m_impl->window->menuBar();
    bar->clear();

    for (const MenuDescriptor& menuDesc : layout.menus)
    {
        QMenu* menu = bar->addMenu(QString::fromStdString(menuDesc.title));

        for (const MenuItemDescriptor& item : menuDesc.items)
        {
            if (item.separator)
            {
                menu->addSeparator();
                continue;
            }

            QAction* action = menu->addAction(QString::fromStdString(item.label));

            // Display the shortcut hint as authored; the actual binding
            // lives in `ShortcutManager`. Empty hints are skipped so we
            // don't show a lonely "Ctrl+" with no key letter.
            if (!item.shortcut.empty())
            {
                action->setShortcut(
                    QKeySequence(QString::fromStdString(item.shortcut)));
            }

            // The descriptor carries the command id, not a handler. We
            // stash the id on the action and let a higher layer
            // (EditorShell) connect the dispatcher to QAction::triggered.
            // This keeps the IUIMainWindow surface decoupled from the
            // command-dispatch service.
            if (!item.commandId.empty())
            {
                action->setData(QString::fromStdString(item.commandId));
            }
        }
    }
}

void QtUIMainWindow::SetStatusMessage(const std::string& message)
{
    m_impl->window->statusBar()->showMessage(QString::fromStdString(message));
}

// ─── Panel Docking ────────────────────────────────────────────────────────────

void QtUIMainWindow::DockPanel(void*              nativeWidget,
                                const std::string& panelId,
                                const std::string& title,
                                UIDockArea         area)
{
    auto* widget = static_cast<QWidget*>(nativeWidget);

    if (area == UIDockArea::Center)
    {
        // Central widget — not wrapped in a QDockWidget.
        m_impl->window->setCentralWidget(widget);
        return;
    }

    auto* dock = new QDockWidget(QString::fromStdString(title), m_impl->window);
    dock->setObjectName(QString::fromStdString(panelId));
    dock->setWidget(widget);

    if (area == UIDockArea::Floating)
    {
        dock->setFloating(true);
        dock->show();
    }
    else
    {
        m_impl->window->addDockWidget(Impl::ToDockArea(area), dock);
    }

    m_impl->dockMap[panelId] = dock;
}

void QtUIMainWindow::UndockPanel(const std::string& panelId)
{
    auto it = m_impl->dockMap.find(panelId);
    if (it == m_impl->dockMap.end())
        return;

    m_impl->window->removeDockWidget(it->second);
    it->second->deleteLater();
    m_impl->dockMap.erase(it);
}

void QtUIMainWindow::FocusPanel(const std::string& panelId)
{
    auto it = m_impl->dockMap.find(panelId);
    if (it != m_impl->dockMap.end())
        it->second->raise();
}

// ─── Layout Persistence ───────────────────────────────────────────────────────

std::vector<uint8_t> QtUIMainWindow::SaveState() const
{
    const QByteArray raw = m_impl->window->saveState();
    return std::vector<uint8_t>(raw.begin(), raw.end());
}

bool QtUIMainWindow::RestoreState(const std::vector<uint8_t>& state)
{
    const QByteArray raw(reinterpret_cast<const char*>(state.data()),
                         static_cast<qsizetype>(state.size()));
    return m_impl->window->restoreState(raw);
}

// ─── Events ───────────────────────────────────────────────────────────────────

void QtUIMainWindow::SetOnClose(CloseCallback cb)
{
    m_impl->window->onClose = std::move(cb);
}

} // namespace SagaEditor
