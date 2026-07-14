/// @file ProblemsPanel.h
/// @brief Problems panel that displays editor diagnostics from a shared service.
///
/// Qt-free header. All widget internals live in the Impl (ProblemsPanel.cpp).

#pragma once

#include "SagaEditor/Diagnostics/EditorDiagnostic.h"
#include "SagaEditor/Panels/IPanel.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace SagaEditor
{

class IEditorDiagnosticsService;

// ─── Problems Panel ──────────────────────────────────────────────────────────

/// Shows the current editor diagnostics snapshot and lets users clear it.
class ProblemsPanel : public IPanel
{
public:
    ProblemsPanel();
    ~ProblemsPanel() override;

    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] void* GetNativeWidget() const noexcept override;

    void OnInit() override;
    void OnShutdown() override;

    /// Attach a diagnostics service; the panel subscribes until shutdown.
    void SetDiagnosticsService(IEditorDiagnosticsService* service);

    /// Replace the displayed diagnostics snapshot without mutating the service.
    void SetDiagnostics(std::vector<EditorDiagnostic> diagnostics);

    /// Return the number of diagnostics currently displayed.
    [[nodiscard]] std::size_t GetDiagnosticCount() const noexcept;

    /// Clear diagnostics through the service when attached, or clear local rows.
    void Clear();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
