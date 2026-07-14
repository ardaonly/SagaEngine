/// @file ScriptId.hpp
/// @brief Shared SagaScript identifier contract.

#pragma once

#include <string>

namespace SagaShared::Scripting
{

/// Stable script resource identifier used across authoring, tooling, and packages.
struct ScriptId
{
    std::string value; ///< Stable identifier value.
};

} // namespace SagaShared::Scripting
