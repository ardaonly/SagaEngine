/// @file RuntimeModule.hpp
/// @brief Runtime role module descriptor for Saga product orchestration.

#pragma once

#include <string>

namespace SagaRuntime
{

/// Lightweight runtime module descriptor used to keep runtime ownership explicit.
struct RuntimeModuleDescriptor
{
    std::string moduleName = "SagaRuntimeModule"; ///< Stable product-facing module name.
    std::string roleName = "runtime";             ///< Product role represented by the module.
    bool headlessCapable = true;                   ///< Runtime can be launched without editor UI.
};

/// Return the default runtime module descriptor.
[[nodiscard]] RuntimeModuleDescriptor GetRuntimeModuleDescriptor();

} // namespace SagaRuntime
