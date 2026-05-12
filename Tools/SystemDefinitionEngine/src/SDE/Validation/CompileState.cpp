/// @file CompileState.cpp
/// @brief CompileState severity ordering, merge logic, and human-readable names.

#include "SDE/Validation/CompileState.h"

namespace SDE
{

CompileState Merge(CompileState a, CompileState b) noexcept
{
    // Enumerators are ordered by severity; return the numerically larger one.
    return (static_cast<int>(a) >= static_cast<int>(b)) ? a : b;
}

bool IsUsable(CompileState s) noexcept
{
    return s == CompileState::Clean || s == CompileState::WithWarnings;
}

std::string StateName(CompileState s)
{
    switch (s)
    {
        case CompileState::Clean:            return "Clean";
        case CompileState::WithWarnings:     return "WithWarnings";
        case CompileState::UnresolvedRefs:   return "UnresolvedRefs";
        case CompileState::ValidationFailed: return "ValidationFailed";
        case CompileState::Aborted:          return "Aborted";
        case CompileState::IOError:          return "IOError";
    }
    return "Unknown";
}

} // namespace SDE
