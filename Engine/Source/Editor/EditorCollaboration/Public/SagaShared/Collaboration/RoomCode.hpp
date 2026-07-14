/// @file RoomCode.hpp
/// @brief Room-code contract for quick collaboration join flows.

#pragma once

#include <string>

namespace SagaShared::Collaboration
{

/// Human-entered short code used to discover a collaboration session.
struct RoomCode
{
    std::string value; ///< Normalized room code; empty means unavailable.
};

[[nodiscard]] bool IsValid(const RoomCode& code) noexcept;

} // namespace SagaShared::Collaboration
