/// @file ChaosProfile.hpp
/// @brief Defines SagaChaosLab deterministic chaos profile loading.

#pragma once

#include "SagaStressArena/StressTier.hpp"

#include "SagaEngine/Networking/NetworkChaosLayer.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace SagaChaosLab
{

inline constexpr std::uint32_t kChaosProfileSchemaVersion = 1;

/// Deterministic bounded chaos profile consumed by SagaChaosLab.
struct ChaosProfile
{
    std::uint32_t schemaVersion = kChaosProfileSchemaVersion;
    std::string profileName;
    std::string scenario;
    SagaStressArena::StressTier tier = SagaStressArena::StressTier::Smoke;
    bool manualOnly = false;
    std::uint64_t seed = 0;
    std::uint32_t actors = 0;
    std::uint32_t ticks = 0;
    std::uint32_t maxDurationSec = 0;
    std::filesystem::path reportDirectory;
    SagaEngine::Networking::NetworkChaosConfig chaosConfig;
};

/// Result of loading or validating a chaos profile.
struct ChaosProfileResult
{
    ChaosProfile profile;
    std::vector<std::string> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return diagnostics.empty();
    }
};

/// Return the built-in safe smoke profile used when no profile file is supplied.
[[nodiscard]] ChaosProfile DefaultSmokeChaosProfile();

/// Load and validate a chaos profile from JSON.
[[nodiscard]] ChaosProfileResult LoadChaosProfile(
    const std::filesystem::path& profilePath);

/// Validate a caller-created profile.
[[nodiscard]] ChaosProfileResult ValidateChaosProfile(
    const ChaosProfile& profile);

} // namespace SagaChaosLab
