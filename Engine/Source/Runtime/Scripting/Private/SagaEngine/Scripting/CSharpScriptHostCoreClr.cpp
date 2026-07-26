// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostCoreClr.cpp

#include <nethost.h>
#include <nlohmann/json.hpp>

#include "CSharpScriptHostImpl.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <fstream>

namespace SagaEngine::Scripting
{

namespace
{

template <typename Function>
[[nodiscard]] Function LoadSymbol(void* library, const char* name)
{
#if defined(_WIN32)
    return reinterpret_cast<Function>(GetProcAddress(static_cast<HMODULE>(library), name));
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

CSharpScriptHost::Impl::HostFxrApi::HostFxrApi(HostFxrApi&& other) noexcept
    : library(std::exchange(other.library, nullptr)),
      initializeForRuntimeConfig(std::exchange(other.initializeForRuntimeConfig, nullptr)),
      getRuntimeDelegate(std::exchange(other.getRuntimeDelegate, nullptr)),
      close(std::exchange(other.close, nullptr))
{}

CSharpScriptHost::Impl::HostFxrApi& CSharpScriptHost::Impl::HostFxrApi::operator=(
    HostFxrApi&& other) noexcept
{
    if (this != &other)
    {
        CloseLibrary(library);
        library = std::exchange(other.library, nullptr);
        initializeForRuntimeConfig = std::exchange(other.initializeForRuntimeConfig, nullptr);
        getRuntimeDelegate = std::exchange(other.getRuntimeDelegate, nullptr);
        close = std::exchange(other.close, nullptr);
    }
    return *this;
}

CSharpScriptHost::Impl::HostFxrApi::~HostFxrApi()
{
    CloseLibrary(library);
}

[[nodiscard]] bool CSharpScriptHost::Impl::ValidateRuntimeConfig(
    const std::filesystem::path& runtimeConfigPath, std::vector<ScriptDiagnostic>& diagnostics)
{
    std::ifstream input(runtimeConfigPath);
    if (!input.is_open())
    {
        auto diagnostic = MakeDiagnostic(ScriptHostDiagnostics::RuntimeConfigMissing,
                                         "Script runtimeconfig missing",
                                         "C# script host could not open the runtimeconfig file.");
        diagnostic.metadata["runtimeConfigPath"] = runtimeConfigPath.generic_string();
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
            std::string("C# script host could not parse the runtimeconfig: ") + exception.what());
        diagnostic.metadata["runtimeConfigPath"] = runtimeConfigPath.generic_string();
        diagnostics.push_back(std::move(diagnostic));
        return false;
    }

    const auto runtimeOptions = root.find("runtimeOptions");
    if (!root.is_object() || runtimeOptions == root.end() || !runtimeOptions->is_object())
    {
        auto diagnostic = MakeDiagnostic(ScriptHostDiagnostics::RuntimeConfigInvalid,
                                         "Invalid script runtimeconfig",
                                         "C# script host requires a runtimeOptions object.");
        diagnostic.metadata["runtimeConfigPath"] = runtimeConfigPath.generic_string();
        diagnostics.push_back(std::move(diagnostic));
        return false;
    }

    return true;
}

[[nodiscard]] bool CSharpScriptHost::Impl::EnsureRuntime(
    const std::filesystem::path& runtimeConfigPath, std::vector<ScriptDiagnostic>& diagnostics)
{
    if (loadAssembly != nullptr)
    {
        return true;
    }

    static HostFxrApi sharedHostfxr;
    static load_assembly_and_get_function_pointer_fn sharedLoadAssemblyAndGetFunctionPointer =
        nullptr;
    static BridgeLoadAssemblyFn sharedLoadAssembly = nullptr;
    static BridgeCreateInstanceFn sharedCreateInstance = nullptr;
    static BridgeInvokeLifecycleFn sharedInvokeLifecycle = nullptr;
    static BridgeInvokeUiNamedActionFn sharedInvokeUiNamedAction = nullptr;
    static BridgeInvokeNamedActionFn sharedInvokeNamedAction = nullptr;
    static BridgeInvokeInt32BinaryMethodFn sharedInvokeInt32BinaryMethod = nullptr;
    static BridgeReleasePackageFn sharedReleasePackage = nullptr;

    if (sharedLoadAssembly != nullptr && sharedCreateInstance != nullptr &&
        sharedInvokeLifecycle != nullptr && sharedInvokeUiNamedAction != nullptr &&
        sharedInvokeNamedAction != nullptr && sharedInvokeInt32BinaryMethod != nullptr &&
        sharedReleasePackage != nullptr)
    {
        loadAssemblyAndGetFunctionPointer = sharedLoadAssemblyAndGetFunctionPointer;
        loadAssembly = sharedLoadAssembly;
        createInstance = sharedCreateInstance;
        invokeLifecycle = sharedInvokeLifecycle;
        invokeUiNamedAction = sharedInvokeUiNamedAction;
        invokeNamedAction = sharedInvokeNamedAction;
        invokeInt32BinaryMethod = sharedInvokeInt32BinaryMethod;
        releasePackage = sharedReleasePackage;
        return true;
    }

    if (options.runtimeBridgeAssembly.empty() ||
        !std::filesystem::exists(options.runtimeBridgeAssembly))
    {
        auto diagnostic = MakeDiagnostic(ScriptHostDiagnostics::ManagedBridgeLoadFailed,
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
    const auto hostPath = options.dotnetRoot.empty() ? decltype(ToHostString(options.dotnetRoot)){}
                                                     : ToHostString(options.dotnetRoot / "dotnet");

    hostfxr_initialize_parameters parameters{};
    parameters.size = sizeof(parameters);
    parameters.host_path = hostPath.empty() ? nullptr : hostPath.c_str();
    parameters.dotnet_root = dotnetRoot.empty() ? nullptr : dotnetRoot.c_str();

    hostfxr_handle context = nullptr;
    const auto initResult = api->initializeForRuntimeConfig(runtimeConfig.c_str(),
                                                            &parameters,
                                                            &context);
    if (initResult < 0 || context == nullptr)
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::HostFxrInitializationFailed,
            "C# runtime initialization failed",
            "hostfxr failed to initialize from the script runtimeconfig.");
        diagnostic.metadata["runtimeConfigPath"] = runtimeConfigPath.generic_string();
        diagnostic.metadata["hostfxrResult"] = std::to_string(initResult);
        diagnostics.push_back(std::move(diagnostic));
        return false;
    }

    void* runtimeDelegate = nullptr;
    const auto delegateResult = api->getRuntimeDelegate(context,
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
    loadAssemblyAndGetFunctionPointer = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(
        runtimeDelegate);
    if (!LoadBridgeFunctions(diagnostics))
    {
        return false;
    }

    sharedLoadAssemblyAndGetFunctionPointer = loadAssemblyAndGetFunctionPointer;
    sharedLoadAssembly = loadAssembly;
    sharedCreateInstance = createInstance;
    sharedInvokeLifecycle = invokeLifecycle;
    sharedInvokeUiNamedAction = invokeUiNamedAction;
    sharedInvokeNamedAction = invokeNamedAction;
    sharedInvokeInt32BinaryMethod = invokeInt32BinaryMethod;
    sharedReleasePackage = releasePackage;
    return true;
}

[[nodiscard]] std::optional<CSharpScriptHost::Impl::HostFxrApi> CSharpScriptHost::Impl::LoadHostFxr(
    std::vector<ScriptDiagnostic>& diagnostics)
{
    const auto dotnetRoot = options.dotnetRoot.empty()
                                ? decltype(ToHostString(options.dotnetRoot)){}
                                : ToHostString(options.dotnetRoot);

    get_hostfxr_parameters parameters{};
    parameters.size = sizeof(parameters);
    parameters.dotnet_root = dotnetRoot.empty() ? nullptr : dotnetRoot.c_str();

    std::array<char_t, 4096> hostfxrPath{};
    std::size_t hostfxrPathSize = hostfxrPath.size();
    const auto pathResult = get_hostfxr_path(hostfxrPath.data(), &hostfxrPathSize, &parameters);
    if (pathResult != 0)
    {
        auto diagnostic = MakeDiagnostic(ScriptHostDiagnostics::HostFxrMissing,
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
        auto diagnostic = MakeDiagnostic(ScriptHostDiagnostics::HostFxrMissing,
                                         "hostfxr missing",
                                         "C# script host could not load the hostfxr library.");
        diagnostic.metadata["hostfxrPath"] =
            std::filesystem::path(hostfxrPath.data()).generic_string();
        diagnostics.push_back(std::move(diagnostic));
        return std::nullopt;
    }

    api.initializeForRuntimeConfig = LoadSymbol<hostfxr_initialize_for_runtime_config_fn>(
        api.library, "hostfxr_initialize_for_runtime_config");
    api.getRuntimeDelegate = LoadSymbol<hostfxr_get_runtime_delegate_fn>(
        api.library, "hostfxr_get_runtime_delegate");
    api.close = LoadSymbol<hostfxr_close_fn>(api.library, "hostfxr_close");

    if (api.initializeForRuntimeConfig == nullptr || api.getRuntimeDelegate == nullptr ||
        api.close == nullptr)
    {
        diagnostics.push_back(
            MakeDiagnostic(ScriptHostDiagnostics::HostFxrMissing,
                           "hostfxr missing",
                           "C# script host could not resolve required hostfxr exports."));
        return std::nullopt;
    }

    return api;
}

[[nodiscard]] bool CSharpScriptHost::Impl::LoadBridgeFunctions(
    std::vector<ScriptDiagnostic>& diagnostics)
{
    return LoadBridgeFunction("LoadAssembly", loadAssembly, diagnostics) &&
           LoadBridgeFunction("CreateInstance", createInstance, diagnostics) &&
           LoadBridgeFunction("InvokeLifecycle", invokeLifecycle, diagnostics) &&
           LoadBridgeFunction("InvokeUiNamedAction", invokeUiNamedAction, diagnostics) &&
           LoadBridgeFunction("InvokeNamedAction", invokeNamedAction, diagnostics) &&
           LoadBridgeFunction("InvokeInt32BinaryMethod", invokeInt32BinaryMethod, diagnostics) &&
           LoadBridgeFunction("ReleasePackage", releasePackage, diagnostics);
}

template <typename Function>
[[nodiscard]] bool CSharpScriptHost::Impl::LoadBridgeFunction(
    std::string_view methodName, Function& function, std::vector<ScriptDiagnostic>& diagnostics)
{
    void* pointer = nullptr;
    const auto bridgePath = ToHostString(options.runtimeBridgeAssembly);
    const auto bridgeType = ToHostString(kBridgeTypeName);
    const auto bridgeMethod = ToHostString(methodName);
    const auto result = loadAssemblyAndGetFunctionPointer(bridgePath.c_str(),
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

} // namespace SagaEngine::Scripting
