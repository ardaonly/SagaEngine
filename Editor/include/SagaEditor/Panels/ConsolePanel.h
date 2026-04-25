/// @file ConsolePanel.h
/// @brief Console/log panel — colour-coded severity output with auto-scroll.
///
/// Qt-free header. All widget internals live in the Impl (ConsolePanel.cpp).

#pragma once

#include "SagaEditor/Panels/IPanel.h"
#include <cstdint>
#include <memory>
#include <string>

namespace SagaEditor
{

// ─── Log Severity ─────────────────────────────────────────────────────────────

enum class LogSeverity : uint8_t { Info, Warning, Error };

// ─── Console Panel ────────────────────────────────────────────────────────────

/// Scrollable rich-text log output. Each severity renders in a distinct colour.
/// Auto-scrolls unless the user has manually scrolled up to review earlier output.
class ConsolePanel : public IPanel
{
public:
    ConsolePanel();
    ~ConsolePanel() override;

    // ─── IPanel ───────────────────────────────────────────────────────────────
    [[nodiscard]] PanelId     GetPanelId()      const override;
    [[nodiscard]] std::string GetTitle()        const override;
    [[nodiscard]] void*       GetNativeWidget() const noexcept override;

    void OnInit()     override;
    void OnShutdown() override;

    // ─── Logging API ──────────────────────────────────────────────────────────
    void Log(LogSeverity severity, const std::string& message);
    void Clear();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
