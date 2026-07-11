/// @file StarterArenaSmokeScript.h
/// @brief StarterArena script metadata, invocation, and lifecycle smoke helpers.

#pragma once

#include "StarterArenaSmoke.h"

#include <filesystem>
#include <vector>

namespace SagaRuntimeApp
{

[[nodiscard]] StarterArenaScriptBindingMetadata
LoadStarterArenaScriptBindingMetadata(
    const std::filesystem::path& manifestPath,
    const std::filesystem::path& artifactsPath,
    std::vector<StarterArenaDiagnostic>& diagnostics);

[[nodiscard]] StarterArenaScriptInvocationResult RunStarterArenaScriptInvocation(
    const StarterArenaProject& project,
    const StarterArenaScriptBindingMetadata& metadata,
    std::vector<StarterArenaDiagnostic>& diagnostics);

[[nodiscard]] StarterArenaScriptLifecycleResult RunStarterArenaScriptLifecycle(
    const StarterArenaProject& project,
    const StarterArenaScriptBindingMetadata& metadata,
    double fixedDtSeconds,
    std::vector<StarterArenaDiagnostic>& diagnostics);

} // namespace SagaRuntimeApp
