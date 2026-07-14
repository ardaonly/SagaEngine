/// @file EditorDiagnosticsService.cpp
/// @brief EditorDiagnosticsService implementation.

#include "SagaEditor/Diagnostics/EditorDiagnosticsService.h"

#include <algorithm>
#include <utility>

namespace SagaEditor
{

// ─── Mutation ────────────────────────────────────────────────────────────────

EditorDiagnosticId EditorDiagnosticsService::Add(EditorDiagnostic diagnostic)
{
    const EditorDiagnosticId id = AssignId(diagnostic);
    m_diagnostics.push_back(std::move(diagnostic));
    NotifySubscribers();
    return id;
}

EditorDiagnosticId EditorDiagnosticsService::Upsert(EditorDiagnostic diagnostic)
{
    const EditorDiagnosticId id = AssignId(diagnostic);
    auto it = std::find_if(m_diagnostics.begin(),
                           m_diagnostics.end(),
                           [id](const EditorDiagnostic& candidate)
                           {
                               return candidate.id == id;
                           });

    if (it != m_diagnostics.end())
        *it = std::move(diagnostic);
    else
        m_diagnostics.push_back(std::move(diagnostic));

    NotifySubscribers();
    return id;
}

bool EditorDiagnosticsService::Remove(EditorDiagnosticId id)
{
    const auto before = m_diagnostics.size();
    m_diagnostics.erase(
        std::remove_if(m_diagnostics.begin(),
                       m_diagnostics.end(),
                       [id](const EditorDiagnostic& diagnostic)
                       {
                           return diagnostic.id == id;
                       }),
        m_diagnostics.end());

    if (m_diagnostics.size() == before)
        return false;

    NotifySubscribers();
    return true;
}

std::size_t EditorDiagnosticsService::Clear()
{
    const std::size_t removed = m_diagnostics.size();
    if (removed == 0)
        return 0;

    m_diagnostics.clear();
    NotifySubscribers();
    return removed;
}

std::size_t EditorDiagnosticsService::ClearSource(const std::string& source)
{
    const auto before = m_diagnostics.size();
    m_diagnostics.erase(
        std::remove_if(m_diagnostics.begin(),
                       m_diagnostics.end(),
                       [&source](const EditorDiagnostic& diagnostic)
                       {
                           return diagnostic.source == source;
                       }),
        m_diagnostics.end());

    const std::size_t removed = before - m_diagnostics.size();
    if (removed > 0)
        NotifySubscribers();

    return removed;
}

std::size_t EditorDiagnosticsService::ReplaceSource(
    const std::string& source,
    std::vector<EditorDiagnostic> diagnostics)
{
    const auto before = m_diagnostics.size();
    m_diagnostics.erase(
        std::remove_if(m_diagnostics.begin(),
                       m_diagnostics.end(),
                       [&source](const EditorDiagnostic& diagnostic)
                       {
                           return diagnostic.source == source;
                       }),
        m_diagnostics.end());

    const std::size_t removed = before - m_diagnostics.size();
    const std::size_t inserted = diagnostics.size();
    for (EditorDiagnostic& diagnostic : diagnostics)
    {
        diagnostic.source = source;
        AssignId(diagnostic);
        m_diagnostics.push_back(std::move(diagnostic));
    }

    if (inserted > 0 || removed > 0)
        NotifySubscribers();

    return inserted;
}

// ─── Queries ─────────────────────────────────────────────────────────────────

const std::vector<EditorDiagnostic>&
EditorDiagnosticsService::GetAll() const noexcept
{
    return m_diagnostics;
}

std::vector<EditorDiagnostic> EditorDiagnosticsService::GetBySeverity(
    EditorDiagnosticSeverity severity) const
{
    std::vector<EditorDiagnostic> result;
    for (const EditorDiagnostic& diagnostic : m_diagnostics)
    {
        if (diagnostic.severity == severity)
            result.push_back(diagnostic);
    }
    return result;
}

// ─── Subscriptions ───────────────────────────────────────────────────────────

EditorDiagnosticsSubscription EditorDiagnosticsService::Subscribe(
    EditorDiagnosticsChangedCallback callback)
{
    if (!callback)
        return kInvalidEditorDiagnosticsSubscription;

    const EditorDiagnosticsSubscription subscription = m_nextSubscription++;
    m_callbacks.emplace(subscription, callback);

    try
    {
        callback(m_diagnostics);
    }
    catch (...)
    {
        // Diagnostics subscribers must not break editor startup.
    }

    return subscription;
}

bool EditorDiagnosticsService::Unsubscribe(
    EditorDiagnosticsSubscription subscription)
{
    return m_callbacks.erase(subscription) > 0;
}

// ─── Internals ───────────────────────────────────────────────────────────────

EditorDiagnosticId EditorDiagnosticsService::AssignId(
    EditorDiagnostic& diagnostic)
{
    if (diagnostic.id == kInvalidEditorDiagnosticId)
    {
        diagnostic.id = m_nextDiagnosticId++;
    }
    else if (diagnostic.id >= m_nextDiagnosticId)
    {
        m_nextDiagnosticId = diagnostic.id + 1;
    }

    return diagnostic.id;
}

void EditorDiagnosticsService::NotifySubscribers()
{
    const std::vector<EditorDiagnostic> snapshot = m_diagnostics;
    std::vector<EditorDiagnosticsChangedCallback> callbacks;
    callbacks.reserve(m_callbacks.size());
    for (const auto& [subscription, callback] : m_callbacks)
    {
        (void)subscription;
        callbacks.push_back(callback);
    }

    for (const auto& callback : callbacks)
    {
        if (!callback)
            continue;

        try
        {
            callback(snapshot);
        }
        catch (...)
        {
            // Diagnostics dispatch is best-effort; bad UI observers are isolated.
        }
    }
}

} // namespace SagaEditor
