/// @file CSharpScriptHost.cpp
/// @brief Implements the hostfxr-backed C# SagaScript host.

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>

#include "SagaShared/Diagnostics/DiagnosticCategory.hpp"
#include "SagaShared/Diagnostics/DiagnosticSeverity.hpp"
#include "SagaShared/Diagnostics/DiagnosticSource.hpp"

#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <nethost.h>
#include <nlohmann/json.hpp>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace SagaEngine::Scripting
{

namespace
{

constexpr std::string_view kArtifactsManifestFileName = "script_artifacts.json";
constexpr std::string_view kBridgeTypeName =
    "SagaEngine.Scripting.RuntimeBridge.NativeBridge, SagaScript.RuntimeBridge";
constexpr std::size_t kBridgeErrorBufferSize = 2048;
constexpr std::int32_t kNativeCallbackTableVersion = 2;
constexpr std::string_view kCapabilityCreateEntity =
    "Gameplay.World.CreateEntity";
constexpr std::string_view kCapabilityTransformRead =
    "Gameplay.Transform.Read";
constexpr std::string_view kCapabilityTransformWrite =
    "Gameplay.Transform.Write";
constexpr std::string_view kCapabilityStatePort =
    "Sample.StarterArena.GameplayState";

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
using NativeCreateEntityCallbackFn =
    int (*)(std::int64_t, const char*, std::int64_t*);
using NativeSetPositionCallbackFn =
    int (*)(std::int64_t, std::int64_t, float, float, float);
using NativeGetPositionCallbackFn =
    int (*)(std::int64_t, std::int64_t, float*, float*, float*);
using NativeGetStateBoolCallbackFn = int (*)(std::int64_t, const char*, int*);
using NativeAddStateInt32CallbackFn =
    int (*)(std::int64_t, const char*, std::int32_t, std::int32_t*);
using NativeSetStateBoolCallbackFn = int (*)(std::int64_t, const char*, int);
using NativeSetStateStringCallbackFn =
    int (*)(std::int64_t, const char*, const char*);
using NativeStatePortAvailableCallbackFn = int (*)(std::int64_t);

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
};

using BridgeLoadAssemblyFn = int (*)(const char*, char*, int, std::int64_t*);
using BridgeCreateInstanceFn =
    int (*)(
        std::int64_t,
        const char*,
        std::int64_t,
        std::int64_t,
        const NativeCallbackTable*,
        char*,
        int,
        std::int64_t*);
using BridgeInvokeLifecycleFn =
    int (*)(std::int64_t, int, float, char*, int);
using BridgeInvokeUiNamedActionFn =
    int (*)(
        std::int64_t,
        const char*,
        const char*,
        const char*,
        const char*,
        int,
        const char*,
        char*,
        int);
using BridgeInvokeInt32BinaryMethodFn =
    int (*)(
        std::int64_t,
        const char*,
        std::int32_t,
        std::int32_t,
        std::int32_t*,
        char*,
        int);
using BridgeReleasePackageFn = int (*)(std::int64_t);

[[nodiscard]] ScriptDiagnostic MakeDiagnostic(
    std::string code,
    std::string title,
    std::string message)
{
    ScriptDiagnostic diagnostic;
    diagnostic.diagnostic.severity =
        SagaShared::Diagnostics::DiagnosticSeverity::Error;
    diagnostic.diagnostic.category =
        SagaShared::Diagnostics::DiagnosticCategory::Script;
    diagnostic.diagnostic.source =
        SagaShared::Diagnostics::DiagnosticSource::Runtime;
    diagnostic.diagnostic.code.value = std::move(code);
    diagnostic.diagnostic.title = std::move(title);
    diagnostic.diagnostic.message = std::move(message);
    return diagnostic;
}

[[nodiscard]] std::filesystem::path NormalizeRoot(
    const std::filesystem::path& root)
{
    return std::filesystem::absolute(root).lexically_normal();
}

[[nodiscard]] std::filesystem::path ResolvePackagePath(
    const std::filesystem::path& packageRoot,
    const std::filesystem::path& path)
{
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }

    return (packageRoot / path).lexically_normal();
}

[[nodiscard]] bool HasStringField(
    const nlohmann::json& object,
    std::string_view field)
{
    const auto iterator = object.find(field);
    return iterator != object.end() && iterator->is_string();
}

[[nodiscard]] std::string ReadStringField(
    const nlohmann::json& object,
    std::string_view field)
{
    return object.at(std::string(field)).get<std::string>();
}

[[nodiscard]] std::vector<std::string> ReadStringArrayField(
    const nlohmann::json& object,
    std::string_view field)
{
    const auto iterator = object.find(field);
    if (iterator == object.end() || !iterator->is_array())
    {
        return {};
    }

    std::vector<std::string> values;
    for (const auto& value : *iterator)
    {
        if (value.is_string())
        {
            values.push_back(value.get<std::string>());
        }
    }
    return values;
}

#if defined(_WIN32)
[[nodiscard]] std::wstring ToHostString(const std::filesystem::path& path)
{
    return path.wstring();
}

[[nodiscard]] std::wstring ToHostString(std::string_view value)
{
    return std::wstring(value.begin(), value.end());
}
#else
[[nodiscard]] std::string ToHostString(const std::filesystem::path& path)
{
    return path.string();
}

[[nodiscard]] std::string ToHostString(std::string_view value)
{
    return std::string(value);
}
#endif

[[nodiscard]] std::filesystem::path RuntimeBridgeFromEnvironment()
{
    const char* value = std::getenv("SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY");
    if (value == nullptr || std::string_view(value).empty())
    {
        return {};
    }

    return std::filesystem::path(value);
}

[[nodiscard]] std::filesystem::path DotnetRootFromEnvironment()
{
    const char* value = std::getenv("DOTNET_ROOT");
    if (value == nullptr || std::string_view(value).empty())
    {
        return {};
    }

    return std::filesystem::path(value);
}

[[nodiscard]] std::string BridgeError(
    const std::array<char, kBridgeErrorBufferSize>& buffer)
{
    return std::string(buffer.data());
}

[[nodiscard]] std::string CodeForBridgeStatus(
    BridgeStatus status,
    std::string_view operation)
{
    switch (status)
    {
    case BridgeStatus::ClassMissing:
        return ScriptHostDiagnostics::ClassMissing;
    case BridgeStatus::ClassInvalid:
        return ScriptHostDiagnostics::ClassInvalid;
    case BridgeStatus::InstanceCreationFailed:
        return ScriptHostDiagnostics::InstanceCreationFailed;
    case BridgeStatus::LifecycleMethodMissing:
        return ScriptHostDiagnostics::LifecycleMethodMissing;
    case BridgeStatus::LifecycleFailed:
        return ScriptHostDiagnostics::LifecycleFailed;
    case BridgeStatus::ManagedException:
        return ScriptHostDiagnostics::ManagedException;
    case BridgeStatus::UiNamedActionMethodMissing:
        return ScriptHostDiagnostics::UiNamedActionMethodMissing;
    case BridgeStatus::UiNamedActionInvalidSignature:
        return ScriptHostDiagnostics::UiNamedActionInvalidSignature;
    case BridgeStatus::UiNamedActionReturnedFalse:
        return ScriptHostDiagnostics::UiNamedActionReturnedFalse;
    case BridgeStatus::Int32BinaryMethodMissing:
        return ScriptHostDiagnostics::Int32BinaryMethodMissing;
    case BridgeStatus::Int32BinaryInvalidSignature:
        return ScriptHostDiagnostics::Int32BinaryMethodInvalidSignature;
    case BridgeStatus::AssemblyLoadFailed:
        return ScriptHostDiagnostics::AssemblyLoadFailed;
    case BridgeStatus::InvalidArgument:
        return operation == "create"
            ? ScriptHostDiagnostics::InvalidPackageHandle
            : (operation == "uiNamedAction" || operation == "int32Binary"
                   ? ScriptHostDiagnostics::InvalidInstanceHandle
                   : ScriptHostDiagnostics::LifecycleFailed);
    case BridgeStatus::Ok:
        break;
    }

    return ScriptHostDiagnostics::LifecycleFailed;
}

