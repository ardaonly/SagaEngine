/// @file AssetBrowserPanel.cpp
/// @brief Qt backend implementation. The public header lives outside
///        UI/Qt/ (Panels/AssetBrowserPanel.h);
///        Qt headers are permitted only inside this folder. If you
///        need to use Qt for something new, add a sibling here.

#include "SagaEditor/Panels/AssetBrowserPanel.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <QBrush>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSplitter>
#include <QTreeWidget>
#include <QWidget>

namespace SagaEditor
{

// ─── Qt widget implementation ─────────────────────────────────────────────────

struct AssetBrowserPanel::Impl
{
    QWidget*     widget    = nullptr;
    QSplitter*   splitter  = nullptr;
    QTreeWidget* dirTree   = nullptr;
    QListWidget* assetGrid = nullptr;
    std::string  rootPath;
    EditorDocumentWorkspace* workspace = nullptr;
    EditorDocumentWorkspace::ChangeSubscription subscription =
        EditorDocumentWorkspace::InvalidChangeSubscription;

    Impl()
    {
        widget = new QWidget();
        auto* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);

        splitter  = new QSplitter(Qt::Horizontal, widget);
        dirTree   = new QTreeWidget(splitter);
        assetGrid = new QListWidget(splitter);

        dirTree->setHeaderHidden(true);
        dirTree->setMaximumWidth(220);

        assetGrid->setViewMode(QListWidget::IconMode);
        assetGrid->setIconSize(QSize(64, 64));
        assetGrid->setResizeMode(QListWidget::Adjust);

        splitter->addWidget(dirTree);
        splitter->addWidget(assetGrid);
        splitter->setStretchFactor(1, 1);
        layout->addWidget(splitter);

        QObject::connect(dirTree, &QTreeWidget::itemSelectionChanged, [this]
        {
            assetGrid->clear();
            const auto selected = dirTree->selectedItems();
            if (selected.empty()) return;
            auto* item = selected.front();
            if (item->childCount() == 0)
                assetGrid->addItem(item->data(0, Qt::UserRole).toString());
            else
                for (int index = 0; index < item->childCount(); ++index)
                    assetGrid->addItem(item->child(index)->data(0, Qt::UserRole).toString());
        });
    }

    void PopulateDirectoryTree()
    {
        dirTree->clear();
        if (!workspace || !workspace->IsOpen()) return;
        auto* source = new QTreeWidgetItem(dirTree, QStringList{"Source Truth"});
        auto* trackedGenerated = new QTreeWidgetItem(dirTree, QStringList{"Generated (tracked exception)"});
        auto* generated = new QTreeWidgetItem(dirTree, QStringList{"Generated State"});
        auto* personal = new QTreeWidgetItem(dirTree, QStringList{"Personal State"});
        auto* shared = new QTreeWidgetItem(dirTree, QStringList{"Shared Workspace State"});
        for (const auto& entry : workspace->SourceListing())
        {
            auto* parent = source;
            switch (entry.classification)
            {
                case ProjectSourceClassification::GeneratedStateTrackedException: parent = trackedGenerated; break;
                case ProjectSourceClassification::GeneratedState: parent = generated; break;
                case ProjectSourceClassification::PersonalState: parent = personal; break;
                case ProjectSourceClassification::SharedWorkspaceState: parent = shared; break;
                case ProjectSourceClassification::SourceTruth: break;
            }
            auto* item = new QTreeWidgetItem(QStringList{QString::fromStdString(entry.relativePath.generic_string())});
            item->setData(0, Qt::UserRole, QString::fromStdString(entry.kind));
            if (entry.classification == ProjectSourceClassification::GeneratedStateTrackedException)
                item->setForeground(0, QBrush(QColor("#d7ba7d")));
            parent->addChild(item);
        }
        for (auto* category : {trackedGenerated, generated, personal, shared})
            if (category->childCount() == 0) delete category;
        dirTree->expandAll();
    }

    void Detach()
    {
        if (workspace && subscription != EditorDocumentWorkspace::InvalidChangeSubscription)
            workspace->UnsubscribeChanges(subscription);
        subscription = EditorDocumentWorkspace::InvalidChangeSubscription;
        workspace = nullptr;
    }
};

// ─── AssetBrowserPanel ────────────────────────────────────────────────────────

AssetBrowserPanel::AssetBrowserPanel()
    : m_impl(std::make_unique<Impl>())
{}

AssetBrowserPanel::~AssetBrowserPanel() { m_impl->Detach(); }

PanelId     AssetBrowserPanel::GetPanelId()      const { return "saga.panel.assetbrowser"; }
std::string AssetBrowserPanel::GetTitle()        const { return "Project Sources"; }
void*       AssetBrowserPanel::GetNativeWidget() const noexcept { return m_impl->widget; }

void AssetBrowserPanel::OnInit()     { m_impl->PopulateDirectoryTree(); }
void AssetBrowserPanel::OnShutdown() { m_impl->Detach(); }

void AssetBrowserPanel::SetWorkspaceRoot(const std::string& path)
{
    m_impl->rootPath = path;
    m_impl->PopulateDirectoryTree();
}

void AssetBrowserPanel::SetDocumentWorkspace(EditorDocumentWorkspace* workspace)
{
    m_impl->Detach();
    m_impl->workspace = workspace;
    if (workspace)
        m_impl->subscription = workspace->SubscribeChanges(
            [impl = m_impl.get()] { impl->PopulateDirectoryTree(); });
    m_impl->PopulateDirectoryTree();
}

} // namespace SagaEditor
