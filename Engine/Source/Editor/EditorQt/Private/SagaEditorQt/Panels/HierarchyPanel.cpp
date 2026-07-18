/// @file HierarchyPanel.cpp
/// @brief Qt backend implementation. The public header lives outside
///        UI/Qt/ (Panels/HierarchyPanel.h);
///        Qt headers are permitted only inside this folder. If you
///        need to use Qt for something new, add a sibling here.

#include "SagaEditor/Panels/HierarchyPanel.h"
#include "SagaEditor/SceneEditing/SceneSelectionIdentity.h"
#include "SagaEditor/Selection/SelectionManager.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <unordered_map>

namespace SagaEditor
{

// ─── Qt widget implementation ─────────────────────────────────────────────────

struct HierarchyPanel::Impl
{
    QWidget*     widget    = nullptr;
    QLineEdit*   searchBox = nullptr;
    QTreeWidget* tree      = nullptr;
    SelectionManager* selection = nullptr;
    EditorDocumentWorkspace* workspace = nullptr;
    EditorDocumentWorkspace::ChangeSubscription workspaceSubscription =
        EditorDocumentWorkspace::InvalidChangeSubscription;
    SelectionSubscription subscription = kInvalidSelectionSubscription;
    SceneDocument* observedScene = nullptr;
    SceneDocument::ChangeSubscription sceneSubscription = SceneDocument::InvalidChangeSubscription;
    bool synchronizing = false;

    Impl()
    {
        widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        searchBox = new QLineEdit(widget);
        searchBox->setPlaceholderText("Search entities…");
        layout->addWidget(searchBox);

        tree = new QTreeWidget(widget);
        tree->setHeaderHidden(true);
        tree->setUniformRowHeights(true);
        layout->addWidget(tree);

        QObject::connect(searchBox, &QLineEdit::textChanged,
            [this](const QString& text)
            {
                const auto visit = [&text](QTreeWidgetItem* item, const auto& self) -> bool
                {
                    bool visible = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive);
                    for (int child = 0; child < item->childCount(); ++child)
                        visible = self(item->child(child), self) || visible;
                    item->setHidden(!visible);
                    return visible;
                };
                for (int i = 0; i < tree->topLevelItemCount(); ++i) visit(tree->topLevelItem(i), visit);
            });
        QObject::connect(tree, &QTreeWidget::itemSelectionChanged, [this]
        {
            if (synchronizing || !selection) return;
            const auto selected = tree->selectedItems();
            if (selected.empty()) selection->ClearSelection();
            else selection->SetSelection(selected.front()->data(0, Qt::UserRole).toULongLong());
        });
    }

    void Populate()
    {
        tree->clear();
        if (!workspace || !workspace->ActiveScene()) return;
        const auto& scene = *workspace->ActiveScene();
        std::unordered_map<std::string, QTreeWidgetItem*> items;
        for (const auto& entity : scene.Entities())
        {
            auto* item = new QTreeWidgetItem(QStringList{QString::fromStdString(entity.name)});
            item->setData(0, Qt::UserRole,
                QVariant::fromValue<qulonglong>(SceneSelectionId(scene.SceneId(), entity.entityId)));
            item->setData(0, Qt::UserRole + 1, QString::fromStdString(entity.entityId));
            items.emplace(entity.entityId, item);
        }
        for (const auto& entity : scene.Entities())
        {
            auto* item = items.at(entity.entityId);
            if (entity.parentEntityId && items.contains(*entity.parentEntityId))
                items.at(*entity.parentEntityId)->addChild(item);
            else tree->addTopLevelItem(item);
        }
        tree->expandAll();
    }

    void SyncSelection(const std::vector<SelectableId>& selected)
    {
        synchronizing = true;
        tree->clearSelection();
        if (!selected.empty())
        {
            const auto matches = tree->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
            for (auto* item : matches)
                if (item->data(0, Qt::UserRole).toULongLong() == selected.front())
                { item->setSelected(true); tree->scrollToItem(item); break; }
        }
        synchronizing = false;
    }

    void DetachSelection()
    {
        if (selection && subscription != kInvalidSelectionSubscription)
            selection->Unsubscribe(subscription);
        subscription = kInvalidSelectionSubscription;
        selection = nullptr;
    }

    void DetachScene()
    {
        if (observedScene && sceneSubscription != SceneDocument::InvalidChangeSubscription)
            observedScene->UnsubscribeChanges(sceneSubscription);
        observedScene = nullptr;
        sceneSubscription = SceneDocument::InvalidChangeSubscription;
    }

    void RefreshWorkspace()
    {
        DetachScene();
        Populate();
        if (workspace && workspace->ActiveScene())
        {
            observedScene = workspace->ActiveScene();
            sceneSubscription = observedScene->SubscribeChanges(
                [this] { Populate(); });
        }
    }

    void DetachWorkspace()
    {
        DetachScene();
        if (workspace && workspaceSubscription != EditorDocumentWorkspace::InvalidChangeSubscription)
            workspace->UnsubscribeChanges(workspaceSubscription);
        workspaceSubscription = EditorDocumentWorkspace::InvalidChangeSubscription;
        workspace = nullptr;
    }
};

// ─── HierarchyPanel ───────────────────────────────────────────────────────────

HierarchyPanel::HierarchyPanel()
    : m_impl(std::make_unique<Impl>())
{}

HierarchyPanel::~HierarchyPanel() { m_impl->DetachSelection(); m_impl->DetachWorkspace(); }

PanelId     HierarchyPanel::GetPanelId()      const { return "saga.panel.hierarchy"; }
std::string HierarchyPanel::GetTitle()        const { return "Hierarchy"; }
void*       HierarchyPanel::GetNativeWidget() const noexcept { return m_impl->widget; }

void HierarchyPanel::OnInit()     { m_impl->Populate(); }
void HierarchyPanel::OnShutdown() { m_impl->DetachSelection(); m_impl->DetachWorkspace(); }

void HierarchyPanel::SetSelectionManager(SelectionManager* mgr)
{
    m_impl->DetachSelection();
    m_impl->selection = mgr;
    if (mgr)
    {
        m_impl->subscription = mgr->Subscribe([impl = m_impl.get()](const auto& selection)
        { impl->SyncSelection(selection); });
        m_impl->SyncSelection(mgr->GetAll());
    }
}

void HierarchyPanel::SetDocumentWorkspace(EditorDocumentWorkspace* workspace)
{
    m_impl->DetachWorkspace();
    m_impl->workspace = workspace;
    if (workspace)
    {
        m_impl->workspaceSubscription = workspace->SubscribeChanges(
            [impl = m_impl.get()] { impl->RefreshWorkspace(); });
    }
    m_impl->RefreshWorkspace();
}

} // namespace SagaEditor
