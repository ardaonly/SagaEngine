/// @file EditorDiagnosticsService.h
/// @brief In-memory implementation of the editor diagnostics service.

#pragma once

#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"

#include <unordered_map>

namespace SagaEditor
{

// ─── Editor Diagnostics Service ──────────────────────────────────────────────

/// Stores diagnostics in insertion order and notifies subscribers after changes.
class EditorDiagnosticsService final : public IEditorDiagnosticsService
{
public:
    EditorDiagnosticsService() = default;
    ~EditorDiagnosticsService() override = default;

    EditorDiagnosticId Add(EditorDiagnostic diagnostic) override;
    EditorDiagnosticId Upsert(EditorDiagnostic diagnostic) override;
    bool Remove(EditorDiagnosticId id) override;
    std::size_t Clear() override;
    std::size_t ClearSource(const std::string& source) override;
    std::size_t ReplaceSource(const std::string& source,
                              std::vector<EditorDiagnostic> diagnostics) override;

    [[nodiscard]] const std::vector<EditorDiagnostic>&
        GetAll() const noexcept override;
    [[nodiscard]] std::vector<EditorDiagnostic>
        GetBySeverity(EditorDiagnosticSeverity severity) const override;

    EditorDiagnosticsSubscription Subscribe(
        EditorDiagnosticsChangedCallback callback) override;
    bool Unsubscribe(EditorDiagnosticsSubscription subscription) override;

private:
    EditorDiagnosticId AssignId(EditorDiagnostic& diagnostic);
    void NotifySubscribers();

    std::vector<EditorDiagnostic> m_diagnostics; ///< Current diagnostics snapshot.
    std::unordered_map<EditorDiagnosticsSubscription,
                       EditorDiagnosticsChangedCallback> m_callbacks;
    EditorDiagnosticId m_nextDiagnosticId = 1;
    EditorDiagnosticsSubscription m_nextSubscription = 1;
};

} // namespace SagaEditor
