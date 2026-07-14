/// @file ParticipantId.hpp
/// @brief Stable participant identifier for collaboration contracts.

#pragma once

#include <string>

namespace SagaShared::Collaboration
{

/// Serializable participant identity wrapper.
struct ParticipantId
{
    std::string value; ///< Stable participant id; empty means invalid.
};

[[nodiscard]] bool IsValid(const ParticipantId& id) noexcept;

} // namespace SagaShared::Collaboration
