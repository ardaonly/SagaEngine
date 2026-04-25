#pragma once
#include "SagaEditor/UI/Qt/QtPanel.h"
namespace SagaEditor::VisualScripting {
class ExecutionTraceView final : public SagaEditor::QtPanel {
public:
    ExecutionTraceView();
    PanelId     GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;
};
} // namespace SagaEditor::VisualScripting
