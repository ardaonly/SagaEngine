/// @file ModelLoader.h
/// @brief Abstract loader interface, raw value tree, and model instance types.
///
/// RawValue is the format-agnostic in-memory representation of a parsed field value.
/// Every node carries its own SourceLocation so diagnostics are precise.
/// ModelLoader implementations translate file formats into ModelInstance collections;
/// they perform no validation or business logic.

#pragma once

#include "SDE/Validation/Diagnostic.h"

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace SDE
{

// ─── Raw value tree ───────────────────────────────────────────────────────────

struct RawValue; // forward decl for recursive types.

using RawNull    = std::monostate;
using RawInteger = int64_t;   ///< JSON integer (distinct from floating-point).
using RawNumber  = double;    ///< JSON floating-point number.
using RawBool    = bool;
using RawText    = std::string;

/// Named structs (not type aliases) so fields can be added later.
struct RawArray  { std::vector<RawValue> elements; };
struct RawObject { std::map<std::string, RawValue> fields; }; ///< map = deterministic iteration.

/// A single parsed value node. Carries its own source location at every nesting level
/// so nested field diagnostics have positions.
struct RawValue
{
    std::variant<RawNull,
                 RawInteger,
                 RawNumber,
                 RawBool,
                 RawText,
                 RawArray,
                 RawObject>  data;
    SourceLocation           location; ///< Points at this value in the source file.

    [[nodiscard]] bool IsNull()    const noexcept;
    [[nodiscard]] bool IsInteger() const noexcept;
    [[nodiscard]] bool IsNumber()  const noexcept; ///< True for Integer or floating-point.
    [[nodiscard]] bool IsText()    const noexcept;
    [[nodiscard]] bool IsBool()    const noexcept;
    [[nodiscard]] bool IsArray()   const noexcept;
    [[nodiscard]] bool IsObject()  const noexcept;

    /// Coerce to double regardless of Integer or Number variant.
    [[nodiscard]] double  AsDouble()  const;

    /// Return the integer value. Undefined behaviour when !IsInteger().
    [[nodiscard]] int64_t AsInteger() const;
};

// ─── ModelInstance ────────────────────────────────────────────────────────────

/// One parsed instance of a model (e.g., one item read from items.json).
/// Fields are a map of fieldId to RawValue (with embedded SourceLocation).
struct ModelInstance
{
    std::string                      modelId;     ///< Which ModelDefinition this is an instance of.
    std::string                      instanceId;  ///< Value of the "id" field.
    std::string                      sourceFile;
    int                              dataVersion = 1;
    SourceLocation                   origin;      ///< Points at the opening brace of this object.
    std::map<std::string, RawValue>  fields;      ///< fieldId → value (location inside RawValue).
};

// ─── ModelLoader ──────────────────────────────────────────────────────────────

/// Abstract interface for loading ModelInstances from a file.
///
/// Concrete loaders handle one file format (JSON, YAML, binary).
/// They produce ModelInstances even on partial failure; diagnostics carry the errors.
class ModelLoader
{
public:
    virtual ~ModelLoader() = default;

    /// Load all model instances from the file at path.
    [[nodiscard]] virtual std::vector<ModelInstance> Load(
        const std::string&       path,
        std::vector<Diagnostic>& outDiagnostics) const = 0;
};

} // namespace SDE
