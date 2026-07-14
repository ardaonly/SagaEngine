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
        RuntimeHostDiagnostics::GenericProjectModeUnsupported;
    result.message =
        "Generic project execution is unsupported. Select a bounded "
        "SagaRuntime mode such as StarterArena smoke or playable evidence.";
    return result;
}

} // namespace SagaRuntimeApp
