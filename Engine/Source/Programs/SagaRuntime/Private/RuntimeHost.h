// SPDX-License-Identifier: MPL-2.0
/// @file RuntimeHost.h
/// @brief Declares the app-private host boundary for standalone runtime modes.

#pragma once

#include <filesystem>
#include <string>

namespace SagaRuntimeApp
{

namespace RuntimeHostDiagnostics
{

inline constexpr const char* PlayEnvelopeRequired =
    "Runtime.Host.PlayEnvelopeRequired";
inline constexpr const char* GenericProjectModeUnsupported =
    "Runtime.Host.GenericProjectModeUnsupported";

} // namespace RuntimeHostDiagnostics

/// Result returned after attempting to run the standalone runtime host.
struct RuntimeHostResult
{
    bool ok = false;              ///< True when the requested runtime mode ran successfully.
    int exitCode = 1;             ///< Process exit code for the host result.
    std::string diagnosticId;     ///< Stable diagnostic identifier when the host rejects a mode.
    std::string message;          ///< Operator-facing explanation of the result.
};

/// Owns app-private dispatch for generic standalone runtime behavior.
class RuntimeHost
{
public:
    RuntimeHost() = default;
    RuntimeHost(std::filesystem::path playEnvelope, bool headless,
                bool smokeInteractions);
    [[nodiscard]] RuntimeHostResult Run() const;

private:
    std::filesystem::path playEnvelope_;
    bool headless_ = false;
    bool smokeInteractions_ = false;
    bool legacyGenericMode_ = true;
};

} // namespace SagaRuntimeApp
