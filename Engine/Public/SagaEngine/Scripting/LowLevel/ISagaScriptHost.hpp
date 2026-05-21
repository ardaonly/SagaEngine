/// @file ISagaScriptHost.hpp
/// @brief Low-level SagaScript host and world contracts.

#pragma once

#include "SagaEngine/Scripting/Namespace.hpp"
#include "SagaShared/Scripting/ScriptDiagnosticPayload.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace SagaEngine::Scripting
{

struct ScriptPackageHandle
{
    std::uint64_t value = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
};

struct ScriptInstanceHandle
{
    std::uint64_t value = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
};

struct ScriptEntityHandle
{
    std::uint64_t value = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
};

struct ScriptVector3
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct ScriptClassId
{
    std::string value;

    [[nodiscard]] bool IsValid() const noexcept { return !value.empty(); }
};

struct ScriptPackageLoadRequest
{
    std::filesystem::path packageRoot;
    std::filesystem::path scriptArtifactManifest;
    std::string packageId;
};

struct ScriptInstanceCreateRequest
{
    ScriptPackageHandle package;
    ScriptClassId classId;
    std::string scriptId;
    ScriptEntityHandle selfEntity;
};

enum class ScriptLifecycleMethod : std::uint8_t
{
    OnCreate,
    OnStart,
    OnUpdate,
    OnDestroy,
};

[[nodiscard]] constexpr std::string_view ToString(
    const ScriptLifecycleMethod method) noexcept
{
    switch (method)
    {
    case ScriptLifecycleMethod::OnCreate:
        return "OnCreate";
    case ScriptLifecycleMethod::OnStart:
        return "OnStart";
    case ScriptLifecycleMethod::OnUpdate:
        return "OnUpdate";
    case ScriptLifecycleMethod::OnDestroy:
        return "OnDestroy";
    }

    return "Unknown";
}

enum class ScriptLogLevel : std::uint8_t
{
    Info,
    Warn,
    Error,
};

[[nodiscard]] constexpr std::string_view ToString(
    const ScriptLogLevel level) noexcept
{
    switch (level)
    {
    case ScriptLogLevel::Info:
        return "Info";
    case ScriptLogLevel::Warn:
        return "Warn";
    case ScriptLogLevel::Error:
        return "Error";
    }

    return "Unknown";
}

struct ScriptLifecycleInvocation
{
    ScriptInstanceHandle instance;
    ScriptLifecycleMethod method = ScriptLifecycleMethod::OnCreate;
    double deltaTimeSeconds = 0.0;
};

using ScriptDiagnostic = SagaShared::Scripting::ScriptDiagnosticPayload;
using ScriptDiagnosticSink = std::function<void(const ScriptDiagnostic&)>;

struct ScriptHostOperationResult
{
    bool succeeded = false;
    std::vector<ScriptDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept { return succeeded; }
};

struct ScriptPackageLoadResult : ScriptHostOperationResult
{
    ScriptPackageHandle package;
};

struct ScriptInstanceCreateResult : ScriptHostOperationResult
{
    ScriptInstanceHandle instance;
};

struct ScriptEntityCreateResult : ScriptHostOperationResult
{
    ScriptEntityHandle entity;
};

struct ScriptEntityPositionResult : ScriptHostOperationResult
{
    ScriptVector3 position;
};

struct ScriptLogEvent
{
    ScriptInstanceHandle instance;
    std::string scriptId;
    ScriptLogLevel level = ScriptLogLevel::Info;
    std::string message;
};

using ScriptLogSink = std::function<void(const ScriptLogEvent&)>;

class ISagaScriptWorld
{
public:
    virtual ~ISagaScriptWorld() = default;

    [[nodiscard]] virtual ScriptEntityCreateResult CreateEntity(
        const std::string& name) = 0;

    [[nodiscard]] virtual ScriptHostOperationResult SetPosition(
        ScriptEntityHandle entity,
        ScriptVector3 position) = 0;

    [[nodiscard]] virtual ScriptEntityPositionResult GetPosition(
        ScriptEntityHandle entity) const = 0;
};

class ISagaScriptHost
{
public:
    virtual ~ISagaScriptHost() = default;

    [[nodiscard]] virtual ScriptPackageLoadResult LoadPackage(
        const ScriptPackageLoadRequest& request) = 0;

    [[nodiscard]] virtual ScriptInstanceCreateResult CreateInstance(
        ScriptPackageHandle package,
        const ScriptClassId& classId) = 0;

    [[nodiscard]] virtual ScriptInstanceCreateResult CreateInstance(
        const ScriptInstanceCreateRequest& request)
    {
        return CreateInstance(request.package, request.classId);
    }

    [[nodiscard]] virtual ScriptHostOperationResult InvokeLifecycle(
        const ScriptLifecycleInvocation& invocation) = 0;
};

namespace ScriptHostDiagnostics
{

inline constexpr const char* InvalidPackageHandle =
    "Script.Host.InvalidPackageHandle";
inline constexpr const char* InvalidInstanceHandle =
    "Script.Host.InvalidInstanceHandle";
inline constexpr const char* InvalidClassId =
    "Script.Host.InvalidClassId";
inline constexpr const char* ClassMissing =
    "Script.Host.ClassMissing";
inline constexpr const char* LifecycleFailed =
    "Script.Host.LifecycleFailed";
inline constexpr const char* ManifestMissing =
    "Script.Host.ManifestMissing";
inline constexpr const char* ManifestInvalid =
    "Script.Host.ManifestInvalid";
inline constexpr const char* ArtifactMissing =
    "Script.Host.ArtifactMissing";
inline constexpr const char* InvalidArtifactPath =
    "Script.Host.InvalidArtifactPath";
inline constexpr const char* PackageDestinationMismatch =
    "Script.Host.PackageDestinationMismatch";
inline constexpr const char* UnsupportedTargetFramework =
    "Script.Host.UnsupportedTargetFramework";
inline constexpr const char* InvalidArtifactHash =
    "Script.Host.InvalidArtifactHash";
inline constexpr const char* ArtifactHashMismatch =
    "Script.Host.ArtifactHashMismatch";
inline constexpr const char* InvalidAuthority =
    "Script.Host.InvalidAuthority";
inline constexpr const char* InvalidCapabilityMetadata =
    "Script.Host.InvalidCapabilityMetadata";
inline constexpr const char* CapabilityManifestMissing =
    "Script.Host.CapabilityManifestMissing";
inline constexpr const char* CapabilityManifestInvalid =
    "Script.Host.CapabilityManifestInvalid";
inline constexpr const char* CapabilityManifestMismatch =
    "Script.Host.CapabilityManifestMismatch";
inline constexpr const char* CapabilityGrantMissing =
    "Script.Host.CapabilityGrantMissing";
inline constexpr const char* CapabilityGrantUnexpected =
    "Script.Host.CapabilityGrantUnexpected";
inline constexpr const char* RuntimeConfigMissing =
    "Script.Host.RuntimeConfigMissing";
inline constexpr const char* RuntimeConfigInvalid =
    "Script.Host.RuntimeConfigInvalid";
inline constexpr const char* InstanceCreationFailed =
    "Script.Host.InstanceCreationFailed";
inline constexpr const char* ScriptCapabilityDenied =
    "Script.Host.ScriptCapabilityDenied";
inline constexpr const char* InvalidEntityHandle =
    "Script.Host.InvalidEntityHandle";
inline constexpr const char* ScriptWorldUnavailable =
    "Script.Host.ScriptWorldUnavailable";

} // namespace ScriptHostDiagnostics

} // namespace SagaEngine::Scripting