[[nodiscard]] std::string TitleForDiagnostic(std::string_view code)
{
    if (code == ScriptHostDiagnostics::ClassMissing)
    {
        return "Script class not found";
    }
    if (code == ScriptHostDiagnostics::ClassInvalid)
    {
        return "Invalid script class";
    }
    if (code == ScriptHostDiagnostics::InstanceCreationFailed)
    {
        return "Script instance creation failed";
    }
    if (code == ScriptHostDiagnostics::ManagedException)
    {
        return "Managed script exception";
    }
    if (code == ScriptHostDiagnostics::AssemblyLoadFailed)
    {
        return "Managed script assembly load failed";
    }
    if (code == ScriptHostDiagnostics::ScriptCapabilityDenied)
    {
        return "Script capability denied";
    }
    if (code == ScriptHostDiagnostics::InvalidEntityHandle)
    {
        return "Invalid script entity handle";
    }
    if (code == ScriptHostDiagnostics::ScriptWorldUnavailable)
    {
        return "Script world unavailable";
    }
    if (code == ScriptHostDiagnostics::UiNamedActionMethodMissing)
    {
        return "UI named action method not found";
    }
    if (code == ScriptHostDiagnostics::UiNamedActionInvalidSignature)
    {
        return "Invalid UI named action signature";
    }
    if (code == ScriptHostDiagnostics::UiNamedActionReturnedFalse)
    {
        return "UI named action rejected";
    }
    if (code == ScriptHostDiagnostics::Int32BinaryMethodMissing)
    {
        return "Int32 binary method not found";
    }
    if (code == ScriptHostDiagnostics::Int32BinaryMethodInvalidSignature)
    {
        return "Invalid Int32 binary method signature";
    }

    return "C# script host failure";
}

template <typename Function>
[[nodiscard]] Function LoadSymbol(void* library, const char* name)
{
#if defined(_WIN32)
    return reinterpret_cast<Function>(
        GetProcAddress(static_cast<HMODULE>(library), name));
#else
    return reinterpret_cast<Function>(dlsym(library, name));
#endif
}

[[nodiscard]] void* OpenLibrary(const std::filesystem::path& path)
{
#if defined(_WIN32)
    return static_cast<void*>(LoadLibraryW(path.wstring().c_str()));
#else
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
}

