/// @file RuntimeHost.cpp
/// @brief Implements standalone runtime host mode dispatch without client networking.

#include "RuntimeHost.h"

namespace SagaRuntimeApp
{

RuntimeHostResult RuntimeHost::Run() const
{
    RuntimeHostResult result;
    result.exitCode = 2;
    result.diagnosticId =
        RuntimeHostDiagnostics::LegacyConnectedClientUnsupported;
    result.message =
        "Connected-client mode is unsupported. Select a supported "
        "SagaRuntime project mode.";
    return result;
}

} // namespace SagaRuntimeApp
