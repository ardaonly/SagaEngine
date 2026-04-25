#pragma once
#include "SagaEditor/UI/Qt/QtPanel.h"
namespace SagaEditor::VisualScripting {
class NodePalette final : public SagaEditor::QtPanel {
public:
    NodePalette();
    PanelId     GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;
};
} // namespace SagaEditor::VisualScripting
