/// @file ScriptAuthorityDescriptor.hpp
/// @brief Shared SagaScript authority and side-effect contract.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SagaShared::Scripting
{

/// Where script behavior is allowed to hold gameplay authority.
enum class ScriptAuthorityContext : std::uint8_t
{
    ClientOnly,
    ServerOnly,
    ClientPredicted,
    ServerValidated,
    Replicated,
    VisualOnly,
    EditorOnly,
    BuildOnly,
    ToolOnly,
    SharedPure,
};

/// Physical process/domain where a script artifact is intended to run.
enum class ScriptExecutionDomain : std::uint8_t
{
    ClientRuntime,
    ServerRuntime,
    EditorProcess,
    BuildProcess,
    ToolProcess,
    TestProcess,
};

/// Declared script side effects used by validation, package, and runtime gates.
enum class ScriptSideEffect : std::uint8_t
{
    Pure,
    ReadLocalState,
    WriteLocalState,
    ReadAuthoritativeState,
    WriteAuthoritativeState,
    ReadReplicatedState,
    WriteReplicatedState,
    SendClientEvent,
    SendServerRequest,
    SendReliableNetworkEvent,
    SendUnreliableNetworkEvent,
    ReadPersistentState,
    WritePersistentState,
    StartTransaction,
    CommitTransaction,
    AllocateRuntimeResource,
    ScheduleJob,
    ScheduleTimer,
    EmitDiagnostic,
    EditorMutation,
    BuildArtifactWrite,
};

/// Whether script behavior is safe to run speculatively on a client.
enum class ScriptPredictionSafety : std::uint8_t
{
    PredictionSafe,
    PredictionUnsafe,
    VisualOnly,
    ServerCorrectionRequired,
    DeterministicPure,
};

/// Trust boundary required before script behavior may execute.
enum class ScriptSecurityBoundary : std::uint8_t
{
    Pure,
    ClientPresentation,
    ClientRequest,
    ServerValidation,
    TrustedServerOnly,
    EditorTooling,
    BuildTooling,
    TestOnly,
};

/// Declares authority, execution, and side-effect requirements for a script.
struct ScriptAuthorityDescriptor
{
    ScriptAuthorityContext authorityContext = ScriptAuthorityContext::SharedPure; ///< Required authority context.
    ScriptExecutionDomain executionDomain = ScriptExecutionDomain::TestProcess;   ///< Intended execution domain.
    ScriptPredictionSafety predictionSafety = ScriptPredictionSafety::DeterministicPure; ///< Prediction policy.
    ScriptSecurityBoundary securityBoundary = ScriptSecurityBoundary::Pure;       ///< Required trust boundary.
    std::vector<ScriptSideEffect> sideEffects; ///< Declared side effects for validation and package gates.
    std::string replicationEffect;             ///< Optional replication policy or effect token.
    std::string persistenceEffect;             ///< Optional persistence policy or effect token.
    std::string validationProfile;             ///< Optional validation profile token.
    std::map<std::string, std::string> metadata; ///< Serialization-safe extension metadata.
};

} // namespace SagaShared::Scripting
