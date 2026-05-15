/// @file SagaSessionModel.cpp
/// @brief Product-level workspace session and launch target helpers.

#include "SagaSessionModel.h"

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

} // namespace SagaProduct
