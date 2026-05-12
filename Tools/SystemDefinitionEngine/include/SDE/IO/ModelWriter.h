/// @file ModelWriter.h
/// @brief Abstract writer interface and JSON-backed implementation for compiled graphs.

#pragma once

#include <string>

namespace SDE
{

class CompiledModelGraph;

// ─── ModelWriter ──────────────────────────────────────────────────────────────

/// Abstract interface for serializing a CompiledModelGraph to persistent storage.
class ModelWriter
{
public:
    virtual ~ModelWriter() = default;

    /// Write the compiled graph to the file at path.
    /// Stores the reason in outError (when non-null) on failure.
    [[nodiscard]] virtual bool Write(const CompiledModelGraph& graph,
                                     const std::string&        path,
                                     std::string*              outError) const = 0;
};

// ─── JsonModelWriter ──────────────────────────────────────────────────────────

/// Serializes a CompiledModelGraph to a human-readable JSON file.
/// Suitable for debug dumps and editor consumption.
class JsonModelWriter final : public ModelWriter
{
public:
    [[nodiscard]] bool Write(const CompiledModelGraph& graph,
                             const std::string&        path,
                             std::string*              outError) const override;
};

} // namespace SDE
