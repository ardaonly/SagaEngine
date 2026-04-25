#pragma once
#include "SagaEditor/UI/Qt/QtPanel.h"
namespace SagaEditor::VisualScripting {
class NodeCategoryBrowser;
class NodeLibraryPanel final : public SagaEditor::QtPanel {
public:
    explicit NodeLibraryPanel(NodeCategoryBrowser& browser);
    PanelId     GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;
private:
    NodeCategoryBrowser& m_browser;
};
} // namespace SagaEditor::VisualScripting
