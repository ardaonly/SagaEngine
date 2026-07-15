/// @file SagaSessionModel.cpp
/// @brief Product-level workspace session and launch target helpers.

#include "Projects/SagaSessionModel.h"

namespace SagaProduct
{

const char* ToString(SagaProductTargetKind kind) noexcept
{
    switch (kind)
    {
        case SagaProductTargetKind::Editor:  return "editor";
        case SagaProductTargetKind::Runtime: return "runtime";
        case SagaProductTargetKind::Server:  return "server";
    }
    return "editor";
}

bool ParseTargetKind(const std::string& text, SagaProductTargetKind& out) noexcept
{
    if (text == "editor")
    {
        out = SagaProductTargetKind::Editor;
        return true;
    }
    if (text == "runtime" || text == "client")
    {
        out = SagaProductTargetKind::Runtime;
        return true;
    }
    if (text == "server")
    {
        out = SagaProductTargetKind::Server;
        return true;
    }
    return false;
}

const char* ToString(SagaProductDiagnosticStage stage) noexcept
{
    switch (stage)
    {
        case SagaProductDiagnosticStage::Config:              return "config";
        case SagaProductDiagnosticStage::WorkspaceResolution: return "workspace_resolution";
        case SagaProductDiagnosticStage::ProjectValidation:   return "project_validation";
        case SagaProductDiagnosticStage::TargetPreparation:   return "target_preparation";
        case SagaProductDiagnosticStage::StartupHandoff:      return "startup_handoff";
    }
    return "unknown";
}

const char* ToString(SagaLaunchTargetAvailability availability) noexcept
{
    switch (availability)
    {
        case SagaLaunchTargetAvailability::Available: return "available";
        case SagaLaunchTargetAvailability::Bounded: return "bounded";
        case SagaLaunchTargetAvailability::Unsupported: return "unsupported";
    }
    return "unsupported";
}

} // namespace SagaProduct