void CloseLibrary(void* library)
{
    if (library == nullptr)
    {
        return;
    }

#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
}

} // namespace

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

        HostFxrApi(HostFxrApi&& other) noexcept
            : library(std::exchange(other.library, nullptr))
            , initializeForRuntimeConfig(std::exchange(
                  other.initializeForRuntimeConfig,
                  nullptr))
            , getRuntimeDelegate(std::exchange(other.getRuntimeDelegate, nullptr))
            , close(std::exchange(other.close, nullptr))
        {
        }

        HostFxrApi& operator=(HostFxrApi&& other) noexcept
        {
            if (this != &other)
            {
                CloseLibrary(library);
                library = std::exchange(other.library, nullptr);
                initializeForRuntimeConfig = std::exchange(
                    other.initializeForRuntimeConfig,
                    nullptr);
                getRuntimeDelegate =
                    std::exchange(other.getRuntimeDelegate, nullptr);
                close = std::exchange(other.close, nullptr);
            }
            return *this;
        }

        ~HostFxrApi()
        {
            CloseLibrary(library);
        }
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

    struct InstanceState
    {
        std::int64_t bridgeInstanceHandle = 0;
        std::int64_t contextHandle = 0;
        std::string classId;
        std::string scriptId;
        ScriptEntityHandle selfEntity;
        CapabilitySet grantedCapabilities;
        std::vector<ScriptDiagnostic> pendingDiagnostics;
    };

    explicit Impl(CSharpScriptHostOptions hostOptions)
        : options(std::move(hostOptions))
    {
        if (options.runtimeBridgeAssembly.empty())
        {
            options.runtimeBridgeAssembly = RuntimeBridgeFromEnvironment();
        }
        if (options.dotnetRoot.empty())
        {
            options.dotnetRoot = DotnetRootFromEnvironment();
        }
    }

    ~Impl()
    {
        for (const auto& [_, instance] : instances)
        {
            ReleaseContextHandle(instance.contextHandle);
        }

        if (releasePackage != nullptr)
        {
            for (const auto& [_, package] : packages)
            {
                for (const auto& managedPackage : package.managedPackages)
                {
                    static_cast<void>(releasePackage(managedPackage.bridgeHandle));
                }
            }
        }
    }

    [[nodiscard]] ScriptPackageLoadResult LoadPackage(
        const ScriptPackageLoadRequest& request)
    {
        ScriptPackageLoadResult result;
        const auto packageRoot = NormalizeRoot(request.packageRoot);
        const auto manifestPath =
            ResolvePackagePath(packageRoot, request.scriptArtifactManifest);
        const auto artifacts = LoadArtifactEntries(manifestPath, result);
        if (!artifacts.has_value())
        {
            return result;
        }

        if (artifacts->empty())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                ScriptHostDiagnostics::ManifestInvalid,
                "Invalid script artifact manifest",
                "Script artifact manifest contains no loadable artifacts."));
            result.diagnostics.back().metadata["manifestPath"] =
                manifestPath.generic_string();
            return result;
        }

        const auto runtimeConfigPath =
            ResolvePackagePath(packageRoot, artifacts->front().runtimeConfigPath);
        if (!std::filesystem::exists(runtimeConfigPath))
        {
            result.diagnostics.push_back(MakeDiagnostic(
                ScriptHostDiagnostics::RuntimeConfigMissing,
                "Script runtimeconfig missing",
                "C# script host could not open the runtimeconfig file."));
            result.diagnostics.back().metadata["runtimeConfigPath"] =
                runtimeConfigPath.generic_string();
            return result;
        }

        if (!ValidateRuntimeConfig(runtimeConfigPath, result.diagnostics))
        {
            return result;
        }

        if (!EnsureRuntime(runtimeConfigPath, result.diagnostics))
        {
            return result;
        }

        PackageState packageState;
        for (const auto& artifact : *artifacts)
        {
            if (artifact.scriptId.empty())
            {
                continue;
            }

            auto& granted =
                packageState.grantedCapabilitiesByScriptId[artifact.scriptId];
            granted.insert(
                artifact.grantedCapabilities.begin(),
                artifact.grantedCapabilities.end());
        }

        std::vector<std::filesystem::path> loadedAssemblies;
        for (const auto& artifact : *artifacts)
        {
            const auto assemblyPath =
                ResolvePackagePath(packageRoot, artifact.assemblyPath);
            if (std::find(loadedAssemblies.begin(), loadedAssemblies.end(), assemblyPath) !=
                loadedAssemblies.end())
            {
                continue;
            }

            if (!std::filesystem::exists(assemblyPath))
            {
                result.diagnostics.push_back(MakeDiagnostic(
                    ScriptHostDiagnostics::AssemblyLoadFailed,
                    "Managed script assembly missing",
                    "C# script host could not open the script assembly."));
                result.diagnostics.back().metadata["assemblyPath"] =
                    assemblyPath.generic_string();
                return result;
            }

            std::array<char, kBridgeErrorBufferSize> error{};
            std::int64_t bridgePackageHandle = 0;
            const auto status = static_cast<BridgeStatus>(loadAssembly(
                assemblyPath.string().c_str(),
                error.data(),
                static_cast<int>(error.size()),
                &bridgePackageHandle));
            if (status != BridgeStatus::Ok)
            {
                auto diagnostic = MakeDiagnostic(
                    ScriptHostDiagnostics::AssemblyLoadFailed,
                    "Managed script assembly load failed",
                    "C# script host failed to load a script assembly.");
                diagnostic.metadata["assemblyPath"] = assemblyPath.generic_string();
                diagnostic.metadata["bridgeError"] = BridgeError(error);
                result.diagnostics.push_back(std::move(diagnostic));
                return result;
            }

            loadedAssemblies.push_back(assemblyPath);
            packageState.managedPackages.push_back(ManagedPackage{
                bridgePackageHandle,
                assemblyPath,
            });
        }

        const auto packageHandle = nextPackageHandle++;
        packages.emplace(packageHandle, std::move(packageState));
        result.succeeded = true;
        result.package.value = packageHandle;
        return result;
    }

    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        ScriptPackageHandle package,
        const ScriptClassId& classId)
    {
        ScriptInstanceCreateRequest request;
        request.package = package;
        request.classId = classId;
        return CreateInstance(request);
    }

    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        const ScriptInstanceCreateRequest& request)
    {
        ScriptInstanceCreateResult result;
        const auto packageIterator = packages.find(request.package.value);
        if (packageIterator == packages.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                ScriptHostDiagnostics::InvalidPackageHandle,
                "Invalid script package handle",
                "C# script host cannot create an instance from an unknown package."));
            return result;
        }

        ScriptDiagnostic lastMissingDiagnostic;
        bool sawMissing = false;
        const auto scriptId = ResolveScriptId(packageIterator->second, request);
        const auto grantedCapabilities =
            ResolveGrantedCapabilities(packageIterator->second, scriptId);
        const auto contextHandle = AllocateContextHandle(this);
        const NativeCallbackTable callbacks{
            kNativeCallbackTableVersion,
            &CSharpScriptHost::Impl::LogCallback,
            &CSharpScriptHost::Impl::CreateEntityCallback,
            &CSharpScriptHost::Impl::SetPositionCallback,
            &CSharpScriptHost::Impl::GetPositionCallback,
            &CSharpScriptHost::Impl::GetStateBoolCallback,
            &CSharpScriptHost::Impl::AddStateInt32Callback,
            &CSharpScriptHost::Impl::SetStateBoolCallback,
            &CSharpScriptHost::Impl::SetStateStringCallback,
            &CSharpScriptHost::Impl::StatePortAvailableCallback,
        };

        for (const auto& managedPackage : packageIterator->second.managedPackages)
        {
            std::array<char, kBridgeErrorBufferSize> error{};
            std::int64_t bridgeInstanceHandle = 0;
            const auto status = static_cast<BridgeStatus>(createInstance(
                managedPackage.bridgeHandle,
                request.classId.value.c_str(),
                contextHandle,
                static_cast<std::int64_t>(request.selfEntity.value),
                &callbacks,
                error.data(),
                static_cast<int>(error.size()),
                &bridgeInstanceHandle));
            if (status == BridgeStatus::Ok)
            {
                const auto instanceHandle = nextInstanceHandle++;
                InstanceState state;
                state.bridgeInstanceHandle = bridgeInstanceHandle;
                state.contextHandle = contextHandle;
                state.classId = request.classId.value;
                state.scriptId = scriptId;
                state.selfEntity = request.selfEntity;
                state.grantedCapabilities = grantedCapabilities;
                instances.emplace(instanceHandle, std::move(state));
                contextToInstance.emplace(contextHandle, instanceHandle);
                result.succeeded = true;
                result.instance.value = instanceHandle;
                return result;
            }

            const auto code = CodeForBridgeStatus(status, "create");
            auto diagnostic = MakeDiagnostic(
                code,
                TitleForDiagnostic(code),
                "C# script host failed to create a managed script instance.");
            diagnostic.scriptId = scriptId;
            diagnostic.metadata["classId"] = request.classId.value;
            diagnostic.metadata["assemblyPath"] =
                managedPackage.assemblyPath.generic_string();
            diagnostic.metadata["bridgeError"] = BridgeError(error);
            if (status == BridgeStatus::ClassMissing)
            {
                lastMissingDiagnostic = std::move(diagnostic);
                sawMissing = true;
                continue;
            }

            result.diagnostics.push_back(std::move(diagnostic));
            ReleaseContextHandle(contextHandle);
            return result;
        }

        if (sawMissing)
        {
            result.diagnostics.push_back(std::move(lastMissingDiagnostic));
        }
        ReleaseContextHandle(contextHandle);
        return result;
    }

    [[nodiscard]] ScriptHostOperationResult InvokeLifecycle(
        const ScriptLifecycleInvocation& invocation)
    {
        ScriptHostOperationResult result;
        const auto instanceIterator = instances.find(invocation.instance.value);
        if (instanceIterator == instances.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                ScriptHostDiagnostics::InvalidInstanceHandle,
                "Invalid script instance handle",
                "C# script host cannot invoke lifecycle on an unknown instance."));
            return result;
        }

        instanceIterator->second.pendingDiagnostics.clear();
        std::array<char, kBridgeErrorBufferSize> error{};
        const auto status = static_cast<BridgeStatus>(invokeLifecycle(
            instanceIterator->second.bridgeInstanceHandle,
            static_cast<int>(invocation.method),
            static_cast<float>(invocation.deltaTimeSeconds),
            error.data(),
            static_cast<int>(error.size())));
        if (status == BridgeStatus::Ok)
        {
            result.diagnostics =
                std::move(instanceIterator->second.pendingDiagnostics);
            result.succeeded = result.diagnostics.empty();
            return result;
        }

        const auto code = CodeForBridgeStatus(status, "lifecycle");
        auto diagnostic = MakeDiagnostic(
            code,
            TitleForDiagnostic(code),
            "C# script host failed while invoking a lifecycle method.");
        diagnostic.metadata["lifecycleMethod"] =
            std::string(ToString(invocation.method));
        diagnostic.metadata["bridgeError"] = BridgeError(error);
        result.diagnostics.push_back(std::move(diagnostic));
        for (auto& pending : instanceIterator->second.pendingDiagnostics)
        {
            result.diagnostics.push_back(std::move(pending));
        }
        instanceIterator->second.pendingDiagnostics.clear();
        return result;
    }

    [[nodiscard]] ScriptHostOperationResult InvokeUiNamedAction(
        const ScriptUiNamedActionInvocation& invocation)
    {
        ScriptHostOperationResult result;
        const auto instanceIterator = instances.find(invocation.instance.value);
        if (instanceIterator == instances.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                ScriptHostDiagnostics::InvalidInstanceHandle,
                "Invalid script instance handle",
                "C# script host cannot invoke UI named action on an unknown instance."));
            return result;
        }

        instanceIterator->second.pendingDiagnostics.clear();
        std::array<char, kBridgeErrorBufferSize> error{};
        const auto status = static_cast<BridgeStatus>(invokeUiNamedAction(
            instanceIterator->second.bridgeInstanceHandle,
            invocation.methodName.c_str(),
            invocation.context.actionId.c_str(),
            invocation.context.screenId.c_str(),
            invocation.context.elementId.c_str(),
            static_cast<int>(invocation.context.eventType),
            invocation.context.text.c_str(),
            error.data(),
            static_cast<int>(error.size())));
        if (status == BridgeStatus::Ok)
        {
            result.diagnostics =
                std::move(instanceIterator->second.pendingDiagnostics);
            result.succeeded = result.diagnostics.empty();
            return result;
        }

        const auto code = CodeForBridgeStatus(status, "uiNamedAction");
        auto diagnostic = MakeDiagnostic(
            code,
            TitleForDiagnostic(code),
            "C# script host failed while invoking a UI named action method.");
        diagnostic.scriptId = instanceIterator->second.scriptId;
        diagnostic.metadata["classId"] = instanceIterator->second.classId;
        diagnostic.metadata["scriptId"] = instanceIterator->second.scriptId;
        diagnostic.metadata["methodName"] = invocation.methodName;
        diagnostic.metadata["actionId"] = invocation.context.actionId;
        diagnostic.metadata["screenId"] = invocation.context.screenId;
        diagnostic.metadata["elementId"] = invocation.context.elementId;
        diagnostic.metadata["bridgeError"] = BridgeError(error);
        result.diagnostics.push_back(std::move(diagnostic));
        for (auto& pending : instanceIterator->second.pendingDiagnostics)
        {
            result.diagnostics.push_back(std::move(pending));
        }
        instanceIterator->second.pendingDiagnostics.clear();
        return result;
    }

    [[nodiscard]] CSharpInt32BinaryMethodInvocationResult InvokeInt32BinaryMethod(
        const CSharpInt32BinaryMethodInvocation& invocation)
    {
        CSharpInt32BinaryMethodInvocationResult result;
        const auto instanceIterator = instances.find(invocation.instance.value);
        if (instanceIterator == instances.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                ScriptHostDiagnostics::InvalidInstanceHandle,
                "Invalid script instance handle",
                "C# script host cannot invoke an Int32 binary method on an unknown instance."));
            return result;
        }

        instanceIterator->second.pendingDiagnostics.clear();
        std::array<char, kBridgeErrorBufferSize> error{};
        std::int32_t value = 0;
        const auto status = static_cast<BridgeStatus>(invokeInt32BinaryMethod(
            instanceIterator->second.bridgeInstanceHandle,
            invocation.methodName.c_str(),
            invocation.left,
            invocation.right,
            &value,
            error.data(),
            static_cast<int>(error.size())));
        if (status == BridgeStatus::Ok)
        {
            result.value = value;
            result.diagnostics =
                std::move(instanceIterator->second.pendingDiagnostics);
            result.succeeded = result.diagnostics.empty();
            return result;
        }

        const auto code = CodeForBridgeStatus(status, "int32Binary");
        auto diagnostic = MakeDiagnostic(
            code,
            TitleForDiagnostic(code),
            "C# script host failed while invoking an Int32 binary method.");
        diagnostic.scriptId = instanceIterator->second.scriptId;
        diagnostic.metadata["classId"] = instanceIterator->second.classId;
        diagnostic.metadata["scriptId"] = instanceIterator->second.scriptId;
        diagnostic.metadata["methodName"] = invocation.methodName;
        diagnostic.metadata["left"] = std::to_string(invocation.left);
        diagnostic.metadata["right"] = std::to_string(invocation.right);
        diagnostic.metadata["bridgeError"] = BridgeError(error);
        result.diagnostics.push_back(std::move(diagnostic));
        for (auto& pending : instanceIterator->second.pendingDiagnostics)
        {
            result.diagnostics.push_back(std::move(pending));
        }
        instanceIterator->second.pendingDiagnostics.clear();
        return result;
    }

    [[nodiscard]] std::string ResolveScriptId(
        const PackageState& package,
        const ScriptInstanceCreateRequest& request) const
    {
        if (!request.scriptId.empty())
        {
            return request.scriptId;
        }

        if (package.grantedCapabilitiesByScriptId.size() == 1)
        {
            return package.grantedCapabilitiesByScriptId.begin()->first;
        }

        return request.classId.value;
    }

    [[nodiscard]] CapabilitySet ResolveGrantedCapabilities(
        const PackageState& package,
        const std::string& scriptId) const
    {
        const auto iterator =
            package.grantedCapabilitiesByScriptId.find(scriptId);
        return iterator == package.grantedCapabilitiesByScriptId.end()
            ? CapabilitySet{}
            : iterator->second;
    }

    [[nodiscard]] InstanceState* FindInstanceByContext(
        const std::int64_t contextHandle)
    {
        const auto contextIterator = contextToInstance.find(contextHandle);
        if (contextIterator == contextToInstance.end())
        {
            return nullptr;
        }

        const auto instanceIterator = instances.find(contextIterator->second);
        return instanceIterator == instances.end()
            ? nullptr
            : &instanceIterator->second;
    }

    void RecordDiagnostic(
        InstanceState& state,
        ScriptDiagnostic diagnostic)
    {
        if (diagnostic.scriptId.empty())
        {
            diagnostic.scriptId = state.scriptId;
        }
        diagnostic.metadata["classId"] = state.classId;
        diagnostic.metadata["scriptId"] = state.scriptId;
        state.pendingDiagnostics.push_back(std::move(diagnostic));
    }

    [[nodiscard]] bool RequireCapability(
        InstanceState& state,
        std::string_view capability,
        std::string operation)
    {
        if (capability == kCapabilityStatePort && options.grantStatePortAccess)
        {
            return true;
        }
        if (state.grantedCapabilities.find(std::string(capability)) !=
            state.grantedCapabilities.end())
        {
            return true;
        }

        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ScriptCapabilityDenied,
            "Script capability denied",
            "Managed script attempted an engine operation without a granted capability.");
        diagnostic.metadata["capability"] = std::string(capability);
        diagnostic.metadata["operation"] = std::move(operation);
        RecordDiagnostic(state, std::move(diagnostic));
        return false;
    }

    [[nodiscard]] bool RequireWorld(
        InstanceState& state,
        std::string operation)
    {
        if (options.world != nullptr)
        {
            return true;
        }

        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ScriptWorldUnavailable,
            "Script world unavailable",
            "Managed script attempted a world operation without an engine world facade.");
        diagnostic.metadata["operation"] = std::move(operation);
        RecordDiagnostic(state, std::move(diagnostic));
        return false;
    }

    [[nodiscard]] bool RequireStatePort(InstanceState& state, std::string operation)
    {
        if (options.statePort != nullptr)
        {
            return true;
        }
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::ScriptStatePortUnavailable,
            "Script state port unavailable",
            "Managed script attempted a state operation without an application state facade.");
        diagnostic.metadata["operation"] = std::move(operation);
        RecordDiagnostic(state, std::move(diagnostic));
        return false;
    }

    void RecordInvalidEntityHandle(
        InstanceState& state,
        const std::int64_t entityHandle,
        std::string operation)
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::InvalidEntityHandle,
            "Invalid script entity handle",
            "Managed script attempted an entity operation with an invalid entity handle.");
        diagnostic.metadata["entityHandle"] = std::to_string(entityHandle);
        diagnostic.metadata["operation"] = std::move(operation);
        RecordDiagnostic(state, std::move(diagnostic));
    }

    void RecordWorldDiagnostics(
        InstanceState& state,
        std::vector<ScriptDiagnostic> diagnostics)
    {
        if (diagnostics.empty())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::LifecycleFailed,
                "Script world operation failed",
                "Script world facade rejected a managed operation.");
            RecordDiagnostic(state, std::move(diagnostic));
            return;
        }

        for (auto& diagnostic : diagnostics)
        {
            RecordDiagnostic(state, std::move(diagnostic));
        }
    }

    [[nodiscard]] NativeCallbackStatus LogFromManaged(
        const std::int64_t contextHandle,
        const int level,
        const char* message)
    {
        auto* state = FindInstanceByContext(contextHandle);
        if (state == nullptr)
        {
            return NativeCallbackStatus::InvalidArgument;
        }

        if (options.logSink)
        {
            ScriptLogEvent event;
            event.instance.value = contextToInstance[contextHandle];
            event.scriptId = state->scriptId;
            event.level = level == 1
                ? ScriptLogLevel::Warn
                : (level == 2 ? ScriptLogLevel::Error : ScriptLogLevel::Info);
            event.message = message == nullptr ? std::string{} : std::string(message);
            options.logSink(event);
        }

        return NativeCallbackStatus::Ok;
    }

    [[nodiscard]] NativeCallbackStatus CreateEntityFromManaged(
        const std::int64_t contextHandle,
        const char* name,
        std::int64_t* entityHandle)
    {
        if (entityHandle == nullptr)
        {
            return NativeCallbackStatus::InvalidArgument;
        }
        *entityHandle = 0;

        auto* state = FindInstanceByContext(contextHandle);
        if (state == nullptr)
        {
            return NativeCallbackStatus::InvalidArgument;
        }

        if (!RequireCapability(
                *state,
                kCapabilityCreateEntity,
                "World.CreateEntity"))
        {
            return NativeCallbackStatus::CapabilityDenied;
        }
        if (!RequireWorld(*state, "World.CreateEntity"))
        {
            return NativeCallbackStatus::WorldUnavailable;
        }

        auto result = options.world->CreateEntity(
            name == nullptr ? std::string{} : std::string(name));
        if (!result.Succeeded())
        {
            RecordWorldDiagnostics(*state, std::move(result.diagnostics));
            return NativeCallbackStatus::OperationFailed;
        }

        *entityHandle = static_cast<std::int64_t>(result.entity.value);
        return NativeCallbackStatus::Ok;
    }

    [[nodiscard]] NativeCallbackStatus SetPositionFromManaged(
        const std::int64_t contextHandle,
        const std::int64_t entityHandle,
        const float x,
        const float y,
        const float z)
    {
        auto* state = FindInstanceByContext(contextHandle);
        if (state == nullptr)
        {
            return NativeCallbackStatus::InvalidArgument;
        }

        if (!RequireCapability(
                *state,
                kCapabilityTransformWrite,
                "Entity.SetPosition"))
        {
            return NativeCallbackStatus::CapabilityDenied;
        }
        if (!RequireWorld(*state, "Entity.SetPosition"))
        {
            return NativeCallbackStatus::WorldUnavailable;
        }
        if (entityHandle <= 0)
        {
            RecordInvalidEntityHandle(
                *state,
                entityHandle,
                "Entity.SetPosition");
            return NativeCallbackStatus::InvalidEntity;
        }

        auto result = options.world->SetPosition(
            ScriptEntityHandle{static_cast<std::uint64_t>(entityHandle)},
            ScriptVector3{x, y, z});
        if (!result.Succeeded())
        {
            RecordWorldDiagnostics(*state, std::move(result.diagnostics));
            return NativeCallbackStatus::InvalidEntity;
        }

        return NativeCallbackStatus::Ok;
    }

    [[nodiscard]] NativeCallbackStatus GetPositionFromManaged(
        const std::int64_t contextHandle,
        const std::int64_t entityHandle,
        float* x,
        float* y,
        float* z)
    {
        if (x == nullptr || y == nullptr || z == nullptr)
        {
            return NativeCallbackStatus::InvalidArgument;
        }

        auto* state = FindInstanceByContext(contextHandle);
        if (state == nullptr)
        {
            return NativeCallbackStatus::InvalidArgument;
        }

        if (!RequireCapability(
                *state,
                kCapabilityTransformRead,
                "Entity.GetPosition"))
        {
            return NativeCallbackStatus::CapabilityDenied;
        }
        if (!RequireWorld(*state, "Entity.GetPosition"))
        {
            return NativeCallbackStatus::WorldUnavailable;
        }
        if (entityHandle <= 0)
        {
            RecordInvalidEntityHandle(
                *state,
                entityHandle,
                "Entity.GetPosition");
            return NativeCallbackStatus::InvalidEntity;
        }

        auto result = options.world->GetPosition(
            ScriptEntityHandle{static_cast<std::uint64_t>(entityHandle)});
        if (!result.Succeeded())
        {
            RecordWorldDiagnostics(*state, std::move(result.diagnostics));
            return NativeCallbackStatus::InvalidEntity;
        }

        *x = result.position.x;
        *y = result.position.y;
        *z = result.position.z;
        return NativeCallbackStatus::Ok;
    }

    [[nodiscard]] NativeCallbackStatus GetStateBoolFromManaged(
        std::int64_t contextHandle, const char* key, int* value)
    {
        if (value == nullptr || key == nullptr) return NativeCallbackStatus::InvalidArgument;
        auto* state = FindInstanceByContext(contextHandle);
        if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
        if (!RequireCapability(*state, kCapabilityStatePort, "State.GetBool"))
            return NativeCallbackStatus::CapabilityDenied;
        if (!RequireStatePort(*state, "State.GetBool"))
            return NativeCallbackStatus::WorldUnavailable;
        auto result = options.statePort->GetBool(key);
        if (!result.Succeeded())
        {
            RecordWorldDiagnostics(*state, std::move(result.diagnostics));
            return NativeCallbackStatus::OperationFailed;
        }
        *value = result.value ? 1 : 0;
        return NativeCallbackStatus::Ok;
    }

    [[nodiscard]] NativeCallbackStatus AddStateInt32FromManaged(
        std::int64_t contextHandle, const char* key, std::int32_t delta, std::int32_t* value)
    {
        if (value == nullptr || key == nullptr) return NativeCallbackStatus::InvalidArgument;
        auto* state = FindInstanceByContext(contextHandle);
        if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
        if (!RequireCapability(*state, kCapabilityStatePort, "State.AddInt32"))
            return NativeCallbackStatus::CapabilityDenied;
        if (!RequireStatePort(*state, "State.AddInt32"))
            return NativeCallbackStatus::WorldUnavailable;
        auto result = options.statePort->AddInt32(key, delta);
        if (!result.Succeeded())
        {
            RecordWorldDiagnostics(*state, std::move(result.diagnostics));
            return NativeCallbackStatus::OperationFailed;
        }
        *value = result.value;
        return NativeCallbackStatus::Ok;
    }

    [[nodiscard]] NativeCallbackStatus SetStateBoolFromManaged(
        std::int64_t contextHandle, const char* key, int value)
    {
        if (key == nullptr) return NativeCallbackStatus::InvalidArgument;
        auto* state = FindInstanceByContext(contextHandle);
        if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
        if (!RequireCapability(*state, kCapabilityStatePort, "State.SetBool"))
            return NativeCallbackStatus::CapabilityDenied;
        if (!RequireStatePort(*state, "State.SetBool"))
            return NativeCallbackStatus::WorldUnavailable;
        auto result = options.statePort->SetBool(key, value != 0);
        if (!result.Succeeded())
        {
            RecordWorldDiagnostics(*state, std::move(result.diagnostics));
            return NativeCallbackStatus::OperationFailed;
        }
        return NativeCallbackStatus::Ok;
    }

    [[nodiscard]] NativeCallbackStatus SetStateStringFromManaged(
        std::int64_t contextHandle, const char* key, const char* value)
    {
        if (key == nullptr || value == nullptr) return NativeCallbackStatus::InvalidArgument;
        auto* state = FindInstanceByContext(contextHandle);
        if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
        if (!RequireCapability(*state, kCapabilityStatePort, "State.SetString"))
            return NativeCallbackStatus::CapabilityDenied;
        if (!RequireStatePort(*state, "State.SetString"))
            return NativeCallbackStatus::WorldUnavailable;
        auto result = options.statePort->SetString(key, value);
        if (!result.Succeeded())
        {
            RecordWorldDiagnostics(*state, std::move(result.diagnostics));
            return NativeCallbackStatus::OperationFailed;
        }
        return NativeCallbackStatus::Ok;
    }

    [[nodiscard]] static std::int64_t AllocateContextHandle(Impl* impl)
    {
        std::lock_guard lock(ContextRegistryMutex());
        const auto handle = NextContextHandle()++;
        ContextRegistry().emplace(handle, impl);
        return handle;
    }

    static void ReleaseContextHandle(const std::int64_t contextHandle)
    {
        std::lock_guard lock(ContextRegistryMutex());
        ContextRegistry().erase(contextHandle);
    }

    [[nodiscard]] static Impl* LookupContext(const std::int64_t contextHandle)
    {
        std::lock_guard lock(ContextRegistryMutex());
        const auto iterator = ContextRegistry().find(contextHandle);
        return iterator == ContextRegistry().end() ? nullptr : iterator->second;
    }

    [[nodiscard]] static std::mutex& ContextRegistryMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    [[nodiscard]] static std::unordered_map<std::int64_t, Impl*>&
    ContextRegistry()
    {
        static std::unordered_map<std::int64_t, Impl*> registry;
        return registry;
    }

    [[nodiscard]] static std::int64_t& NextContextHandle()
    {
        static std::int64_t next = 1;
        return next;
    }

    static int LogCallback(
        const std::int64_t contextHandle,
        const int level,
        const char* message) noexcept
    {
        try
        {
            auto* impl = LookupContext(contextHandle);
            return impl == nullptr
                ? static_cast<int>(NativeCallbackStatus::InvalidArgument)
                : static_cast<int>(impl->LogFromManaged(
                      contextHandle,
                      level,
                      message));
        }
        catch (...)
        {
            return static_cast<int>(NativeCallbackStatus::OperationFailed);
        }
    }

    static int CreateEntityCallback(
        const std::int64_t contextHandle,
        const char* name,
        std::int64_t* entityHandle) noexcept
    {
        try
        {
            auto* impl = LookupContext(contextHandle);
            return impl == nullptr
                ? static_cast<int>(NativeCallbackStatus::InvalidArgument)
                : static_cast<int>(impl->CreateEntityFromManaged(
                      contextHandle,
                      name,
                      entityHandle));
        }
        catch (...)
        {
            return static_cast<int>(NativeCallbackStatus::OperationFailed);
        }
    }

    static int SetPositionCallback(
        const std::int64_t contextHandle,
        const std::int64_t entityHandle,
        const float x,
        const float y,
        const float z) noexcept
    {
        try
        {
            auto* impl = LookupContext(contextHandle);
            return impl == nullptr
                ? static_cast<int>(NativeCallbackStatus::InvalidArgument)
                : static_cast<int>(impl->SetPositionFromManaged(
                      contextHandle,
                      entityHandle,
                      x,
                      y,
                      z));
        }
        catch (...)
        {
            return static_cast<int>(NativeCallbackStatus::OperationFailed);
        }
    }

    static int GetPositionCallback(
        const std::int64_t contextHandle,
        const std::int64_t entityHandle,
        float* x,
        float* y,
        float* z) noexcept
    {
        try
        {
            auto* impl = LookupContext(contextHandle);
            return impl == nullptr
                ? static_cast<int>(NativeCallbackStatus::InvalidArgument)
                : static_cast<int>(impl->GetPositionFromManaged(
                      contextHandle,
                      entityHandle,
                      x,
                      y,
                      z));
        }
        catch (...)
        {
            return static_cast<int>(NativeCallbackStatus::OperationFailed);
        }
    }

    static int GetStateBoolCallback(std::int64_t contextHandle, const char* key, int* value) noexcept
    {
        try { auto* impl = LookupContext(contextHandle); return impl == nullptr ? 1 : static_cast<int>(impl->GetStateBoolFromManaged(contextHandle, key, value)); }
        catch (...) { return static_cast<int>(NativeCallbackStatus::OperationFailed); }
    }

    static int AddStateInt32Callback(std::int64_t contextHandle, const char* key, std::int32_t delta, std::int32_t* value) noexcept
    {
        try { auto* impl = LookupContext(contextHandle); return impl == nullptr ? 1 : static_cast<int>(impl->AddStateInt32FromManaged(contextHandle, key, delta, value)); }
        catch (...) { return static_cast<int>(NativeCallbackStatus::OperationFailed); }
    }

    static int SetStateBoolCallback(std::int64_t contextHandle, const char* key, int value) noexcept
    {
        try { auto* impl = LookupContext(contextHandle); return impl == nullptr ? 1 : static_cast<int>(impl->SetStateBoolFromManaged(contextHandle, key, value)); }
        catch (...) { return static_cast<int>(NativeCallbackStatus::OperationFailed); }
    }

    static int SetStateStringCallback(std::int64_t contextHandle, const char* key, const char* value) noexcept
    {
        try { auto* impl = LookupContext(contextHandle); return impl == nullptr ? 1 : static_cast<int>(impl->SetStateStringFromManaged(contextHandle, key, value)); }
        catch (...) { return static_cast<int>(NativeCallbackStatus::OperationFailed); }
    }

    static int StatePortAvailableCallback(std::int64_t contextHandle) noexcept
    {
        try
        {
            auto* impl = LookupContext(contextHandle);
            return impl != nullptr && impl->FindInstanceByContext(contextHandle) != nullptr &&
                   impl->options.statePort != nullptr && impl->options.grantStatePortAccess ? 1 : 0;
        }
        catch (...) { return 0; }
    }

    struct ArtifactEntry
    {
        std::filesystem::path assemblyPath;
        std::filesystem::path runtimeConfigPath;
        std::string scriptId;
        std::vector<std::string> grantedCapabilities;
    };

    [[nodiscard]] std::optional<std::vector<ArtifactEntry>> LoadArtifactEntries(
        const std::filesystem::path& manifestPath,
        ScriptPackageLoadResult& result)
    {
        std::ifstream input(manifestPath);
        if (!input.is_open())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                ScriptHostDiagnostics::ManifestMissing,
                "Script artifact manifest missing",
                "C# script host could not open the script artifact manifest."));
            result.diagnostics.back().metadata["manifestPath"] =
                manifestPath.generic_string();
            return std::nullopt;
        }

        nlohmann::json root;
        try
        {
            input >> root;
        }
        catch (const nlohmann::json::exception& exception)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                ScriptHostDiagnostics::ManifestInvalid,
                "Invalid script artifact manifest",
                std::string("C# script host could not parse the manifest: ") +
                    exception.what()));
            result.diagnostics.back().metadata["manifestPath"] =
                manifestPath.generic_string();
            return std::nullopt;
        }

        if (!root.is_object() || !root.contains("artifacts") ||
            !root["artifacts"].is_array())
        {
            result.diagnostics.push_back(MakeDiagnostic(
                ScriptHostDiagnostics::ManifestInvalid,
                "Invalid script artifact manifest",
                "C# script host requires an artifacts array."));
            result.diagnostics.back().metadata["manifestPath"] =
                manifestPath.generic_string();
            return std::nullopt;
        }

        std::vector<ArtifactEntry> artifacts;
        for (const auto& artifact : root["artifacts"])
        {
            if (!artifact.is_object() ||
                !HasStringField(artifact, "assemblyPath") ||
                !HasStringField(artifact, "runtimeConfigPath"))
            {
                continue;
            }

            artifacts.push_back(ArtifactEntry{
                ReadStringField(artifact, "assemblyPath"),
                ReadStringField(artifact, "runtimeConfigPath"),
                HasStringField(artifact, "scriptId")
                    ? ReadStringField(artifact, "scriptId")
                    : std::string{},
                ReadStringArrayField(artifact, "grantedCapabilities"),
            });
        }

        return artifacts;
    }

    [[nodiscard]] bool ValidateRuntimeConfig(
        const std::filesystem::path& runtimeConfigPath,
        std::vector<ScriptDiagnostic>& diagnostics)
    {
        std::ifstream input(runtimeConfigPath);
        if (!input.is_open())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::RuntimeConfigMissing,
                "Script runtimeconfig missing",
                "C# script host could not open the runtimeconfig file.");
            diagnostic.metadata["runtimeConfigPath"] =
                runtimeConfigPath.generic_string();
            diagnostics.push_back(std::move(diagnostic));
            return false;
        }

        nlohmann::json root;
        try
        {
            input >> root;
        }
        catch (const nlohmann::json::exception& exception)
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::RuntimeConfigInvalid,
                "Invalid script runtimeconfig",
                std::string("C# script host could not parse the runtimeconfig: ") +
                    exception.what());
            diagnostic.metadata["runtimeConfigPath"] =
                runtimeConfigPath.generic_string();
            diagnostics.push_back(std::move(diagnostic));
            return false;
        }

        const auto runtimeOptions = root.find("runtimeOptions");
        if (!root.is_object() || runtimeOptions == root.end() ||
            !runtimeOptions->is_object())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::RuntimeConfigInvalid,
                "Invalid script runtimeconfig",
                "C# script host requires a runtimeOptions object.");
            diagnostic.metadata["runtimeConfigPath"] =
                runtimeConfigPath.generic_string();
            diagnostics.push_back(std::move(diagnostic));
            return false;
        }

        return true;
    }

    [[nodiscard]] bool EnsureRuntime(
        const std::filesystem::path& runtimeConfigPath,
        std::vector<ScriptDiagnostic>& diagnostics)
    {
        if (loadAssembly != nullptr)
        {
            return true;
        }

        static HostFxrApi sharedHostfxr;
        static load_assembly_and_get_function_pointer_fn
            sharedLoadAssemblyAndGetFunctionPointer = nullptr;
        static BridgeLoadAssemblyFn sharedLoadAssembly = nullptr;
        static BridgeCreateInstanceFn sharedCreateInstance = nullptr;
        static BridgeInvokeLifecycleFn sharedInvokeLifecycle = nullptr;
        static BridgeInvokeUiNamedActionFn sharedInvokeUiNamedAction = nullptr;
        static BridgeInvokeInt32BinaryMethodFn sharedInvokeInt32BinaryMethod =
            nullptr;
        static BridgeReleasePackageFn sharedReleasePackage = nullptr;

        if (sharedLoadAssembly != nullptr &&
            sharedCreateInstance != nullptr &&
            sharedInvokeLifecycle != nullptr &&
            sharedInvokeUiNamedAction != nullptr &&
            sharedInvokeInt32BinaryMethod != nullptr &&
            sharedReleasePackage != nullptr)
        {
            loadAssemblyAndGetFunctionPointer =
                sharedLoadAssemblyAndGetFunctionPointer;
            loadAssembly = sharedLoadAssembly;
            createInstance = sharedCreateInstance;
            invokeLifecycle = sharedInvokeLifecycle;
            invokeUiNamedAction = sharedInvokeUiNamedAction;
            invokeInt32BinaryMethod = sharedInvokeInt32BinaryMethod;
            releasePackage = sharedReleasePackage;
            return true;
        }

        if (options.runtimeBridgeAssembly.empty() ||
            !std::filesystem::exists(options.runtimeBridgeAssembly))
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::ManagedBridgeLoadFailed,
                "Managed script bridge missing",
                "C# script host requires SagaScript.RuntimeBridge.dll.");
            diagnostic.metadata["runtimeBridgeAssembly"] =
                options.runtimeBridgeAssembly.generic_string();
            diagnostics.push_back(std::move(diagnostic));
            return false;
        }

        auto api = LoadHostFxr(diagnostics);
        if (!api.has_value())
        {
            return false;
        }

        const auto runtimeConfig = ToHostString(runtimeConfigPath);
        const auto dotnetRoot = options.dotnetRoot.empty()
            ? decltype(ToHostString(options.dotnetRoot)){}
            : ToHostString(options.dotnetRoot);
        const auto hostPath = options.dotnetRoot.empty()
            ? decltype(ToHostString(options.dotnetRoot)){}
            : ToHostString(options.dotnetRoot / "dotnet");

        hostfxr_initialize_parameters parameters{};
        parameters.size = sizeof(parameters);
        parameters.host_path = hostPath.empty() ? nullptr : hostPath.c_str();
        parameters.dotnet_root =
            dotnetRoot.empty() ? nullptr : dotnetRoot.c_str();

        hostfxr_handle context = nullptr;
        const auto initResult = api->initializeForRuntimeConfig(
            runtimeConfig.c_str(),
            &parameters,
            &context);
        if (initResult < 0 || context == nullptr)
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::HostFxrInitializationFailed,
                "C# runtime initialization failed",
                "hostfxr failed to initialize from the script runtimeconfig.");
            diagnostic.metadata["runtimeConfigPath"] =
                runtimeConfigPath.generic_string();
            diagnostic.metadata["hostfxrResult"] = std::to_string(initResult);
            diagnostics.push_back(std::move(diagnostic));
            return false;
        }

        void* runtimeDelegate = nullptr;
        const auto delegateResult = api->getRuntimeDelegate(
            context,
            hdt_load_assembly_and_get_function_pointer,
            &runtimeDelegate);
        static_cast<void>(api->close(context));
        if (delegateResult < 0 || runtimeDelegate == nullptr)
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::RuntimeDelegateResolutionFailed,
                "C# runtime delegate resolution failed",
                "hostfxr could not resolve load_assembly_and_get_function_pointer.");
            diagnostic.metadata["hostfxrResult"] = std::to_string(delegateResult);
            diagnostics.push_back(std::move(diagnostic));
            return false;
        }

        sharedHostfxr = std::move(*api);
        loadAssemblyAndGetFunctionPointer =
            reinterpret_cast<load_assembly_and_get_function_pointer_fn>(
                runtimeDelegate);
        if (!LoadBridgeFunctions(diagnostics))
        {
            return false;
        }

        sharedLoadAssemblyAndGetFunctionPointer =
            loadAssemblyAndGetFunctionPointer;
        sharedLoadAssembly = loadAssembly;
        sharedCreateInstance = createInstance;
        sharedInvokeLifecycle = invokeLifecycle;
        sharedInvokeUiNamedAction = invokeUiNamedAction;
        sharedInvokeInt32BinaryMethod = invokeInt32BinaryMethod;
        sharedReleasePackage = releasePackage;
        return true;
    }

    [[nodiscard]] std::optional<HostFxrApi> LoadHostFxr(
        std::vector<ScriptDiagnostic>& diagnostics)
    {
        const auto dotnetRoot = options.dotnetRoot.empty()
            ? decltype(ToHostString(options.dotnetRoot)){}
            : ToHostString(options.dotnetRoot);

        get_hostfxr_parameters parameters{};
        parameters.size = sizeof(parameters);
        parameters.dotnet_root =
            dotnetRoot.empty() ? nullptr : dotnetRoot.c_str();

        std::array<char_t, 4096> hostfxrPath{};
        std::size_t hostfxrPathSize = hostfxrPath.size();
        const auto pathResult = get_hostfxr_path(
            hostfxrPath.data(),
            &hostfxrPathSize,
            &parameters);
        if (pathResult != 0)
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::HostFxrMissing,
                "hostfxr missing",
                "nethost could not locate hostfxr.");
            diagnostic.metadata["nethostResult"] = std::to_string(pathResult);
            diagnostic.metadata["dotnetRoot"] = options.dotnetRoot.generic_string();
            diagnostics.push_back(std::move(diagnostic));
            return std::nullopt;
        }

        HostFxrApi api;
        api.library = OpenLibrary(std::filesystem::path(hostfxrPath.data()));
        if (api.library == nullptr)
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::HostFxrMissing,
                "hostfxr missing",
                "C# script host could not load the hostfxr library.");
            diagnostic.metadata["hostfxrPath"] =
                std::filesystem::path(hostfxrPath.data()).generic_string();
            diagnostics.push_back(std::move(diagnostic));
            return std::nullopt;
        }

        api.initializeForRuntimeConfig =
            LoadSymbol<hostfxr_initialize_for_runtime_config_fn>(
                api.library,
                "hostfxr_initialize_for_runtime_config");
        api.getRuntimeDelegate = LoadSymbol<hostfxr_get_runtime_delegate_fn>(
            api.library,
            "hostfxr_get_runtime_delegate");
        api.close = LoadSymbol<hostfxr_close_fn>(api.library, "hostfxr_close");

        if (api.initializeForRuntimeConfig == nullptr ||
            api.getRuntimeDelegate == nullptr ||
            api.close == nullptr)
        {
            diagnostics.push_back(MakeDiagnostic(
                ScriptHostDiagnostics::HostFxrMissing,
                "hostfxr missing",
                "C# script host could not resolve required hostfxr exports."));
            return std::nullopt;
        }

        return api;
    }

    [[nodiscard]] bool LoadBridgeFunctions(
        std::vector<ScriptDiagnostic>& diagnostics)
    {
        return LoadBridgeFunction("LoadAssembly", loadAssembly, diagnostics) &&
               LoadBridgeFunction("CreateInstance", createInstance, diagnostics) &&
               LoadBridgeFunction("InvokeLifecycle", invokeLifecycle, diagnostics) &&
               LoadBridgeFunction(
                   "InvokeUiNamedAction",
                   invokeUiNamedAction,
                   diagnostics) &&
               LoadBridgeFunction(
                   "InvokeInt32BinaryMethod",
                   invokeInt32BinaryMethod,
                   diagnostics) &&
               LoadBridgeFunction("ReleasePackage", releasePackage, diagnostics);
    }

    template <typename Function>
    [[nodiscard]] bool LoadBridgeFunction(
        std::string_view methodName,
        Function& function,
        std::vector<ScriptDiagnostic>& diagnostics)
    {
        void* pointer = nullptr;
        const auto bridgePath = ToHostString(options.runtimeBridgeAssembly);
        const auto bridgeType = ToHostString(kBridgeTypeName);
        const auto bridgeMethod = ToHostString(methodName);
        const auto result = loadAssemblyAndGetFunctionPointer(
            bridgePath.c_str(),
            bridgeType.c_str(),
            bridgeMethod.c_str(),
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            &pointer);
        if (result < 0 || pointer == nullptr)
        {
            auto diagnostic = MakeDiagnostic(
                ScriptHostDiagnostics::ManagedBridgeLoadFailed,
                "Managed script bridge load failed",
                "C# script host could not resolve a managed bridge entrypoint.");
            diagnostic.metadata["runtimeBridgeAssembly"] =
                options.runtimeBridgeAssembly.generic_string();
            diagnostic.metadata["methodName"] = std::string(methodName);
            diagnostic.metadata["hostfxrResult"] = std::to_string(result);
            diagnostics.push_back(std::move(diagnostic));
            return false;
        }

        function = reinterpret_cast<Function>(pointer);
        return true;
    }

    CSharpScriptHostOptions options;
    HostFxrApi hostfxr;
    load_assembly_and_get_function_pointer_fn loadAssemblyAndGetFunctionPointer =
        nullptr;
    BridgeLoadAssemblyFn loadAssembly = nullptr;
    BridgeCreateInstanceFn createInstance = nullptr;
    BridgeInvokeLifecycleFn invokeLifecycle = nullptr;
    BridgeInvokeUiNamedActionFn invokeUiNamedAction = nullptr;
    BridgeInvokeInt32BinaryMethodFn invokeInt32BinaryMethod = nullptr;
    BridgeReleasePackageFn releasePackage = nullptr;
    std::uint64_t nextPackageHandle = 1;
    std::uint64_t nextInstanceHandle = 1;
    std::unordered_map<std::uint64_t, PackageState> packages;
    std::unordered_map<std::uint64_t, InstanceState> instances;
    std::unordered_map<std::int64_t, std::uint64_t> contextToInstance;
};

