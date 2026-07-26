// SPDX-License-Identifier: MPL-2.0

#include "RuntimeHost.h"

#include "RuntimeFrameLoop.h"

namespace SagaRuntimeApp
{

RuntimeHost::RuntimeHost(std::filesystem::path playEnvelope, bool headless,
                         bool smokeInteractions)
    : playEnvelope_(std::move(playEnvelope))
    , headless_(headless)
    , smokeInteractions_(smokeInteractions)
    , legacyGenericMode_(false)
{
}

RuntimeHostResult RuntimeHost::Run() const
{
    return RunRuntimeFrameLoop(
        playEnvelope_, headless_, smokeInteractions_, legacyGenericMode_);
}

} // namespace SagaRuntimeApp
