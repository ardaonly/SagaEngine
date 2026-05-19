/// @file ExitCode.cpp
/// @brief Stable Forge CLI exit code category names.

#include "Forge/ExitCode.h"

namespace Forge
{

const char* ExitCodeName(const ExitCode code) noexcept
{
    switch (code)
    {
        case ExitCode::Success:
            return "Success";
        case ExitCode::UsageError:
            return "UsageError";
        case ExitCode::ExecutionFailure:
            return "ExecutionFailure";
        case ExitCode::StrictFailure:
            return "StrictFailure";
        case ExitCode::ValidationFailure:
            return "ValidationFailure";
        case ExitCode::ToolchainFailure:
            return "ToolchainFailure";
        case ExitCode::DependencyFailure:
            return "DependencyFailure";
        case ExitCode::InternalError:
            return "InternalError";
    }

    return "Unknown";
}

} // namespace Forge
