// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace SagaRuntimeApp
{
struct RuntimeGameplayResultContext
{
    std::filesystem::path resultPath;
    std::filesystem::path logPath;
    std::string playSessionId;
    std::string canonicalManifestHash;
    std::string scenarioIdentity;
    std::string packageGenerationId;
    std::string packageGenerationHash;
    std::string packageGraphHash;
    std::string collectibleImplementation;
};

[[nodiscard]] bool WriteRuntimeGameplayResult(
    const RuntimeGameplayResultContext& context,
    const std::vector<SagaRuntime::Play::RuntimeDiagnostic>& diagnostics,
    const std::string& status, const std::string& classification,
    std::uint64_t frames, std::uint64_t actions,
    const nlohmann::json& observed);
} // namespace SagaRuntimeApp
