/// @file PackageId.hpp
/// @brief Shared stable package identifier.

#pragma once

#include <string>

namespace SagaShared::Packages
{

/// Stable package identifier used across build, publish, runtime, and reports.
struct PackageId
{
    std::string value;
};

} // namespace SagaShared::Packages
