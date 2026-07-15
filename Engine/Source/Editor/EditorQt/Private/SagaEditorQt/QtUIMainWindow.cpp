/// @file QtUIMainWindow.cpp
/// @brief Qt implementation of IUIMainWindow.

#include "SagaEditorQt/QtUIMainWindow.h"
#include "SagaEditor/Shell/ShellLayout.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QToolBar>
#include <QWidget>
#include <QString>
#include <QCloseEvent>
#include <QEvent>
#include <QObject>
#include <QStackedWidget>
#include <utility>
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

/// Event filter for product-owned windows that cannot use QtMainWindowWidget.
class QtMainWindowCloseFilter final : public QObject
{
public:
    explicit QtMainWindowCloseFilter(QObject* parent = nullptr)
        : QObject(parent)
    {}

    std::function<void()> onClose; ///< Forwarded from IUIMainWindow::SetOnClose.

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Close && onClose)
        {
            onClose();
        }
        return QObject::eventFilter(watched, event);
    }
};

// ─── Pimpl ────────────────────────────────────────────────────────────────────

struct QtUIMainWindow::Impl
{
    QMainWindow* window = nullptr; ///< May be owned by this wrapper or by Saga.
    bool ownsWindow = true;
    QStackedWidget* centralStack = nullptr; ///< Non-owning mounted central host.
    QWidget* centralWidget = nullptr;       ///< Panel widget mounted in the stack.
    int centralIndex = -1;
    std::string centralPanelId;
    QtMainWindowCloseFilter* closeFilter = nullptr;

    /// panelId → QDockWidget* (non-owning; Qt parent is the main window).
    std::unordered_map<std::string, QDockWidget*> dockMap;
    std::unordered_map<std::string, QWidget*>     widgetMap;
    CommandCallback                               onCommand;
    QToolBar*                                     mainToolbar = nullptr;

    explicit Impl(const std::string& title, int w, int h)
    {
        window = new QtMainWindowWidget();
        window->setWindowTitle(QString::fromStdString(title));
        window->resize(w, h);
        window->statusBar()->showMessage("Ready");
    }

    Impl(QMainWindow* nativeWindow, QStackedWidget* nativeCentralStack)
        : window(nativeWindow)
        , ownsWindow(false)
        , centralStack(nativeCentralStack)
    {
        if (window != nullptr)
        {
            window->setDockNestingEnabled(true);
            window->setAnimated(false);
            window->statusBar()->showMessage("Ready");
            closeFilter = new QtMainWindowCloseFilter(window);
            window->installEventFilter(closeFilter);
        }
    }

