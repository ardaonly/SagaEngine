/// @file EnumDefinition.h
/// @brief Closed set of named values referenced by EnumType fields.

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace SDE
{

// ─── EnumMember ───────────────────────────────────────────────────────────────

/// One named member of an enum.
struct EnumMember
{
    std::string name;
    std::string value; ///< String representation; empty means use positional index.
};

// ─── EnumDefinition ───────────────────────────────────────────────────────────

/// A closed set of named values. Loaded once and referenced by id everywhere.
struct EnumDefinition
{
    std::string              id;
    std::string              displayName;
    std::vector<EnumMember>  members;

    /// Return true when a member with the given name exists.
    [[nodiscard]] bool ContainsMember(const std::string& name) const noexcept;

    /// Return the member with the given name, or nullopt.
    [[nodiscard]] std::optional<EnumMember> FindMember(const std::string& name) const noexcept;
};

} // namespace SDE
