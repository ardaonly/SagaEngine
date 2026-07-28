// SPDX-License-Identifier: MPL-2.0
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
#include <QTimer>
#include <QStackedWidget>
#include <QWidget>
#include <QString>
#include <QCloseEvent>
#include <QObject>
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

    IUIMainWindow::CloseCallback onClose; ///< Forwarded from IUIMainWindow::SetOnClose.

protected:
    void closeEvent(QCloseEvent* event) override
    {
        if (!onClose || onClose()) event->accept();
        else event->ignore();
    }
};

// ─── Pimpl ────────────────────────────────────────────────────────────────────

struct QtUIMainWindow::Impl
{
    QMainWindow* window = nullptr;
    QStackedWidget* centralStack = nullptr;
    bool ownsWindow = true;
    QWidget* centralWidget = nullptr;
    std::string centralPanelId;

    /// panelId → QDockWidget* (non-owning; Qt parent is the main window).
    std::unordered_map<std::string, QDockWidget*> dockMap;
    std::unordered_map<std::string, QWidget*>     widgetMap;
    CommandCallback                               onCommand;
    WorkflowStateCallback                         onWorkflowStatePoll;
    QToolBar*                                     mainToolbar = nullptr;
    QTimer*                                       workflowStateTimer = nullptr;
    std::unordered_map<std::string, std::vector<QAction*>> commandActions;

    explicit Impl(const std::string& title, int w, int h)
    {
        window = new QtMainWindowWidget();
        window->setWindowTitle(QString::fromStdString(title));
        window->resize(w, h);
        window->statusBar()->showMessage("Ready");
        workflowStateTimer = new QTimer(window);
        workflowStateTimer->setInterval(100);
        QObject::connect(workflowStateTimer, &QTimer::timeout, window, [this]
        {
            if (onWorkflowStatePoll)
                onWorkflowStatePoll();
        });
        workflowStateTimer->start();
    }

    Impl(QMainWindow& attachedWindow, QStackedWidget& stack)
        : window(&attachedWindow), centralStack(&stack), ownsWindow(false)
    {
        window->setDockNestingEnabled(true);
        window->setAnimated(false);
        workflowStateTimer = new QTimer(window);
        workflowStateTimer->setInterval(100);
        QObject::connect(workflowStateTimer, &QTimer::timeout, window, [this]
        {
            if (onWorkflowStatePoll)
                onWorkflowStatePoll();
        });
        workflowStateTimer->start();
    }

    ~Impl()
    {
        if (workflowStateTimer)
            workflowStateTimer->stop();
        if (mainToolbar)
        {
            window->removeToolBar(mainToolbar);
            delete mainToolbar;
            mainToolbar = nullptr;
        }
        for (const auto& [id, dock] : dockMap)
        {
            window->removeDockWidget(dock);
            delete dock;
        }
        dockMap.clear();
        if (centralStack && centralWidget)
        {
            centralStack->removeWidget(centralWidget);
            delete centralWidget;
        }
        if (ownsWindow)
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

QtUIMainWindow::QtUIMainWindow(QMainWindow& window, QStackedWidget& centralStack)
    : m_impl(std::make_unique<Impl>(window, centralStack))
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

int QtUIMainWindow::GetWidth()  const noexcept { return m_impl->window->width(); }
int QtUIMainWindow::GetHeight() const noexcept { return m_impl->window->height(); }

// ─── Chrome ───────────────────────────────────────────────────────────────────

void QtUIMainWindow::ApplyShellLayout(const ShellLayout& layout)
{
    m_impl->commandActions.clear();
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
                m_impl->commandActions[item.commandId].push_back(action);
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
        m_impl->mainToolbar->setObjectName(QStringLiteral("SagaEditorWorkspaceToolbar"));
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
                m_impl->commandActions[item.commandId].push_back(action);
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

void QtUIMainWindow::SetCommandEnabled(const std::string& commandId, bool enabled)
{
    const auto it = m_impl->commandActions.find(commandId);
    if (it == m_impl->commandActions.end())
        return;
    for (QAction* action : it->second)
        action->setEnabled(enabled);
}

void QtUIMainWindow::SetCommandChecked(const std::string& commandId, bool checked)
{
    const auto it = m_impl->commandActions.find(commandId);
    if (it == m_impl->commandActions.end())
        return;
    for (QAction* action : it->second)
    {
        action->setCheckable(true);
        action->setChecked(checked);
    }
}

void QtUIMainWindow::ResetWorkspaceLayout()
{
    const auto place = [this](const char* id, Qt::DockWidgetArea area)
    {
        const auto it = m_impl->dockMap.find(id);
        if (it == m_impl->dockMap.end())
            return;
        it->second->setFloating(false);
        m_impl->window->addDockWidget(area, it->second);
    };
    place("saga.panel.hierarchy", Qt::LeftDockWidgetArea);
    place("saga.panel.inspector", Qt::RightDockWidgetArea);
    place("saga.panel.assetbrowser", Qt::BottomDockWidgetArea);
    place("saga.panel.code_box", Qt::BottomDockWidgetArea);
    place("saga.panel.problems", Qt::BottomDockWidgetArea);
    const auto sources = m_impl->dockMap.find("saga.panel.assetbrowser");
    const auto codeBox = m_impl->dockMap.find("saga.panel.code_box");
    const auto problems = m_impl->dockMap.find("saga.panel.problems");
    if (sources != m_impl->dockMap.end() && codeBox != m_impl->dockMap.end())
        m_impl->window->tabifyDockWidget(codeBox->second, sources->second);
    if (codeBox != m_impl->dockMap.end() && problems != m_impl->dockMap.end())
        m_impl->window->tabifyDockWidget(codeBox->second, problems->second);
}

// ─── Panel Docking ────────────────────────────────────────────────────────────

void QtUIMainWindow::DockPanel(QWidget*           widget,
                                const std::string& panelId,
                                const std::string& title,
                                UIDockArea         area)
{
    if (area == UIDockArea::Center)
    {
        if (m_impl->centralStack)
        {
            m_impl->centralStack->addWidget(widget);
            m_impl->centralStack->setCurrentWidget(widget);
        }
        else
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
            if (m_impl->centralStack)
            {
                m_impl->centralStack->removeWidget(m_impl->centralWidget);
                delete m_impl->centralWidget;
            }
            else
                delete m_impl->window->takeCentralWidget();
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
    if (auto* owned = dynamic_cast<QtMainWindowWidget*>(m_impl->window))
        owned->onClose = std::move(cb);
}

void QtUIMainWindow::SetOnCommand(CommandCallback cb)
{
    m_impl->onCommand = std::move(cb);
}

void QtUIMainWindow::SetOnWorkflowStatePoll(WorkflowStateCallback cb)
{
    m_impl->onWorkflowStatePoll = std::move(cb);
}

QMainWindow* QtUIMainWindow::Window() const noexcept
{
    return m_impl->window;
}

} // namespace SagaEditor
