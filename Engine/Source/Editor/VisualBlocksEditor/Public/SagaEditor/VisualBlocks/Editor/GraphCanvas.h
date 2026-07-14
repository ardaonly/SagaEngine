#pragma once
#include "SagaEditor/UI/Qt/QtPanel.h"
namespace SagaEditor::VisualScripting {
class GraphDocument;
class GraphCanvas final : public SagaEditor::QtPanel {
public:
    explicit GraphCanvas(GraphDocument& doc);
    PanelId     GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;
private:
    GraphDocument& m_doc;
};
} // namespace SagaEditor::VisualScripting
