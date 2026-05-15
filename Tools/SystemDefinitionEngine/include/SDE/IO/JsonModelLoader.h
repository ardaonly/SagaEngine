/// @file JsonModelLoader.h
/// @brief JSON-backed ModelLoader using nlohmann/json.
///
/// nlohmann/json is NOT included in this header — it is isolated to the implementation
/// file. Nothing outside JsonModelLoader.cpp depends on nlohmann.

#pragma once

#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/ModelDefinition.h"

#include <optional>
#include <string>
#include <vector>

namespace SDE
{

class TypeRegistry;
class EnumRegistry;

// ─── JsonModelLoader ──────────────────────────────────────────────────────────

/// Loads ModelInstances and ModelDefinitions from JSON files.
///
/// Data file format:
///   { "modelId": "Item", "data": [ { "id": "sword_01", ... }, ... ] }
///
/// Schema file format:
///   { "id": "Item", "displayName": "Item", "schemaVersion": 1, "fields": [...] }
///
/// Field type strings: "Number", "Integer", "Text", "Boolean", "Color",
///   "Enum<EnumName>", "Ref<ModelId>", "List<T>", "Map<K,V>"
class JsonModelLoader final : public ModelLoader
{
public:
    /// Load model instances from a JSON data file.
    /// Emits a Fatal diagnostic and returns empty on I/O or parse failure.
    [[nodiscard]] std::vector<ModelInstance> Load(
        const std::string&       path,
        std::vector<Diagnostic>& outDiagnostics) const override;

    /// Load a ModelDefinition from a JSON schema file.
    /// Registers types into the provided TypeRegistry as it parses field type strings.
    [[nodiscard]] static std::optional<ModelDefinition> LoadDefinition(
        const std::string&       path,
        TypeRegistry&            types,
        std::vector<Diagnostic>& outDiagnostics,
        EnumRegistry*            enumRegistry = nullptr);
};

} // namespace SDE
