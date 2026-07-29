// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SagaRuntime::Play
{
struct RuntimeDiagnostic
{
    std::string code;
    std::string message;
    std::string subject;
};

struct ArtifactIdentity
{
    std::string logicalId;
    std::string hash;
};

struct RuntimeBridgeIdentity
{
    std::string logicalReferenceId;
    std::string assemblyHash;
    std::string abi;
    std::string targetFramework;
    std::string runtimeCompatibility;
    std::string trust;
};

struct CanonicalRuntimeLaunchManifest
{
    std::uint32_t schemaVersion = 1;
    std::string projectId;
    std::string projectHash;
    std::string playSessionId;
    std::string runtimeProfile = "Client";
    ArtifactIdentity scene;
    ArtifactIdentity input;
    ArtifactIdentity uiSet;
    ArtifactIdentity scriptAssembly;
    ArtifactIdentity scriptRuntimeConfig;
    ArtifactIdentity scriptAssemblyManifest;
    ArtifactIdentity scriptReferenceManifest;
    ArtifactIdentity behaviors;
    ArtifactIdentity namedActions;
    ArtifactIdentity capabilities;
    ArtifactIdentity packageGeneration;
    std::string packageGraphHash;
    std::string scriptSourceRevision;
    std::string scriptSourceSetHash;
    RuntimeBridgeIdentity runtimeBridge;
    std::vector<std::string> requiredRuntimeCapabilities;
    std::map<std::string, std::string> toolVersions;
};

struct CanonicalManifestResult
{
    std::optional<CanonicalRuntimeLaunchManifest> manifest;
    std::string canonicalBytes;
    std::string hash;
    std::vector<RuntimeDiagnostic> diagnostics;
    [[nodiscard]] bool Succeeded() const noexcept
    { return manifest.has_value() && diagnostics.empty(); }
};

[[nodiscard]] bool IsSha256(std::string_view value) noexcept;
[[nodiscard]] std::string Sha256Bytes(std::string_view bytes);
[[nodiscard]] std::string SerializeCanonical(
    const CanonicalRuntimeLaunchManifest& manifest);
[[nodiscard]] CanonicalManifestResult ParseCanonicalManifest(
    std::string_view bytes, std::string_view expectedHash = {});

struct RuntimeStartupReadyV1
{
    std::uint32_t schemaVersion = 1;
    std::string playSessionId;
    std::string canonicalLaunchManifestHash;
    std::string status = "Ready";
};

struct RuntimeStartupReadyParseResult
{
    std::optional<RuntimeStartupReadyV1> ready;
    std::vector<RuntimeDiagnostic> diagnostics;
    [[nodiscard]] bool Valid() const noexcept
    { return ready.has_value() && diagnostics.empty(); }
};

[[nodiscard]] std::string SerializeRuntimeStartupReadyV1(
    const RuntimeStartupReadyV1& ready);
[[nodiscard]] RuntimeStartupReadyParseResult ParseRuntimeStartupReadyV1(
    std::string_view bytes, std::string_view expectedPlaySessionId = {},
    std::string_view expectedCanonicalManifestHash = {});

struct RuntimeResultV1
{
    std::uint32_t schemaVersion = 1;
    std::string playSessionId;
    std::string canonicalLaunchManifestHash;
    std::string status;
    std::string exitClassification;
    int exitCode = -1;
    std::string shutdownClassification;
    std::vector<RuntimeDiagnostic> startupDiagnostics;
    std::vector<RuntimeDiagnostic> runtimeDiagnostics;
    std::uint64_t framesExecuted = 0;
    std::uint64_t interactionActionCount = 0;
    std::string observedStateCanonicalJson;
    std::optional<std::string> logPath;
    std::optional<std::string> crashArtifactPath;

    [[nodiscard]] bool Successful() const noexcept
    { return status == "Succeeded" && exitCode == 0; }
};

struct RuntimeResultParseResult
{
    std::optional<RuntimeResultV1> result;
    std::vector<RuntimeDiagnostic> diagnostics;
    [[nodiscard]] bool Valid() const noexcept
    { return result.has_value() && diagnostics.empty(); }
};

[[nodiscard]] RuntimeResultParseResult ParseRuntimeResultV1(
    std::string_view bytes, std::string_view expectedPlaySessionId = {},
    std::string_view expectedCanonicalManifestHash = {});

struct ResolvedLocalLaunchEnvelope
{
    std::uint32_t schemaVersion = 1;
    std::string playSessionId;
    std::filesystem::path canonicalManifestPath;
    std::string canonicalManifestHash;
    std::filesystem::path runtimeExecutable;
    std::filesystem::path workingDirectory;
    std::map<std::string, std::filesystem::path> artifacts;
    std::map<std::string, std::string> environment;
    std::filesystem::path resultPath;
    std::filesystem::path logPath;
    std::filesystem::path stopPath;
    std::filesystem::path startupReadyPath;
};

[[nodiscard]] std::vector<RuntimeDiagnostic> ValidateLocalEnvelope(
    const ResolvedLocalLaunchEnvelope& envelope,
    const std::vector<std::filesystem::path>& approvedRoots);

struct RuntimeTransform
{
    std::array<double, 3> position{0.0, 0.0, 0.0};
    std::array<double, 4> rotation{0.0, 0.0, 0.0, 1.0};
    std::array<double, 3> scale{1.0, 1.0, 1.0};
};

struct RuntimeMeshRenderer
{
    ArtifactIdentity mesh;
    ArtifactIdentity material;
};

struct RuntimeScriptBehavior
{
    std::string behaviorTypeId;
    std::string scriptArtifactLogicalId;
    std::vector<std::string> requiredCapabilities;
    std::string instanceConfigurationCanonicalJson;
};

struct RuntimeScriptActionBinding
{
    std::string inputActionId;
    std::string behaviorTypeId;
    std::string namedActionId;
};

struct RuntimeTriggerParticipant
{
    std::array<double, 3> bounds{1.0, 1.0, 1.0};
    std::string category;
};

struct RuntimeTriggerVolume
{
    std::array<double, 3> bounds{1.0, 1.0, 1.0};
    bool enabled = true;
    std::string acceptedCategory;
};

struct RuntimeScriptEventBinding
{
    std::string eventType;
    std::string sourceEntityId;
    std::string targetEntityId;
    std::string behaviorTypeId;
    std::string namedActionId;
};

struct RuntimeSceneEntity
{
    std::string entityId;
    std::string name;
    std::optional<std::string> parentEntityId;
    RuntimeTransform transform;
    std::optional<RuntimeMeshRenderer> meshRenderer;
    std::optional<RuntimeScriptBehavior> scriptBehavior;
    std::vector<RuntimeScriptActionBinding> actionBindings;
    std::optional<RuntimeTriggerParticipant> triggerParticipant;
    std::optional<RuntimeTriggerVolume> triggerVolume;
    std::vector<RuntimeScriptEventBinding> eventBindings;
    bool active = true;
    bool primaryCamera = false;
    double verticalFovDegrees = 60.0;
    double nearClip = 0.1;
    double farClip = 1000.0;
    bool directionalLight = false;
    std::array<double, 3> lightColor{1.0, 1.0, 1.0};
    double lightIntensity = 1.0;
    bool castsShadows = false;
};

struct RuntimeSceneArtifact
{
    std::uint32_t schemaVersion = 1;
    std::string artifactId;
    std::string contentHash;
    std::string sourceHash;
    std::vector<RuntimeSceneEntity> entities;
};

struct RuntimePhysicalBinding
{
    std::string device;
    std::string control;
    std::string displayLabel;
    std::string displayName;
};

struct RuntimeInputAction
{
    std::string actionId;
    std::string displayName;
    std::string runtimeRole;
    std::vector<RuntimePhysicalBinding> bindings;
};

struct RuntimeInputMapArtifact
{
    std::string artifactId;
    std::string contentHash;
    std::string sourceHash;
    std::vector<RuntimeInputAction> actions;
};

struct RuntimeUiResource
{
    std::string sourceId;
    std::filesystem::path path;
    std::string hash;
    std::string documentId;
    std::string family;
    std::string visibilityBehaviorId;
    std::string visibilityEntityId;
    std::string visibilityStateKey;
    std::string visibilityValueType;
    std::vector<std::string> visibilityEqualsAny;
};

struct RuntimeUiPromptBinding
{
    std::string actionId;
    std::string behaviorId;
    std::string elementId;
    std::string entityId;
    std::string falseLabel;
    std::string stateKey;
    std::string trueLabel;
    std::string visibility;
};

struct RuntimeUiSetArtifact
{
    std::uint32_t schemaVersion = 1;
    std::string artifactId;
    std::string contentHash;
    std::string sourceHash;
    std::string aggregateManifestHash;
    std::string screenId;
    std::vector<RuntimeUiResource> documents;
    std::vector<RuntimeUiResource> stylesheets;
    std::vector<RuntimeUiResource> fonts;
    std::vector<RuntimeUiPromptBinding> prompts;
    struct TypedTextBinding
    {
        std::string behaviorId;
        std::string documentId;
        std::string elementId;
        std::string entityId;
        std::string format;
        std::string stateKey;
        std::string valueType;
    };
    struct InputLabelBinding
    {
        std::string actionId;
        std::string documentId;
        std::string elementId;
        std::string format;
    };
    std::vector<TypedTextBinding> textBindings;
    std::vector<InputLabelBinding> inputLabelBindings;
};

template <typename T>
struct ArtifactLoadResult
{
    std::optional<T> value;
    std::vector<RuntimeDiagnostic> diagnostics;
    [[nodiscard]] bool Succeeded() const noexcept
    { return value.has_value() && diagnostics.empty(); }
};

[[nodiscard]] ArtifactLoadResult<RuntimeSceneArtifact> LoadRuntimeSceneArtifact(
    const std::filesystem::path& path);
[[nodiscard]] ArtifactLoadResult<RuntimeInputMapArtifact> LoadRuntimeInputMapArtifact(
    const std::filesystem::path& path);
[[nodiscard]] ArtifactLoadResult<RuntimeUiSetArtifact> LoadRuntimeUiSetArtifact(
    const std::filesystem::path& path);

class INamedActionTarget
{
public:
    virtual ~INamedActionTarget() = default;
    [[nodiscard]] virtual bool InvokeNamedAction(
        std::string_view behaviorTypeId, std::string_view namedActionId,
        std::string_view entityId) = 0;
};

class ScriptActionBindingDispatcher
{
public:
    explicit ScriptActionBindingDispatcher(INamedActionTarget& target) : target_(target) {}
    [[nodiscard]] std::vector<RuntimeDiagnostic> Build(const RuntimeSceneArtifact& scene);
    [[nodiscard]] std::vector<RuntimeDiagnostic> Dispatch(std::string_view inputActionId);
    [[nodiscard]] std::uint64_t DispatchCount() const noexcept { return dispatchCount_; }

private:
    struct BoundAction
    {
        std::string entityId;
        std::string behaviorTypeId;
        std::string namedActionId;
    };
    INamedActionTarget& target_;
    std::map<std::string, std::vector<BoundAction>> bindings_;
    std::uint64_t dispatchCount_ = 0;
};

class ScriptEventBindingDispatcher
{
public:
    explicit ScriptEventBindingDispatcher(INamedActionTarget& target) : target_(target) {}
    [[nodiscard]] std::vector<RuntimeDiagnostic> Build(const RuntimeSceneArtifact& scene);
    [[nodiscard]] std::vector<RuntimeDiagnostic> Dispatch(
        std::string_view eventType, std::string_view sourceEntityId);
    void Reset() noexcept { dispatchCount_ = 0; }
    [[nodiscard]] std::uint64_t DispatchCount() const noexcept { return dispatchCount_; }

private:
    struct BoundEvent
    {
        std::string targetEntityId;
        std::string behaviorTypeId;
        std::string namedActionId;
    };
    INamedActionTarget& target_;
    std::map<std::pair<std::string, std::string>, std::vector<BoundEvent>> bindings_;
    std::uint64_t dispatchCount_ = 0;
};
} // namespace SagaRuntime::Play
