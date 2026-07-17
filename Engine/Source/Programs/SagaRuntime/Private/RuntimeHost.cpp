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
        "Generic project execution is unsupported until a validated runtime "
        "artifact launch contract is available.";
    return result;
}

} // namespace SagaRuntimeApp
