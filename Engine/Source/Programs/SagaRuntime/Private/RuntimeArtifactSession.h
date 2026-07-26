// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <filesystem>
#include <map>
#include <vector>

namespace SagaRuntimeApp
{

class RuntimeArtifactSession final
{
public:
    [[nodiscard]] static bool VerifyFiles(
        const std::map<std::string, std::filesystem::path>& paths,
        const std::vector<SagaRuntime::Play::ArtifactIdentity>& identities,
        std::vector<SagaRuntime::Play::RuntimeDiagnostic>& diagnostics);
};

} // namespace SagaRuntimeApp
