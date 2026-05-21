/// @file ScriptCapabilityDescriptor.hpp
/// @brief Shared SagaScript capability request and grant contract.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SagaShared::Scripting
{

/// High-level capability domains available to SagaScript validation and hosts.
enum class ScriptCapabilityDomain : std::uint8_t
{
    SharedPure,
    ClientPresentation,
    ClientRequest,
    ServerValidation,
    TrustedServerOnly,
    EditorTooling,
    BuildTooling,
    NativeExtension,
    EngineInternal,
};

/// Requested access from a script binding or artifact.
struct ScriptCapabilityRequest
{
    ScriptCapabilityDomain domain = ScriptCapabilityDomain::SharedPure; ///< Requested capability domain.
    std::string capabilityId;       ///< Stable capability identifier.
    bool required = true;           ///< True when the script cannot run without this capability.
    std::string rationale;          ///< Optional explanation for review and diagnostics.
};

/// Granted access after validation and package policy checks.
struct ScriptCapabilityGrant
{
    ScriptCapabilityDomain domain = ScriptCapabilityDomain::SharedPure; ///< Granted capability domain.
    std::string capabilityId;       ///< Stable capability identifier.
    bool granted = false;           ///< True when policy granted the capability.
    std::string policyId;           ///< Policy or profile that granted or denied the capability.
    std::string reason;             ///< Optional review or diagnostic reason.
};

/// Capability summary carried by script bindings, artifacts, and manifests.
struct ScriptCapabilityDescriptor
{
    std::vector<ScriptCapabilityRequest> requested; ///< Capabilities requested by source metadata.
    std::vector<ScriptCapabilityGrant> granted;     ///< Capabilities granted by validation or package policy.
    std::map<std::string, std::string> metadata;    ///< Serialization-safe extension metadata.
};

} // namespace SagaShared::Scripting
