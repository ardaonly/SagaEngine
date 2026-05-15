/// @file SagaProductHost.cpp
/// @brief Product host boundary implementation for Saga role targets.

#include "SagaProductHost.h"

namespace SagaProduct
{

SagaPreparedTarget SagaProductHost::PrepareTarget(
    const SagaSessionModel& session) const
{
    SagaPreparedTarget prepared;
    prepared.kind = session.target;
    prepared.workspace = session.workspace;

    switch (session.target)
    {
        case SagaProductTargetKind::Editor:
            prepared.moduleName = "SagaEditorModule";
            break;
        case SagaProductTargetKind::Runtime:
            prepared.moduleName = "SagaRuntimeModule";
            break;
        case SagaProductTargetKind::Server:
            prepared.moduleName = "SagaServerModule";
            break;
    }
    prepared.executableName = "Saga";

    return prepared;
}

} // namespace SagaProduct
