/// @file PackageVersion.hpp
/// @brief Shared package version value.

#pragma once

#include <cstdint>
#include <string>

namespace SagaShared::Packages
{

/// Version metadata for package manifests without prescribing a serializer.
struct PackageVersion
{
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;
    std::string label;
};

} // namespace SagaShared::Packages
