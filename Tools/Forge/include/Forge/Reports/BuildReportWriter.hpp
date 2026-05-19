/// @file BuildReportWriter.hpp
/// @brief Deterministic JSON writer for Forge build reports.

#pragma once

#include "Forge/Reports/BuildReport.hpp"

#include <iosfwd>

namespace Forge::Reports
{

class BuildReportWriter
{
public:
    static void WriteJson(const BuildReport& report, std::ostream& os);
};

} // namespace Forge::Reports
