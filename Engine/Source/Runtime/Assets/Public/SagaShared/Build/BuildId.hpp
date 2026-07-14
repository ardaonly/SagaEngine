/// @file BuildId.hpp
/// @brief Shared stable build identifier.

#pragma once

#include <string>

namespace SagaShared::Build
{

/// Stable identifier for a build invocation or report.
struct BuildId
{
    std::string value;
};

} // namespace SagaShared::Build
