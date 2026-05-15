/// @file SessionId.hpp
/// @brief Stable collaboration session identifier contract.

#pragma once

#include <string>

namespace SagaShared::Collaboration
{

/// Serializable collaboration session identity wrapper.
struct SessionId
{
    std::string value; ///< Stable session id; empty means invalid.
};

[[nodiscard]] bool IsValid(const SessionId& id) noexcept;

} // namespace SagaShared::Collaboration
