/// @file ResourceType.hpp
/// @brief Defines explicitly tracked diagnostics resource kinds.

#pragma once

namespace SagaEngine::Diagnostics
{

/// Non-memory resource types tracked by explicit diagnostics registration.
enum class ResourceType
{
    Socket,
    File,
    Timer,
    Job,
    AssetHandle,
    DatabaseConnection,
    Thread,
    Other,
};

/// Convert a resource type to its stable report token.
[[nodiscard]] const char* ToString(ResourceType type) noexcept;

} // namespace SagaEngine::Diagnostics
