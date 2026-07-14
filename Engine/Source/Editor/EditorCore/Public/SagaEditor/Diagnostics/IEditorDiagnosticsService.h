/// @file IEditorDiagnosticsService.h
/// @brief Service contract for collecting and observing editor diagnostics.

#pragma once

#include "SagaEditor/Diagnostics/EditorDiagnostic.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SagaEditor
{

// ─── Diagnostics Subscriptions ───────────────────────────────────────────────

using EditorDiagnosticsChangedCallback =
    std::function<void(const std::vector<EditorDiagnostic>&)>;

using EditorDiagnosticsSubscription = uint64_t;
inline constexpr EditorDiagnosticsSubscription
    kInvalidEditorDiagnosticsSubscription = 0;

// ─── Diagnostics Service Interface ───────────────────────────────────────────

/// Collects diagnostics from editor systems and broadcasts snapshots to panels.
class IEditorDiagnosticsService
{
public:
    virtual ~IEditorDiagnosticsService() = default;

    /// Add a diagnostic and return its assigned stable id.
    virtual EditorDiagnosticId Add(EditorDiagnostic diagnostic) = 0;

    /// Replace an existing diagnostic by id, or add it when the id is new.
    virtual EditorDiagnosticId Upsert(EditorDiagnostic diagnostic) = 0;

    /// Remove one diagnostic by id. Returns false if the id is unknown.
    virtual bool Remove(EditorDiagnosticId id) = 0;

    /// Remove all diagnostics and return the number of removed records.
    virtual std::size_t Clear() = 0;

    /// Remove diagnostics from one source and return the number removed.
    virtual std::size_t ClearSource(const std::string& source) = 0;

    /// Replace all diagnostics from one source with a refreshed snapshot.
    virtual std::size_t ReplaceSource(
        const std::string& source,
        std::vector<EditorDiagnostic> diagnostics) = 0;

    /// Return all diagnostics in insertion order.
    [[nodiscard]] virtual const std::vector<EditorDiagnostic>&
        GetAll() const noexcept = 0;

    /// Return diagnostics matching one severity in insertion order.
    [[nodiscard]] virtual std::vector<EditorDiagnostic>
        GetBySeverity(EditorDiagnosticSeverity severity) const = 0;

    /// Subscribe to diagnostic changes; the callback receives the current snapshot immediately.
    virtual EditorDiagnosticsSubscription Subscribe(
        EditorDiagnosticsChangedCallback callback) = 0;

    /// Remove a prior subscription.
    virtual bool Unsubscribe(EditorDiagnosticsSubscription subscription) = 0;
};

} // namespace SagaEditor
