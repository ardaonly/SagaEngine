/// @file BuildConfiguration.hpp
/// @brief Shared build configuration metadata.

#pragma once

#include <map>
#include <string>

namespace SagaShared::Build
{

/// Neutral configuration tokens for a build profile or invocation.
struct BuildConfiguration
{
    std::string configuration;
    std::string validationStrictness;
    std::map<std::string, std::string> options;
};

} // namespace SagaShared::Build
