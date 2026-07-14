/// @file NodeLibraryPanel.h
/// @brief Backend-neutral visual scripting node library panel contract.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor::VisualScripting
{

class NodeCategoryBrowser;

/// Dockable node library surface; Qt widget ownership stays private.
class NodeLibraryPanel final : public SagaEditor::IPanel
{
public:
    explicit NodeLibraryPanel(NodeCategoryBrowser& browser);
    ~NodeLibraryPanel() override;

    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] void* GetNativeWidget() const noexcept override;

    void OnInit() override;
    void OnShutdown() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor::VisualScripting
