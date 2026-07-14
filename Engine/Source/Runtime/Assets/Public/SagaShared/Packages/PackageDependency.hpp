/// @file PackageDependency.hpp
/// @brief Shared package dependency reference.

#pragma once

#include "SagaShared/Packages/PackageId.hpp"
#include "SagaShared/Packages/PackageVersion.hpp"

#include <string>

namespace SagaShared::Packages
{

/// Package dependency metadata used by package manifests and publish reports.
struct PackageDependency
{
    PackageId packageId;
    PackageVersion version;
    std::string relationship; ///< Stable relationship token, empty when unspecified.
    bool required = true;
};

} // namespace SagaShared::Packages
