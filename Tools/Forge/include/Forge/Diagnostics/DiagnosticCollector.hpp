/// @file DiagnosticCollector.hpp
/// @brief Collects Forge diagnostics before report emission.

#pragma once

#include "Forge/Diagnostics/ForgeDiagnostic.hpp"
#include "Forge/Reports/BuildReport.hpp"

#include <cstddef>
#include <vector>

namespace Forge::Diagnostics
{

class DiagnosticCollector
{
public:
    void Add(ForgeDiagnostic diagnostic);

    [[nodiscard]] const std::vector<ForgeDiagnostic>& Diagnostics() const noexcept;
    [[nodiscard]] bool Empty() const noexcept;
    [[nodiscard]] std::size_t Count() const noexcept;
    [[nodiscard]] std::size_t CountBySeverity(ForgeDiagnosticSeverity severity) const;
    [[nodiscard]] std::size_t BuildBlockingCount() const;

    void AppendTo(Reports::BuildReport& report) const;

private:
    std::vector<ForgeDiagnostic> diagnostics_;
};

} // namespace Forge::Diagnostics
