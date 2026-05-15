/// @file CompileContext.h
/// @brief Stable high-level options for an SDE compile request.

#pragma once

#include "SDE/Core/Cancellation.h"

namespace SDE
{

struct CompileContext
{
    CancellationToken cancellation;
    bool              failFast = false;
};

} // namespace SDE
