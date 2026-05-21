/// @file ScriptManifest.hpp
/// @brief Shared SagaScript manifest contracts.

#pragma once

#include "SagaShared/Diagnostics/DiagnosticSummary.hpp"
#include "SagaShared/Scripting/GeneratedCodeOriginDescriptor.hpp"
#include "SagaShared/Scripting/ScriptArtifactRef.hpp"
#include "SagaShared/Scripting/ScriptBindingDescriptor.hpp"
#include "SagaShared/Scripting/ScriptCapabilityDescriptor.hpp"
#include "SagaShared/Scripting/ScriptDiagnosticPayload.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SagaShared::Scripting
{

/// Collection of script bindings emitted by the scripting toolchain.
struct ScriptBindingManifest
{
    std::uint32_t schemaVersion = 1; ///< Manifest schema version.
    std::string manifestId;          ///< Stable manifest identifier.
    std::string sourceHash;          ///< Aggregate source hash used to produce the manifest.
    std::string toolchainId;         ///< Producing scripting toolchain id.
    std::string toolchainVersion;    ///< Producing scripting toolchain version.
    std::vector<ScriptBindingDescriptor> bindings; ///< Exposed script bindings.
    std::vector<ScriptDiagnosticPayload> diagnostics; ///< Binding and source diagnostics.
    Diagnostics::DiagnosticSummary diagnosticSummary; ///< Summary for quick gate checks.
    std::map<std::string, std::string> metadata; ///< Serialization-safe extension metadata.
};

/// Collection of script artifacts emitted by the scripting toolchain.
struct ScriptArtifactManifest
{
    std::uint32_t schemaVersion = 1; ///< Manifest schema version.
    std::string manifestId;          ///< Stable manifest identifier.
    std::string sourceHash;          ///< Aggregate source hash used to produce artifacts.
    std::string toolchainId;         ///< Producing scripting toolchain id.
    std::string toolchainVersion;    ///< Producing scripting toolchain version.
    std::vector<ScriptArtifactRef> artifacts; ///< Produced script artifacts.
    std::vector<GeneratedCodeOriginDescriptor> generatedOrigins; ///< Generated source origins.
    std::vector<ScriptDiagnosticPayload> diagnostics; ///< Artifact diagnostics.
    Diagnostics::DiagnosticSummary diagnosticSummary; ///< Summary for quick gate checks.
    std::map<std::string, std::string> metadata; ///< Serialization-safe extension metadata.
};

/// Collection of script capability requests and grants emitted by validation.
struct ScriptCapabilityManifest
{
    std::uint32_t schemaVersion = 1; ///< Manifest schema version.
    std::string manifestId;          ///< Stable manifest identifier.
    std::string sourceHash;          ///< Aggregate source hash used for capability validation.
    std::string policyId;            ///< Policy, profile, or package rule set used for validation.
    ScriptCapabilityDescriptor capabilities; ///< Aggregate requested and granted capabilities.
    std::vector<ScriptDiagnosticPayload> diagnostics; ///< Capability diagnostics.
    Diagnostics::DiagnosticSummary diagnosticSummary; ///< Summary for quick gate checks.
    std::map<std::string, std::string> metadata; ///< Serialization-safe extension metadata.
};

} // namespace SagaShared::Scripting
