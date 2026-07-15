#pragma once

#include "SagaEditorQt/QtPanel.h"

namespace SagaEditor::VisualBlocks
{
class GraphDocument;
}

namespace SagaEditorQt::VisualScripting
{

class GraphCanvas final : public SagaEditor::QtPanel
{
public:
    explicit GraphCanvas(SagaEditor::VisualBlocks::GraphDocument& doc);
    SagaEditor::PanelId GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;

private:
    SagaEditor::VisualBlocks::GraphDocument& m_doc;
};

} // namespace SagaEditorQt::VisualScripting
