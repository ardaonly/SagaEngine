#pragma once
#include "SagaEditor/UI/Qt/QtPanel.h"
namespace SagaEditor::VisualScripting {
class WatchPanel final : public SagaEditor::QtPanel {
public:
    WatchPanel();
    PanelId     GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;
};
} // namespace SagaEditor::VisualScripting
