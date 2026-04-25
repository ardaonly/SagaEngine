#pragma once
#include "SagaEditor/UI/Qt/QtPanel.h"
namespace SagaEditor {
class ProjectBrowserPanel final : public QtPanel {
public:
    ProjectBrowserPanel();
    PanelId     GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;
};
} // namespace SagaEditor
