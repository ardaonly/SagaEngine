/// @file TargetPlatform.hpp
/// @brief Shared target platform descriptor.

#pragma once

#include <string>

namespace SagaShared::Build
{

/// Target platform token and optional human-readable label.
struct TargetPlatform
{
    std::string id;
    std::string displayName;
};

} // namespace SagaShared::Build
