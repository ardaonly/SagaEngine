/// @file Resolver.h
/// @brief Turn a raw registry value into a concrete executable path on disk.

#pragma once

#include "SagaTools/Registry.h"

#include <string>

namespace SagaTools
{

// ─── Resolution Result ────────────────────────────────────────────────────────

/// Outcome of attempting to resolve a registry value to a real executable.
/// Always populated — `available == false` is a normal, expected state and
/// the caller is responsible for printing the diagnostic fields.
struct Resolution
{
    bool        available = false;     ///< True iff `resolvedPath` exists and is a regular file.
    std::string resolvedPath;          ///< Absolute path on disk when available.
    std::string source;                ///< "registry-path" or "PATH".
    std::string expandedValue;         ///< Registry value after `${VAR}` substitution.
};

// ─── Resolver ─────────────────────────────────────────────────────────────────

/// Stateless query: registry + name → Resolution.
///
/// The dispatcher does not own a "resolver object" or maintain any cache —
/// every command is one-shot and resolves on demand. Keeps the dispatcher
/// trivially testable and lock-free.
class Resolver
{
public:
    [[nodiscard]] static Resolution Resolve(const Registry&   registry,
                                            const std::string& name) noexcept;
};

} // namespace SagaTools
