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
    EditorPreview,
    Tool,
    Content,
    Shared,
};

} // namespace SagaShared::Packages
