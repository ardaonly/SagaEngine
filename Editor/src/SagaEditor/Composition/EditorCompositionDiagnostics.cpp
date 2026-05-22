/// @file EditorCompositionDiagnostics.cpp
/// @brief Helpers for querying editor composition diagnostic collections.

#include "SagaEditor/Composition/EditorCompositionDiagnostics.h"

namespace SagaEditor
{

bool HasBlockingCompositionDiagnostic(
    const std::vector<EditorCompositionDiagnostic>& diagnostics) noexcept
{
    for (const EditorCompositionDiagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.severity == EditorCompositionDiagnosticSeverity::Blocker)
        {
            return true;
        }
    }

    return false;
}

bool HasErrorCompositionDiagnostic(
    const std::vector<EditorCompositionDiagnostic>& diagnostics) noexcept
{
    for (const EditorCompositionDiagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.severity == EditorCompositionDiagnosticSeverity::Error ||
            diagnostic.severity == EditorCompositionDiagnosticSeverity::Blocker)
        {
            return true;
        }
    }

    return false;
}

} // namespace SagaEditor
