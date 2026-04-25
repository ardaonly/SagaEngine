#pragma once
#include "SagaEditor/UI/Qt/QtPanel.h"
namespace SagaEditor::VisualScripting {
class BreakpointPanel final : public SagaEditor::QtPanel {
public:
    BreakpointPanel();
    PanelId     GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;
};
} // namespace SagaEditor::VisualScripting
