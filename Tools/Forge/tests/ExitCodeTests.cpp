/// @file ExitCodeTests.cpp
/// @brief Stable coverage for Forge CLI exit code categories.

#include "Forge/ExitCode.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

bool Expect(bool condition, const char* label)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "[ExitCodeTests] failed: " << label << "\n";
    return false;
}

bool ExpectCode(const Forge::ExitCode code, const int expected, const char* label)
{
    return Expect(Forge::ToInt(code) == expected, label);
}

bool ExpectName(const Forge::ExitCode code, const char* expected, const char* label)
{
    return Expect(std::string(Forge::ExitCodeName(code)) == expected, label);
}

} // namespace

int main()
{
    bool ok = true;

    ok &= ExpectCode(Forge::ExitCode::Success, 0, "Success is 0");
    ok &= ExpectCode(Forge::ExitCode::UsageError, 1, "UsageError is 1");
    ok &= ExpectCode(Forge::ExitCode::ExecutionFailure, 2, "ExecutionFailure is 2");
    ok &= ExpectCode(Forge::ExitCode::StrictFailure, 3, "StrictFailure is 3");
    ok &= ExpectCode(Forge::ExitCode::ValidationFailure, 4, "ValidationFailure is 4");
    ok &= ExpectCode(Forge::ExitCode::ToolchainFailure, 5, "ToolchainFailure is 5");
    ok &= ExpectCode(Forge::ExitCode::DependencyFailure, 6, "DependencyFailure is 6");
    ok &= ExpectCode(Forge::ExitCode::InternalError, 70, "InternalError is 70");

    ok &= ExpectName(Forge::ExitCode::Success, "Success", "Success name");
    ok &= ExpectName(Forge::ExitCode::UsageError, "UsageError", "UsageError name");
    ok &= ExpectName(Forge::ExitCode::ExecutionFailure,
                     "ExecutionFailure",
                     "ExecutionFailure name");
    ok &= ExpectName(Forge::ExitCode::StrictFailure, "StrictFailure", "StrictFailure name");
    ok &= ExpectName(Forge::ExitCode::ValidationFailure,
                     "ValidationFailure",
                     "ValidationFailure name");
    ok &= ExpectName(Forge::ExitCode::ToolchainFailure,
                     "ToolchainFailure",
                     "ToolchainFailure name");
    ok &= ExpectName(Forge::ExitCode::DependencyFailure,
                     "DependencyFailure",
                     "DependencyFailure name");
    ok &= ExpectName(Forge::ExitCode::InternalError,
                     "InternalError",
                     "InternalError name");
    ok &= ExpectName(static_cast<Forge::ExitCode>(999), "Unknown", "unknown name");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
