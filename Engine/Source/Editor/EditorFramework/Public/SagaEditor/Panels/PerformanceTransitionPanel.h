// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor
{
class EditorDocumentWorkspace;

class PerformanceTransitionPanel final : public IPanel
{
public:
    PerformanceTransitionPanel();
    ~PerformanceTransitionPanel() override;
    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] QWidget* Widget() const noexcept override;
    void OnInit() override;
    void SetWorkspace(EditorDocumentWorkspace* workspace) noexcept;
    void RefreshProfiles();
    void CreateVerifiedRunInput();
    void LoadResults();
    void ReviewConversionState();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
