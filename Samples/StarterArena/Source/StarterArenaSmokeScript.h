/// @file StarterArenaSmokeScript.h
/// @brief StarterArena script metadata, invocation, and lifecycle smoke helpers.

#pragma once

#include "StarterArenaSmoke.h"
#include "StarterArenaGameplayState.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace SagaRuntimeApp
{

class StarterArenaGameplaySession
{
public:
    ~StarterArenaGameplaySession();
    StarterArenaGameplaySession(StarterArenaGameplaySession&&) noexcept;
    StarterArenaGameplaySession& operator=(StarterArenaGameplaySession&&) noexcept;
    [[nodiscard]] bool Update(std::uint32_t tick, StarterArenaVec2 playerPosition,
                              double fixedDtSeconds,
                              std::vector<StarterArenaDiagnostic>& diagnostics);
    [[nodiscard]] bool Shutdown(std::vector<StarterArenaDiagnostic>& diagnostics);
private:
    struct Impl;
    explicit StarterArenaGameplaySession(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
    friend std::unique_ptr<StarterArenaGameplaySession> CreateStarterArenaGameplaySession(
        const StarterArenaProject&, const StarterArenaScriptBindingMetadata&,
        StarterArenaGameplayState&, std::vector<StarterArenaDiagnostic>&);
};

[[nodiscard]] std::unique_ptr<StarterArenaGameplaySession>
CreateStarterArenaGameplaySession(const StarterArenaProject& project,
                                  const StarterArenaScriptBindingMetadata& metadata,
                                  StarterArenaGameplayState& state,
                                  std::vector<StarterArenaDiagnostic>& diagnostics);

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