    ~Impl()
    {
        if (mainToolbar != nullptr && window != nullptr)
        {
            window->removeToolBar(mainToolbar);
            delete mainToolbar;
            mainToolbar = nullptr;
        }

        for (auto& [panelId, dock] : dockMap)
        {
            (void)panelId;
            if (window != nullptr)
            {
                window->removeDockWidget(dock);
            }
            delete dock;
        }
        dockMap.clear();

        if (centralStack != nullptr && centralIndex >= 0)
        {
            QWidget* widget = centralStack->widget(centralIndex);
            centralStack->removeWidget(widget);
            delete widget;
        }
        else if (ownsWindow)
        {
            // QMainWindow deletes its central widget when the owned window dies.
            centralWidget = nullptr;
        }

        if (window != nullptr && closeFilter != nullptr)
        {
            window->removeEventFilter(closeFilter);
            closeFilter = nullptr;
        }

        if (!ownsWindow && window != nullptr)
        {
            // Return basic chrome visibility to the product shell. Saga rebuilds
            // its own menus immediately after editor mode shuts down.
            window->menuBar()->setVisible(true);
            window->statusBar()->setVisible(true);
        }

        if (ownsWindow)
        {
            delete window;
        }
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

QtUIMainWindow::QtUIMainWindow(void* nativeMainWindow, void* nativeCentralStack)
    : m_impl(std::make_unique<Impl>(
          static_cast<QMainWindow*>(nativeMainWindow),
          static_cast<QStackedWidget*>(nativeCentralStack)))
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
    bar->setVisible(layout.showMenuBar);
    bar->clear();

    m_impl->window->statusBar()->setVisible(layout.showStatusBar);

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
                QObject::connect(action, &QAction::triggered,
                    [this, action]()
                    {
                        if (!m_impl->onCommand)
                            return;
                        m_impl->onCommand(action->data().toString().toStdString());
                    });
            }
        }
    }

    if (m_impl->mainToolbar != nullptr)
    {
        m_impl->window->removeToolBar(m_impl->mainToolbar);
        delete m_impl->mainToolbar;
        m_impl->mainToolbar = nullptr;
    }

    if (!layout.mainToolbarItems.empty())
    {
        m_impl->mainToolbar = new QToolBar(QStringLiteral("Workspace"), m_impl->window);
        m_impl->mainToolbar->setMovable(false);
        m_impl->mainToolbar->setVisible(layout.showMainToolbar);

        for (const MenuItemDescriptor& item : layout.mainToolbarItems)
        {
            if (item.separator)
            {
                m_impl->mainToolbar->addSeparator();
                continue;
            }

            QAction* action =
                m_impl->mainToolbar->addAction(QString::fromStdString(item.label));
            if (!item.commandId.empty())
            {
                action->setData(QString::fromStdString(item.commandId));
                QObject::connect(action, &QAction::triggered,
                    [this, action]()
                    {
                        if (!m_impl->onCommand)
                            return;
                        m_impl->onCommand(action->data().toString().toStdString());
                    });
            }
        }

        m_impl->window->addToolBar(Qt::TopToolBarArea, m_impl->mainToolbar);
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
        if (m_impl->centralStack != nullptr)
        {
            m_impl->centralIndex = m_impl->centralStack->addWidget(widget);
            m_impl->centralStack->setCurrentIndex(m_impl->centralIndex);
            m_impl->centralWidget = widget;
            m_impl->centralPanelId = panelId;
            m_impl->widgetMap[panelId] = widget;
            return;
        }

        // Standalone editor launcher owns the QMainWindow central widget.
        m_impl->window->setCentralWidget(widget);
        m_impl->centralWidget = widget;
        m_impl->centralPanelId = panelId;
        m_impl->widgetMap[panelId] = widget;
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
    m_impl->widgetMap[panelId] = widget;
}

void QtUIMainWindow::UndockPanel(const std::string& panelId)
{
    auto it = m_impl->dockMap.find(panelId);
    if (it == m_impl->dockMap.end())
    {
        if (panelId == m_impl->centralPanelId)
        {
            if (m_impl->centralStack != nullptr && m_impl->centralIndex >= 0)
            {
                QWidget* widget = m_impl->centralStack->widget(m_impl->centralIndex);
                m_impl->centralStack->removeWidget(widget);
                delete widget;
                m_impl->centralIndex = -1;
            }
            m_impl->centralWidget = nullptr;
            m_impl->centralPanelId.clear();
            m_impl->widgetMap.erase(panelId);
        }
        return;
    }

    m_impl->window->removeDockWidget(it->second);
    it->second->deleteLater();
    m_impl->dockMap.erase(it);
    m_impl->widgetMap.erase(panelId);
}

void QtUIMainWindow::FocusPanel(const std::string& panelId)
{
    auto it = m_impl->dockMap.find(panelId);
    if (it != m_impl->dockMap.end())
    {
        it->second->show();
        it->second->raise();
        return;
    }

    auto widgetIt = m_impl->widgetMap.find(panelId);
    if (widgetIt != m_impl->widgetMap.end())
    {
        widgetIt->second->show();
        widgetIt->second->setFocus();
    }
}

void QtUIMainWindow::SetPanelVisible(const std::string& panelId, bool visible)
{
    auto dockIt = m_impl->dockMap.find(panelId);
    if (dockIt != m_impl->dockMap.end())
    {
        dockIt->second->setVisible(visible);
        return;
    }

    auto widgetIt = m_impl->widgetMap.find(panelId);
    if (widgetIt != m_impl->widgetMap.end())
    {
        widgetIt->second->setVisible(visible);
    }
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
    if (auto* ownedWindow = dynamic_cast<QtMainWindowWidget*>(m_impl->window))
    {
        ownedWindow->onClose = std::move(cb);
        return;
    }

    if (m_impl->closeFilter != nullptr)
    {
        m_impl->closeFilter->onClose = std::move(cb);
    }
}

void QtUIMainWindow::SetOnCommand(CommandCallback cb)
{
    m_impl->onCommand = std::move(cb);
}

} // namespace SagaEditor
