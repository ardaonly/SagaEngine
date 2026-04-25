#pragma once
#include "SagaEditor/UI/Qt/QtPanel.h"
namespace SagaEditor {
class ProfilerPanel final : public QtPanel {
public:
    ProfilerPanel();
    PanelId     GetPanelId() const override;
    std::string GetTitle()   const override;
    void        OnInit()     override;
};
} // namespace SagaEditor
