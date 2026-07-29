// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostImpl.hpp
/// @brief Private state and implementation declarations for CSharpScriptHost.

#pragma once

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <array>
#include <coreclr_delegates.h>
#include <cstdint>
#include <filesystem>
#include <hostfxr.h>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace SagaEngine::Scripting
{

inline constexpr std::string_view kArtifactsManifestFileName = "script_artifacts.json";
inline constexpr std::string_view kBridgeTypeName =
    "SagaEngine.Scripting.RuntimeBridge.NativeBridge, SagaScript.RuntimeBridge";
inline constexpr std::size_t kBridgeErrorBufferSize = 2048;
inline constexpr std::int32_t kNativeCallbackTableVersion = 3;
inline constexpr std::string_view kCapabilityCreateEntity = "Gameplay.World.CreateEntity";
inline constexpr std::string_view kCapabilityTransformRead = "Gameplay.Transform.Read";
inline constexpr std::string_view kCapabilityTransformWrite = "Gameplay.Transform.Write";
inline constexpr std::string_view kCapabilityTransformReadWrite = "World.Transform.ReadWrite";
inline constexpr std::string_view kCapabilityStatePort = "Gameplay.State.ReadWrite";
inline constexpr std::string_view kCapabilityConfiguration = "Gameplay.Configuration.Read";
inline constexpr std::string_view kCapabilityInput = "Gameplay.Input.Read";
inline constexpr std::string_view kCapabilityEntityLookup = "Gameplay.World.FindEntity";
inline constexpr std::string_view kCapabilityEntityActive = "Gameplay.Entity.Active";
inline constexpr std::string_view kCapabilityRuntimeExit = "Runtime.Exit";

enum class BridgeStatus : int
{
    Ok = 0,
    InvalidArgument = 1,
    AssemblyLoadFailed = 2,
    ClassMissing = 3,
    ClassInvalid = 4,
    InstanceCreationFailed = 5,
    LifecycleMethodMissing = 6,
    LifecycleFailed = 7,
    ManagedException = 8,
    UiNamedActionMethodMissing = 9,
    UiNamedActionInvalidSignature = 10,
    UiNamedActionReturnedFalse = 11,
    Int32BinaryMethodMissing = 12,
    Int32BinaryInvalidSignature = 13,
    NamedActionMethodMissing = 14,
    NamedActionInvalidContract = 15,
};

enum class NativeCallbackStatus : int
{
    Ok = 0,
    InvalidArgument = 1,
    CapabilityDenied = 2,
    WorldUnavailable = 3,
    InvalidEntity = 4,
    OperationFailed = 5,
};

using NativeLogCallbackFn = int (*)(std::int64_t, int, const char*);
using NativeCreateEntityCallbackFn = int (*)(std::int64_t, const char*, std::int64_t*);
using NativeSetPositionCallbackFn = int (*)(std::int64_t, std::int64_t, float, float, float);
using NativeGetPositionCallbackFn = int (*)(std::int64_t, std::int64_t, float*, float*, float*);
using NativeGetStateBoolCallbackFn = int (*)(std::int64_t, const char*, int*);
using NativeAddStateInt32CallbackFn = int (*)(std::int64_t,
                                              const char*,
                                              std::int32_t,
                                              std::int32_t*);
using NativeSetStateBoolCallbackFn = int (*)(std::int64_t, const char*, int);
using NativeSetStateStringCallbackFn = int (*)(std::int64_t, const char*, const char*);
using NativeStatePortAvailableCallbackFn = int (*)(std::int64_t);
using NativeGetInt32CallbackFn = int (*)(std::int64_t, const char*, std::int32_t*);
using NativeGetDoubleCallbackFn = int (*)(std::int64_t, const char*, double*);
using NativeGetStringCallbackFn = int (*)(std::int64_t, const char*, char*, int);
using NativeQueryInputCallbackFn = int (*)(std::int64_t, const char*, int, int*);
using NativeFindEntityCallbackFn = int (*)(std::int64_t, const char*, std::int64_t*);
using NativeSetEntityActiveCallbackFn = int (*)(std::int64_t, std::int64_t, int);
using NativeGetEntityActiveCallbackFn = int (*)(std::int64_t, std::int64_t, int*);
using NativeSetStateInt32CallbackFn = int (*)(std::int64_t, const char*, std::int32_t);
using NativeSetStateDoubleCallbackFn = int (*)(std::int64_t, const char*, double);
using NativeRequestExitCallbackFn = int (*)(std::int64_t);

struct NativeCallbackTable
{
    std::int32_t version = kNativeCallbackTableVersion;
    NativeLogCallbackFn log = nullptr;
    NativeCreateEntityCallbackFn createEntity = nullptr;
    NativeSetPositionCallbackFn setPosition = nullptr;
    NativeGetPositionCallbackFn getPosition = nullptr;
    NativeGetStateBoolCallbackFn getStateBool = nullptr;
    NativeAddStateInt32CallbackFn addStateInt32 = nullptr;
    NativeSetStateBoolCallbackFn setStateBool = nullptr;
    NativeSetStateStringCallbackFn setStateString = nullptr;
    NativeStatePortAvailableCallbackFn statePortAvailable = nullptr;
    NativeGetInt32CallbackFn getConfigurationInt32 = nullptr;
    NativeGetDoubleCallbackFn getConfigurationDouble = nullptr;
    NativeGetStringCallbackFn getConfigurationString = nullptr;
    NativeQueryInputCallbackFn queryInput = nullptr;
    NativeGetDoubleCallbackFn getInputValue = nullptr;
    NativeFindEntityCallbackFn findEntity = nullptr;
    NativeSetEntityActiveCallbackFn setEntityActive = nullptr;
    NativeGetEntityActiveCallbackFn getEntityActive = nullptr;
    NativeGetInt32CallbackFn getStateInt32 = nullptr;
    NativeGetDoubleCallbackFn getStateDouble = nullptr;
    NativeGetStringCallbackFn getStateString = nullptr;
    NativeSetStateInt32CallbackFn setStateInt32 = nullptr;
    NativeSetStateDoubleCallbackFn setStateDouble = nullptr;
    NativeRequestExitCallbackFn requestExit = nullptr;
};

using BridgeLoadAssemblyFn = int (*)(const char*, char*, int, std::int64_t*);
using BridgeCreateInstanceFn = int (*)(std::int64_t,
                                       const char*,
                                       std::int64_t,
                                       std::int64_t,
                                       const NativeCallbackTable*,
                                       char*,
                                       int,
                                       std::int64_t*);
using BridgeInvokeLifecycleFn = int (*)(std::int64_t, int, float, char*, int);
using BridgeInvokeUiNamedActionFn = int (*)(
    std::int64_t, const char*, const char*, const char*, const char*, int, const char*, char*, int);
using BridgeInvokeNamedActionFn = int (*)(std::int64_t, const char*, const char*, char*, int);
using BridgeInvokeInt32BinaryMethodFn =
    int (*)(std::int64_t, const char*, std::int32_t, std::int32_t, std::int32_t*, char*, int);
using BridgeReleasePackageFn = int (*)(std::int64_t);

[[nodiscard]] ScriptDiagnostic MakeDiagnostic(std::string code,
                                              std::string title,
                                              std::string message);
[[nodiscard]] std::filesystem::path NormalizeRoot(const std::filesystem::path& root);
[[nodiscard]] std::filesystem::path ResolvePackagePath(const std::filesystem::path& packageRoot,
                                                       const std::filesystem::path& path);
[[nodiscard]] bool HasStringField(const nlohmann::json& object, std::string_view field);
[[nodiscard]] std::string ReadStringField(const nlohmann::json& object, std::string_view field);
[[nodiscard]] std::vector<std::string> ReadStringArrayField(const nlohmann::json& object,
                                                            std::string_view field);
#if defined(_WIN32)
[[nodiscard]] std::wstring ToHostString(const std::filesystem::path& path);
[[nodiscard]] std::wstring ToHostString(std::string_view value);
#else
[[nodiscard]] std::string ToHostString(const std::filesystem::path& path);
[[nodiscard]] std::string ToHostString(std::string_view value);
#endif
[[nodiscard]] std::filesystem::path RuntimeBridgeFromEnvironment();
[[nodiscard]] std::filesystem::path DotnetRootFromEnvironment();
[[nodiscard]] std::string Sha256File(const std::filesystem::path& path);
[[nodiscard]] std::string BridgeError(const std::array<char, kBridgeErrorBufferSize>& buffer);
[[nodiscard]] std::string CodeForBridgeStatus(BridgeStatus status, std::string_view operation);
[[nodiscard]] std::string TitleForDiagnostic(std::string_view code);

struct CSharpScriptHost::Impl
{
    struct HostFxrApi
    {
        void* library = nullptr;
        hostfxr_initialize_for_runtime_config_fn initializeForRuntimeConfig = nullptr;
        hostfxr_get_runtime_delegate_fn getRuntimeDelegate = nullptr;
        hostfxr_close_fn close = nullptr;

        HostFxrApi() = default;
        HostFxrApi(const HostFxrApi&) = delete;
        HostFxrApi& operator=(const HostFxrApi&) = delete;
        HostFxrApi(HostFxrApi&& other) noexcept;
        HostFxrApi& operator=(HostFxrApi&& other) noexcept;
        ~HostFxrApi();
    };

    struct ManagedPackage
    {
        std::int64_t bridgeHandle = 0;
        std::filesystem::path assemblyPath;
    };

    using CapabilitySet = std::unordered_set<std::string>;

    struct PackageState
    {
        std::vector<ManagedPackage> managedPackages;
        std::unordered_map<std::string, CapabilitySet> grantedCapabilitiesByScriptId;
    };

    struct InstanceIdentity
    {
        std::uint64_t entity = 0;
        std::string scriptId;

        bool operator==(const InstanceIdentity&) const = default;
    };

    struct InstanceIdentityHash
    {
        [[nodiscard]] std::size_t operator()(const InstanceIdentity& identity) const noexcept;
    };

    struct InstanceState
    {
        ScriptInstanceHandle instance;
        std::int64_t bridgeInstanceHandle = 0;
        std::int64_t contextHandle = 0;
        std::string classId;
        std::string scriptId;
        ScriptEntityHandle selfEntity;
        std::uint64_t activeInstanceToken = 0;
        CapabilitySet grantedCapabilities;
        std::string instanceConfigurationCanonicalJson = "{}";
        std::vector<ScriptDiagnostic> pendingDiagnostics;
    };

    struct ArtifactEntry
    {
        std::filesystem::path assemblyPath;
        std::filesystem::path runtimeConfigPath;
        std::string scriptId;
        std::vector<std::string> grantedCapabilities;
    };

    explicit Impl(CSharpScriptHostOptions hostOptions);
    ~Impl();

    [[nodiscard]] ScriptPackageLoadResult LoadVerifiedGeneration(
        const VerifiedScriptGenerationLoadRequest& request);
    [[nodiscard]] ScriptPackageLoadResult LoadPackage(const ScriptPackageLoadRequest& request);
    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(ScriptPackageHandle package,
                                                            const ScriptClassId& classId);
    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        const ScriptInstanceCreateRequest& request);
    [[nodiscard]] ScriptHostOperationResult InvokeLifecycle(
        const ScriptLifecycleInvocation& invocation);
    [[nodiscard]] ScriptHostOperationResult InvokeUiNamedAction(
        const ScriptUiNamedActionInvocation& invocation);
    [[nodiscard]] ScriptHostOperationResult InvokeNamedAction(
        const ScriptNamedActionInvocation& invocation);
    [[nodiscard]] CSharpInt32BinaryMethodInvocationResult InvokeInt32BinaryMethod(
        const CSharpInt32BinaryMethodInvocation& invocation);
    [[nodiscard]] std::size_t InstanceCount() const noexcept
    { return instances.size(); }

    [[nodiscard]] std::string ResolveScriptId(const PackageState& package,
                                              const ScriptInstanceCreateRequest& request) const;
    [[nodiscard]] CapabilitySet ResolveGrantedCapabilities(const PackageState& package,
                                                           const std::string& scriptId) const;
    [[nodiscard]] InstanceState* FindInstanceByContext(std::int64_t contextHandle);
    [[nodiscard]] static ScriptStateContext StateContext(const InstanceState& state);

    void RecordDiagnostic(InstanceState& state, ScriptDiagnostic diagnostic);
    [[nodiscard]] bool RequireCapability(InstanceState& state,
                                         std::string_view capability,
                                         std::string operation);
    [[nodiscard]] bool RequireWorld(InstanceState& state, std::string operation);
    [[nodiscard]] bool RequireStatePort(InstanceState& state, std::string operation);
    void RecordInvalidEntityHandle(InstanceState& state,
                                   std::int64_t entityHandle,
                                   std::string operation);
    void RecordWorldDiagnostics(InstanceState& state, std::vector<ScriptDiagnostic> diagnostics);
    [[nodiscard]] NativeCallbackStatus LogFromManaged(std::int64_t contextHandle,
                                                      int level,
                                                      const char* message);
    [[nodiscard]] NativeCallbackStatus CreateEntityFromManaged(std::int64_t contextHandle,
                                                               const char* name,
                                                               std::int64_t* entityHandle);
    [[nodiscard]] NativeCallbackStatus SetPositionFromManaged(
        std::int64_t contextHandle, std::int64_t entityHandle, float x, float y, float z);
    [[nodiscard]] NativeCallbackStatus GetPositionFromManaged(
        std::int64_t contextHandle, std::int64_t entityHandle, float* x, float* y, float* z);
    [[nodiscard]] NativeCallbackStatus GetStateBoolFromManaged(std::int64_t contextHandle,
                                                               const char* key,
                                                               int* value);
    [[nodiscard]] NativeCallbackStatus AddStateInt32FromManaged(std::int64_t contextHandle,
                                                                const char* key,
                                                                std::int32_t delta,
                                                                std::int32_t* value);
    [[nodiscard]] NativeCallbackStatus SetStateBoolFromManaged(std::int64_t contextHandle,
                                                               const char* key,
                                                               int value);
    [[nodiscard]] NativeCallbackStatus SetStateStringFromManaged(std::int64_t contextHandle,
                                                                 const char* key,
                                                                 const char* value);
    [[nodiscard]] NativeCallbackStatus GetConfigurationInt32FromManaged(
        std::int64_t, const char*, std::int32_t*);
    [[nodiscard]] NativeCallbackStatus GetConfigurationDoubleFromManaged(
        std::int64_t, const char*, double*);
    [[nodiscard]] NativeCallbackStatus GetConfigurationStringFromManaged(
        std::int64_t, const char*, char*, int);
    [[nodiscard]] NativeCallbackStatus QueryInputFromManaged(
        std::int64_t, const char*, int, int*);
    [[nodiscard]] NativeCallbackStatus GetInputValueFromManaged(
        std::int64_t, const char*, double*);
    [[nodiscard]] NativeCallbackStatus FindEntityFromManaged(
        std::int64_t, const char*, std::int64_t*);
    [[nodiscard]] NativeCallbackStatus SetEntityActiveFromManaged(
        std::int64_t, std::int64_t, int);
    [[nodiscard]] NativeCallbackStatus GetEntityActiveFromManaged(
        std::int64_t, std::int64_t, int*);
    [[nodiscard]] NativeCallbackStatus GetStateInt32FromManaged(
        std::int64_t, const char*, std::int32_t*);
    [[nodiscard]] NativeCallbackStatus GetStateDoubleFromManaged(
        std::int64_t, const char*, double*);
    [[nodiscard]] NativeCallbackStatus GetStateStringFromManaged(
        std::int64_t, const char*, char*, int);
    [[nodiscard]] NativeCallbackStatus SetStateInt32FromManaged(
        std::int64_t, const char*, std::int32_t);
    [[nodiscard]] NativeCallbackStatus SetStateDoubleFromManaged(
        std::int64_t, const char*, double);
    [[nodiscard]] NativeCallbackStatus RequestExitFromManaged(std::int64_t);

    [[nodiscard]] static std::int64_t AllocateContextHandle(Impl* impl);
    static void ReleaseContextHandle(std::int64_t contextHandle);
    [[nodiscard]] static Impl* LookupContext(std::int64_t contextHandle);
    [[nodiscard]] static std::mutex& ContextRegistryMutex();
    [[nodiscard]] static std::unordered_map<std::int64_t, Impl*>& ContextRegistry();
    [[nodiscard]] static std::int64_t& NextContextHandle();

    static int LogCallback(std::int64_t contextHandle, int level, const char* message) noexcept;
    static int CreateEntityCallback(std::int64_t contextHandle,
                                    const char* name,
                                    std::int64_t* entityHandle) noexcept;
    static int SetPositionCallback(
        std::int64_t contextHandle, std::int64_t entityHandle, float x, float y, float z) noexcept;
    static int GetPositionCallback(std::int64_t contextHandle,
                                   std::int64_t entityHandle,
                                   float* x,
                                   float* y,
                                   float* z) noexcept;
    static int GetStateBoolCallback(std::int64_t contextHandle,
                                    const char* key,
                                    int* value) noexcept;
    static int AddStateInt32Callback(std::int64_t contextHandle,
                                     const char* key,
                                     std::int32_t delta,
                                     std::int32_t* value) noexcept;
    static int SetStateBoolCallback(std::int64_t contextHandle,
                                    const char* key,
                                    int value) noexcept;
    static int SetStateStringCallback(std::int64_t contextHandle,
                                      const char* key,
                                      const char* value) noexcept;
    static int StatePortAvailableCallback(std::int64_t contextHandle) noexcept;
    static int GetConfigurationInt32Callback(
        std::int64_t, const char*, std::int32_t*) noexcept;
    static int GetConfigurationDoubleCallback(
        std::int64_t, const char*, double*) noexcept;
    static int GetConfigurationStringCallback(
        std::int64_t, const char*, char*, int) noexcept;
    static int QueryInputCallback(std::int64_t, const char*, int, int*) noexcept;
    static int GetInputValueCallback(std::int64_t, const char*, double*) noexcept;
    static int FindEntityCallback(std::int64_t, const char*, std::int64_t*) noexcept;
    static int SetEntityActiveCallback(std::int64_t, std::int64_t, int) noexcept;
    static int GetEntityActiveCallback(std::int64_t, std::int64_t, int*) noexcept;
    static int GetStateInt32Callback(
        std::int64_t, const char*, std::int32_t*) noexcept;
    static int GetStateDoubleCallback(std::int64_t, const char*, double*) noexcept;
    static int GetStateStringCallback(
        std::int64_t, const char*, char*, int) noexcept;
    static int SetStateInt32Callback(
        std::int64_t, const char*, std::int32_t) noexcept;
    static int SetStateDoubleCallback(std::int64_t, const char*, double) noexcept;
    static int RequestExitCallback(std::int64_t) noexcept;

    [[nodiscard]] std::optional<std::vector<ArtifactEntry>> LoadArtifactEntries(
        const std::filesystem::path& manifestPath, ScriptPackageLoadResult& result);
    [[nodiscard]] bool ValidateRuntimeConfig(const std::filesystem::path& runtimeConfigPath,
                                             std::vector<ScriptDiagnostic>& diagnostics);
    [[nodiscard]] bool EnsureRuntime(const std::filesystem::path& runtimeConfigPath,
                                     std::vector<ScriptDiagnostic>& diagnostics);
    [[nodiscard]] std::optional<HostFxrApi> LoadHostFxr(std::vector<ScriptDiagnostic>& diagnostics);
    [[nodiscard]] bool LoadBridgeFunctions(std::vector<ScriptDiagnostic>& diagnostics);
    template <typename Function>
    [[nodiscard]] bool LoadBridgeFunction(std::string_view methodName,
                                          Function& function,
                                          std::vector<ScriptDiagnostic>& diagnostics);

    CSharpScriptHostOptions options;
    HostFxrApi hostfxr;
    load_assembly_and_get_function_pointer_fn loadAssemblyAndGetFunctionPointer = nullptr;
    BridgeLoadAssemblyFn loadAssembly = nullptr;
    BridgeCreateInstanceFn createInstance = nullptr;
    BridgeInvokeLifecycleFn invokeLifecycle = nullptr;
    BridgeInvokeUiNamedActionFn invokeUiNamedAction = nullptr;
    BridgeInvokeNamedActionFn invokeNamedAction = nullptr;
    BridgeInvokeInt32BinaryMethodFn invokeInt32BinaryMethod = nullptr;
    BridgeReleasePackageFn releasePackage = nullptr;
    std::uint64_t nextPackageHandle = 1;
    std::uint64_t nextInstanceHandle = 1;
    std::uint64_t nextActiveInstanceToken = 1;
    std::unordered_map<std::uint64_t, PackageState> packages;
    std::unordered_map<std::uint64_t, InstanceState> instances;
    std::unordered_map<std::int64_t, std::uint64_t> contextToInstance;
    std::unordered_set<InstanceIdentity, InstanceIdentityHash> activeIdentities;
};

} // namespace SagaEngine::Scripting
