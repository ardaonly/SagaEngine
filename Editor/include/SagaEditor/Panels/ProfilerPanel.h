/// @file ProfilerPanel.h
/// @brief Backend-neutral profiler panel contract.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor
{

/// Dockable profiler surface; Qt widget ownership stays private.
class ProfilerPanel final : public IPanel
{
public:
    ProfilerPanel();
    ~ProfilerPanel() override;

    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] void* GetNativeWidget() const noexcept override;

    void OnInit() override;
    void OnShutdown() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
