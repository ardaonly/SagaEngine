// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <SagaRuntime/Play/RuntimePlayContracts.hpp>
#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace SagaRuntimeApp
{

struct RuntimeLaunchReadResult final
{
    SagaRuntime::Play::ResolvedLocalLaunchEnvelope envelope;
    std::vector<SagaRuntime::Play::RuntimeDiagnostic> diagnostics;
};

class RuntimeLaunchSession final
{
public:
    [[nodiscard]] static RuntimeLaunchReadResult ReadEnvelope(
        const std::filesystem::path& path);
    [[nodiscard]] static std::string ReadFile(const std::filesystem::path& path);
    [[nodiscard]] static bool WriteJson(const std::filesystem::path& path,
                                        const nlohmann::json& value);
    [[nodiscard]] static bool VerifyFile(
        const std::map<std::string, std::filesystem::path>& paths,
        const SagaRuntime::Play::ArtifactIdentity& identity,
        std::vector<SagaRuntime::Play::RuntimeDiagnostic>& diagnostics);
};

} // namespace SagaRuntimeApp
