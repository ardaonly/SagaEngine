/// @file EditorCustomizationDiagnostics.cpp
/// @brief Editor customization diagnostic helpers.

#include "SagaEditor/Customization/EditorCustomizationDiagnostics.h"

#include <utility>

namespace SagaEditor
{

bool HasErrorCustomizationDiagnostic(
    const std::vector<EditorCustomizationDiagnostic>& diagnostics)
{
    for (const EditorCustomizationDiagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.severity == EditorCustomizationDiagnosticSeverity::Error ||
            diagnostic.severity == EditorCustomizationDiagnosticSeverity::Blocker)
        {
            return true;
        }
    }
    return false;
}

bool HasBlockingCustomizationDiagnostic(
    const std::vector<EditorCustomizationDiagnostic>& diagnostics)
{
    for (const EditorCustomizationDiagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.severity == EditorCustomizationDiagnosticSeverity::Blocker)
        {
            return true;
        }
    }
    return false;
}

void AddCustomizationDiagnostic(
    std::vector<EditorCustomizationDiagnostic>& diagnostics,
    EditorCustomizationDiagnosticSeverity severity,
    std::string code,
    std::string message,
    std::string targetId,
    std::string path)
{
    diagnostics.push_back({ severity,
                            std::move(code),
                            std::move(message),
                            std::move(targetId),
                            std::move(path) });
}

} // namespace SagaEditor