CSharpScriptHost::CSharpScriptHost(CSharpScriptHostOptions options)
    : impl_(std::make_unique<Impl>(std::move(options)))
{
}

CSharpScriptHost::~CSharpScriptHost() = default;

ScriptPackageLoadResult CSharpScriptHost::LoadPackage(
    const ScriptPackageLoadRequest& request)
{
    return impl_->LoadPackage(request);
}

ScriptInstanceCreateResult CSharpScriptHost::CreateInstance(
    const ScriptPackageHandle package,
    const ScriptClassId& classId)
{
    return impl_->CreateInstance(package, classId);
}

ScriptInstanceCreateResult CSharpScriptHost::CreateInstance(
    const ScriptInstanceCreateRequest& request)
{
    return impl_->CreateInstance(request);
}

ScriptHostOperationResult CSharpScriptHost::InvokeLifecycle(
    const ScriptLifecycleInvocation& invocation)
{
    return impl_->InvokeLifecycle(invocation);
}

ScriptHostOperationResult CSharpScriptHost::InvokeUiNamedAction(
    const ScriptUiNamedActionInvocation& invocation)
{
    return impl_->InvokeUiNamedAction(invocation);
}

CSharpInt32BinaryMethodInvocationResult CSharpScriptHost::InvokeInt32BinaryMethod(
    const CSharpInt32BinaryMethodInvocation& invocation)
{
    return impl_->InvokeInt32BinaryMethod(invocation);
}

} // namespace SagaEngine::Scripting
