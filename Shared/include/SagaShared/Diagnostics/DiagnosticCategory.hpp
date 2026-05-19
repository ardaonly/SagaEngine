/// @file DiagnosticCategory.hpp
/// @brief Shared top-level diagnostic category families.

#pragma once

#include <cstdint>

namespace SagaShared::Diagnostics
{

/// Top-level diagnostic category families documented by the diagnostics roadmap.
enum class DiagnosticCategory : std::uint8_t
{
    Project,
    Workspace,
    Editor,
    Profile,
    Graph,
    Authority,
    Script,
    Asset,
    SDE,
    Build,
    Package,
    Publish,
    Prism,
    Collaboration,
    Runtime,
    Server,
    Network,
    Replication,
    Prediction,
    Persistence,
    Security,
    Toolchain,
    DependencyBoundary,
    Crash,
    Shutdown,
    Performance,
    Memory,
};

} // namespace SagaShared::Diagnostics
