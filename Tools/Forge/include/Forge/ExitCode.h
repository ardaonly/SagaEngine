/// @file ExitCode.h
/// @brief Stable Forge CLI exit code categories.

#pragma once

namespace Forge
{

enum class ExitCode
{
    Success = 0,
    UsageError = 1,
    ExecutionFailure = 2,
    StrictFailure = 3,
    ValidationFailure = 4,
    ToolchainFailure = 5,
    DependencyFailure = 6,
    InternalError = 70,
};

constexpr int ToInt(const ExitCode code) noexcept
{
    return static_cast<int>(code);
}

const char* ExitCodeName(ExitCode code) noexcept;

} // namespace Forge
