// SPDX-License-Identifier: MPL-2.0

#include "RuntimeArtifactSession.h"

#include "RuntimeLaunchSession.h"

namespace SagaRuntimeApp
{

bool RuntimeArtifactSession::VerifyFiles(
    const std::map<std::string, std::filesystem::path>& paths,
    const std::vector<SagaRuntime::Play::ArtifactIdentity>& identities,
    std::vector<SagaRuntime::Play::RuntimeDiagnostic>& diagnostics)
{
    bool valid = true;
    for (const auto& identity : identities)
        valid = RuntimeLaunchSession::VerifyFile(paths, identity, diagnostics) && valid;
    return valid;
}

} // namespace SagaRuntimeApp
