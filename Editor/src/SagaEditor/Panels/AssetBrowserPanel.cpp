/// @file AssetBrowserPanel.cpp
/// @brief Asset browser panel — Qt widget lives entirely inside Impl.

#include "SagaEditor/Panels/AssetBrowserPanel.h"
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

        QObject::connect(dirTree, &QTreeWidget::itemSelectionChanged,
            [this]() { assetGrid->clear(); /* TODO: enumerate files */ });

        QObject::connect(assetGrid, &QListWidget::itemDoubleClicked,
            [](QListWidgetItem*) { /* TODO: open asset editor */ });
    }

    void PopulateDirectoryTree()
    {
        dirTree->clear();
        auto* root = new QTreeWidgetItem(dirTree, QStringList{"Assets"});
        root->addChild(new QTreeWidgetItem(QStringList{"Meshes"}));
        root->addChild(new QTreeWidgetItem(QStringList{"Textures"}));
        root->addChild(new QTreeWidgetItem(QStringList{"Materials"}));
        root->addChild(new QTreeWidgetItem(QStringList{"Scripts"}));
        dirTree->expandAll();
    }
};

// ─── AssetBrowserPanel ────────────────────────────────────────────────────────

AssetBrowserPanel::AssetBrowserPanel()
    : m_impl(std::make_unique<Impl>())
{}

AssetBrowserPanel::~AssetBrowserPanel() = default;

PanelId     AssetBrowserPanel::GetPanelId()      const { return "saga.panel.assetbrowser"; }
std::string AssetBrowserPanel::GetTitle()        const { return "Asset Browser"; }
void*       AssetBrowserPanel::GetNativeWidget() const noexcept { return m_impl->widget; }

void AssetBrowserPanel::OnInit()     { m_impl->PopulateDirectoryTree(); }
void AssetBrowserPanel::OnShutdown() {}

void AssetBrowserPanel::SetWorkspaceRoot(const std::string& path)
{
    m_impl->rootPath = path;
    m_impl->PopulateDirectoryTree();
}

} // namespace SagaEditor
