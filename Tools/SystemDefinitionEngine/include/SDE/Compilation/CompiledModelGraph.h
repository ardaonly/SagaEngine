/// @file CompiledModelGraph.h
/// @brief Read-only output of the compilation pipeline.
///
/// CompiledModelGraph is move-only and immutable after ModelCompiler::Compile() returns.
/// References are resolved to CompiledInstanceRef handles. Collections are stored in a
/// contiguous ValueSlab (no recursive heap allocation per node).

#pragma once

#include "SDE/Validation/Diagnostic.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace SDE
{

class ModelCompiler;

// ─── CompiledInstanceRef ──────────────────────────────────────────────────────

/// Typed handle pointing at a specific compiled instance.
struct CompiledInstanceRef
{
    std::string modelId;
    std::string instanceId;
};

// ─── SlabRange ────────────────────────────────────────────────────────────────

/// Index range into a ValueSlab. [offset, offset+count).
struct SlabRange
{
    uint32_t offset = 0;
    uint32_t count  = 0;
};

// ─── CompiledValue ────────────────────────────────────────────────────────────

using CompiledNull    = std::monostate;
using CompiledInteger = int64_t;
using CompiledNumber  = double;
using CompiledBool    = bool;
using CompiledText    = std::string;

/// Array elements are stored contiguously in a ValueSlab; this holds the index range.
struct CompiledArrayRef  { SlabRange range; };

/// Object key-value pairs are stored as alternating Text/value pairs in a ValueSlab.
struct CompiledObjectRef { SlabRange range; };

/// One compiled value. Collections reference into a ValueSlab — no recursive heap.
struct CompiledValue
{
    std::variant<
        CompiledNull,
        CompiledInteger,
        CompiledNumber,
        CompiledBool,
        CompiledText,
        CompiledInstanceRef,
        CompiledArrayRef,
        CompiledObjectRef
    > data;

    [[nodiscard]] bool IsNull()   const noexcept;
    [[nodiscard]] bool IsUsable() const noexcept; ///< Not null.
};

// ─── ValueSlab ────────────────────────────────────────────────────────────────

/// Flat contiguous storage for all array and object values in a compiled graph.
///
/// Index-based access eliminates pointer chasing during traversal.
/// Only ModelCompiler may append to the slab.
class ValueSlab
{
public:
    [[nodiscard]] uint32_t            Count()             const noexcept;
    [[nodiscard]] const CompiledValue& At(uint32_t index) const;

private:
    friend class ModelCompiler;
    std::vector<CompiledValue> mSlots;

    /// Append a value; return its index.
    uint32_t Append(CompiledValue value);

    /// Reserve a contiguous run of count slots; return the starting index.
    SlabRange Reserve(uint32_t count);
};

// ─── CompiledInstance ─────────────────────────────────────────────────────────

/// One compiled model instance. All references resolved; all rules passed.
struct CompiledInstance
{
    std::string                          modelId;
    std::string                          instanceId;
    SourceLocation                       origin;  ///< Points back to source file/line.
    std::map<std::string, CompiledValue> fields;  ///< fieldId → compiled value.

    /// Return the field value, or nullptr when the field is absent.
    [[nodiscard]] const CompiledValue* GetField(const std::string& fieldId) const noexcept;
};

// ─── CompiledModelGraph ───────────────────────────────────────────────────────

/// The validated, reference-resolved output of ModelCompiler::Compile().
///
/// Move-only. Immutable after construction (only ModelCompiler writes to it).
/// All instances are keyed by (modelId, instanceId) — no global-unique-id assumption.
class CompiledModelGraph
{
public:
    CompiledModelGraph()                                        = default;
    CompiledModelGraph(CompiledModelGraph&&)                    = default;
    CompiledModelGraph& operator=(CompiledModelGraph&&)         = default;
    CompiledModelGraph(const CompiledModelGraph&)               = delete;
    CompiledModelGraph& operator=(const CompiledModelGraph&)    = delete;

    /// Look up an instance by model id and instance id. Returns nullptr when absent.
    [[nodiscard]] const CompiledInstance* Find(const std::string& modelId,
                                                const std::string& instanceId) const noexcept;

    /// Return pointers to all instances of the given model kind.
    [[nodiscard]] std::vector<const CompiledInstance*> AllOf(const std::string& modelId) const;

    /// Return all model ids present in this graph.
    [[nodiscard]] std::vector<std::string> ModelIds() const;

    /// Total number of compiled instances across all model kinds.
    [[nodiscard]] std::size_t TotalCount() const noexcept;

    /// Access the value slab for array/object traversal.
    [[nodiscard]] const ValueSlab& Slab() const noexcept;

private:
    friend class ModelCompiler;

    // Compound key: (modelId, instanceId) — avoids cross-model id collision.
    struct PairHash
    {
        std::size_t operator()(const std::pair<std::string, std::string>& k) const noexcept;
    };

    using InstanceKey = std::pair<std::string, std::string>;
    std::unordered_map<InstanceKey, CompiledInstance, PairHash> mInstances;
    ValueSlab                                                    mSlab;

    void AddInstance(CompiledInstance instance);
};

} // namespace SDE
