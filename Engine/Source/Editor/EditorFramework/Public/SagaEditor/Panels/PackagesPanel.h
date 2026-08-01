// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor
{
class CommandDispatcher;
class PackageStateCatalog;
class ProjectReadinessSession;
class PackageModuleWorkflow;
class PackageWorkflowController;

class PackagesPanel final : public IPanel
{
public:
    PackagesPanel();
    ~PackagesPanel() override;
    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] QWidget* Widget() const noexcept override;
    void OnInit() override;
    void OnShutdown() override;
    void SetCatalog(PackageStateCatalog* catalog) noexcept;
    void SetReadiness(ProjectReadinessSession* readiness) noexcept;
    void SetWorkflow(PackageModuleWorkflow* workflow) noexcept;
    void SetController(PackageWorkflowController* controller) noexcept;
    void SetCommandDispatcher(CommandDispatcher* dispatcher) noexcept;
    void OpenOrRefresh();
    void EnableSelected();
    void DisableSelected();
    void RetryRefresh();
    void InspectRecovery();
    void Undo();
    void Redo();
    void Refresh();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
