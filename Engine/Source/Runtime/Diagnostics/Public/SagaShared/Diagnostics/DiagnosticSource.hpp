/// @file DiagnosticSource.hpp
/// @brief Shared diagnostic producer vocabulary.

#pragma once

#include <cstdint>

namespace SagaShared::Diagnostics
{

/// Neutral producer families for diagnostics crossing module or process boundaries.
enum class DiagnosticSource : std::uint8_t
{
    Product,
    Editor,
    Runtime,
    Server,
    Shared,
    Collaboration,
    Forge,
    AssetPipeline,
    ScriptingToolchain,
    PackagePipeline,
    PublishPipeline,
    CI,
    Tool,
};

} // namespace SagaShared::Diagnostics
