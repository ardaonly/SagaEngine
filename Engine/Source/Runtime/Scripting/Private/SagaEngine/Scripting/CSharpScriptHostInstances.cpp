// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostInstances.cpp

#include "CSharpScriptHostImpl.hpp"

namespace SagaEngine::Scripting
{

CSharpScriptHost::Impl::Impl(CSharpScriptHostOptions hostOptions) : options(std::move(hostOptions))
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

CSharpScriptHost::Impl::~Impl()
{
    for (const auto& [_, instance] : instances)
    {
        if (options.statePort != nullptr)
            options.statePort->OnInstanceDeactivated(StateContext(instance));
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

std::size_t CSharpScriptHost::Impl::InstanceIdentityHash::operator()(
    const InstanceIdentity& identity) const noexcept
{
    const auto entityHash = std::hash<std::uint64_t>{}(identity.entity);
    const auto scriptHash = std::hash<std::string>{}(identity.scriptId);
    return entityHash ^ (scriptHash + 0x9e3779b9U + (entityHash << 6U) + (entityHash >> 2U));
}

[[nodiscard]] ScriptInstanceCreateResult CSharpScriptHost::Impl::CreateInstance(
    ScriptPackageHandle package, const ScriptClassId& classId)
{
    ScriptInstanceCreateRequest request;
    request.package = package;
    request.classId = classId;
    return CreateInstance(request);
}

[[nodiscard]] ScriptInstanceCreateResult CSharpScriptHost::Impl::CreateInstance(
    const ScriptInstanceCreateRequest& request)
{
    ScriptInstanceCreateResult result;
    const auto packageIterator = packages.find(request.package.value);
    if (packageIterator == packages.end())
    {
        result.diagnostics.push_back(
            MakeDiagnostic(ScriptHostDiagnostics::InvalidPackageHandle,
                           "Invalid script package handle",
                           "C# script host cannot create an instance from an unknown package."));
        return result;
    }

    ScriptDiagnostic lastMissingDiagnostic;
    bool sawMissing = false;
    const auto scriptId = ResolveScriptId(packageIterator->second, request);
    const auto grantedCapabilities = ResolveGrantedCapabilities(packageIterator->second, scriptId);
    const InstanceIdentity identity{request.selfEntity.value, scriptId};
    if (activeIdentities.contains(identity))
    {
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::InstanceCreationFailed,
            "Script instance creation failed",
            "C# script host rejected a duplicate active entity/script identity.");
        diagnostic.scriptId = scriptId;
        diagnostic.metadata["classId"] = request.classId.value;
        diagnostic.metadata["scriptId"] = scriptId;
        diagnostic.metadata["entityHandle"] = std::to_string(request.selfEntity.value);
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }
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
        &CSharpScriptHost::Impl::GetConfigurationInt32Callback,
        &CSharpScriptHost::Impl::GetConfigurationDoubleCallback,
        &CSharpScriptHost::Impl::GetConfigurationStringCallback,
        &CSharpScriptHost::Impl::QueryInputCallback,
        &CSharpScriptHost::Impl::GetInputValueCallback,
        &CSharpScriptHost::Impl::FindEntityCallback,
        &CSharpScriptHost::Impl::SetEntityActiveCallback,
        &CSharpScriptHost::Impl::GetEntityActiveCallback,
        &CSharpScriptHost::Impl::GetStateInt32Callback,
        &CSharpScriptHost::Impl::GetStateDoubleCallback,
        &CSharpScriptHost::Impl::GetStateStringCallback,
        &CSharpScriptHost::Impl::SetStateInt32Callback,
        &CSharpScriptHost::Impl::SetStateDoubleCallback,
        &CSharpScriptHost::Impl::RequestExitCallback,
    };

    for (const auto& managedPackage : packageIterator->second.managedPackages)
    {
        std::array<char, kBridgeErrorBufferSize> error{};
        std::int64_t bridgeInstanceHandle = 0;
        const auto status = static_cast<BridgeStatus>(
            createInstance(managedPackage.bridgeHandle,
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
            state.instance.value = instanceHandle;
            state.bridgeInstanceHandle = bridgeInstanceHandle;
            state.contextHandle = contextHandle;
            state.classId = request.classId.value;
            state.scriptId = scriptId;
            state.selfEntity = request.selfEntity;
            state.activeInstanceToken = nextActiveInstanceToken++;
            state.grantedCapabilities = grantedCapabilities;
            state.operationCatalog = packageIterator->second.operationCatalog;
            state.packageGenerationId = packageIterator->second.packageGenerationId;
            state.packageGenerationHash = packageIterator->second.packageGenerationHash;
            state.instanceConfigurationCanonicalJson =
                request.instanceConfigurationCanonicalJson;
            auto [inserted, _] = instances.emplace(instanceHandle, std::move(state));
            contextToInstance.emplace(contextHandle, instanceHandle);
            activeIdentities.emplace(identity);
            if (options.statePort != nullptr)
                options.statePort->OnInstanceActivated(StateContext(inserted->second));
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
        diagnostic.metadata["assemblyPath"] = managedPackage.assemblyPath.generic_string();
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

ScriptStateContext CSharpScriptHost::Impl::StateContext(const InstanceState& state)
{
    return {state.instance, state.selfEntity, state.scriptId, state.scriptId,
            state.activeInstanceToken};
}

[[nodiscard]] std::string CSharpScriptHost::Impl::ResolveScriptId(
    const PackageState& package, const ScriptInstanceCreateRequest& request) const
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

[[nodiscard]] CSharpScriptHost::Impl::CapabilitySet
CSharpScriptHost::Impl::ResolveGrantedCapabilities(const PackageState& package,
                                                   const std::string& scriptId) const
{
    const auto iterator = package.grantedCapabilitiesByScriptId.find(scriptId);
    return iterator == package.grantedCapabilitiesByScriptId.end() ? CapabilitySet{}
                                                                   : iterator->second;
}

[[nodiscard]] CSharpScriptHost::Impl::InstanceState* CSharpScriptHost::Impl::FindInstanceByContext(
    const std::int64_t contextHandle)
{
    const auto contextIterator = contextToInstance.find(contextHandle);
    if (contextIterator == contextToInstance.end())
    {
        return nullptr;
    }

    const auto instanceIterator = instances.find(contextIterator->second);
    return instanceIterator == instances.end() ? nullptr : &instanceIterator->second;
}

[[nodiscard]] std::int64_t CSharpScriptHost::Impl::AllocateContextHandle(Impl* impl)
{
    std::lock_guard lock(ContextRegistryMutex());
    const auto handle = NextContextHandle()++;
    ContextRegistry().emplace(handle, impl);
    return handle;
}

void CSharpScriptHost::Impl::ReleaseContextHandle(const std::int64_t contextHandle)
{
    std::lock_guard lock(ContextRegistryMutex());
    ContextRegistry().erase(contextHandle);
}

[[nodiscard]] CSharpScriptHost::Impl* CSharpScriptHost::Impl::LookupContext(
    const std::int64_t contextHandle)
{
    std::lock_guard lock(ContextRegistryMutex());
    const auto iterator = ContextRegistry().find(contextHandle);
    return iterator == ContextRegistry().end() ? nullptr : iterator->second;
}

[[nodiscard]] std::mutex& CSharpScriptHost::Impl::ContextRegistryMutex()
{
    static std::mutex mutex;
    return mutex;
}

[[nodiscard]] std::unordered_map<std::int64_t, CSharpScriptHost::Impl*>&
CSharpScriptHost::Impl::ContextRegistry()
{
    static std::unordered_map<std::int64_t, Impl*> registry;
    return registry;
}

[[nodiscard]] std::int64_t& CSharpScriptHost::Impl::NextContextHandle()
{
    static std::int64_t next = 1;
    return next;
}

} // namespace SagaEngine::Scripting
