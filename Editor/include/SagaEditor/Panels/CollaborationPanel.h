#pragma once
#include "SagaEditor/UI/Qt/QtPanel.h"
namespace SagaEditor {
class CollaborationPanel final : public QtPanel {
public:
    CollaborationPanel();
    PanelId     GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;
};
} // namespace SagaEditor
