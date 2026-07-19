// SPDX-License-Identifier: MPL-2.0
/// @file PackageKind.hpp
/// @brief Shared package kind vocabulary.

#pragma once

#include <cstdint>

namespace SagaShared::Packages
{

/// Neutral package destination or purpose.
enum class PackageKind : std::uint8_t
{
    Client,
    Server,
    EditorEvaluation,
    Tool,
    Content,
    Shared,
};

} // namespace SagaShared::Packages
