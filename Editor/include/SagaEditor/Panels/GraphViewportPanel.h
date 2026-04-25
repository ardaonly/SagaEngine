#pragma once
#include "SagaEditor/UI/Qt/QtPanel.h"
namespace SagaEditor {
class GraphViewportPanel final : public QtPanel {
public:
    GraphViewportPanel();
    PanelId     GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;
};
} // namespace SagaEditor
