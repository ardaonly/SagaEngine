// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostCallbacks.cpp

#include "CSharpScriptHostImpl.hpp"

namespace SagaEngine::Scripting
{

void CSharpScriptHost::Impl::RecordDiagnostic(InstanceState& state, ScriptDiagnostic diagnostic)
{
    if (diagnostic.scriptId.empty())
    {
        diagnostic.scriptId = state.scriptId;
    }
    diagnostic.metadata["classId"] = state.classId;
    diagnostic.metadata["scriptId"] = state.scriptId;
    state.pendingDiagnostics.push_back(std::move(diagnostic));
}

[[nodiscard]] bool CSharpScriptHost::Impl::RequireCapability(InstanceState& state,
                                                             std::string_view capability,
                                                             std::string operation)
{
    if (capability == kCapabilityStatePort && options.grantStatePortAccess)
    {
        return true;
    }
    if (state.grantedCapabilities.find(std::string(capability)) != state.grantedCapabilities.end())
    {
        return true;
    }
    if ((capability == kCapabilityTransformRead || capability == kCapabilityTransformWrite) &&
        state.grantedCapabilities.contains(std::string(kCapabilityTransformReadWrite)))
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

[[nodiscard]] bool CSharpScriptHost::Impl::RequireWorld(InstanceState& state, std::string operation)
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

[[nodiscard]] bool CSharpScriptHost::Impl::RequireStatePort(InstanceState& state,
                                                            std::string operation)
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

void CSharpScriptHost::Impl::RecordInvalidEntityHandle(InstanceState& state,
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

void CSharpScriptHost::Impl::RecordWorldDiagnostics(InstanceState& state,
                                                    std::vector<ScriptDiagnostic> diagnostics)
{
    if (diagnostics.empty())
    {
        auto diagnostic = MakeDiagnostic(ScriptHostDiagnostics::LifecycleFailed,
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

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::LogFromManaged(
    const std::int64_t contextHandle, const int level, const char* message)
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
        event.level = level == 1 ? ScriptLogLevel::Warn
                                 : (level == 2 ? ScriptLogLevel::Error : ScriptLogLevel::Info);
        event.message = message == nullptr ? std::string{} : std::string(message);
        options.logSink(event);
    }

    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::CreateEntityFromManaged(
    const std::int64_t contextHandle, const char* name, std::int64_t* entityHandle)
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

    if (!RequireCapability(*state, kCapabilityCreateEntity, "World.CreateEntity"))
    {
        return NativeCallbackStatus::CapabilityDenied;
    }
    if (!RequireWorld(*state, "World.CreateEntity"))
    {
        return NativeCallbackStatus::WorldUnavailable;
    }

    auto result = options.world->CreateEntity(name == nullptr ? std::string{} : std::string(name));
    if (!result.Succeeded())
    {
        RecordWorldDiagnostics(*state, std::move(result.diagnostics));
        return NativeCallbackStatus::OperationFailed;
    }

    *entityHandle = static_cast<std::int64_t>(result.entity.value);
    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::SetPositionFromManaged(
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

    if (!RequireCapability(*state, kCapabilityTransformWrite, "Entity.SetPosition"))
    {
        return NativeCallbackStatus::CapabilityDenied;
    }
    if (!RequireWorld(*state, "Entity.SetPosition"))
    {
        return NativeCallbackStatus::WorldUnavailable;
    }
    if (entityHandle <= 0)
    {
        RecordInvalidEntityHandle(*state, entityHandle, "Entity.SetPosition");
        return NativeCallbackStatus::InvalidEntity;
    }

    auto result = options.world->SetPosition(
        ScriptEntityHandle{static_cast<std::uint64_t>(entityHandle)}, ScriptVector3{x, y, z});
    if (!result.Succeeded())
    {
        RecordWorldDiagnostics(*state, std::move(result.diagnostics));
        return NativeCallbackStatus::InvalidEntity;
    }

    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::GetPositionFromManaged(
    const std::int64_t contextHandle, const std::int64_t entityHandle, float* x, float* y, float* z)
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

    if (!RequireCapability(*state, kCapabilityTransformRead, "Entity.GetPosition"))
    {
        return NativeCallbackStatus::CapabilityDenied;
    }
    if (!RequireWorld(*state, "Entity.GetPosition"))
    {
        return NativeCallbackStatus::WorldUnavailable;
    }
    if (entityHandle <= 0)
    {
        RecordInvalidEntityHandle(*state, entityHandle, "Entity.GetPosition");
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

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::GetStateBoolFromManaged(
    std::int64_t contextHandle, const char* key, int* value)
{
    if (value == nullptr || key == nullptr)
        return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr)
        return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityStatePort, "State.GetBool"))
        return NativeCallbackStatus::CapabilityDenied;
    if (!RequireStatePort(*state, "State.GetBool"))
        return NativeCallbackStatus::WorldUnavailable;
    auto result = options.statePort->GetBool(StateContext(*state), key);
    if (!result.Succeeded())
    {
        RecordWorldDiagnostics(*state, std::move(result.diagnostics));
        return NativeCallbackStatus::OperationFailed;
    }
    *value = result.value ? 1 : 0;
    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::AddStateInt32FromManaged(
    std::int64_t contextHandle, const char* key, std::int32_t delta, std::int32_t* value)
{
    if (value == nullptr || key == nullptr)
        return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr)
        return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityStatePort, "State.AddInt32"))
        return NativeCallbackStatus::CapabilityDenied;
    if (!RequireStatePort(*state, "State.AddInt32"))
        return NativeCallbackStatus::WorldUnavailable;
    auto result = options.statePort->AddInt32(StateContext(*state), key, delta);
    if (!result.Succeeded())
    {
        RecordWorldDiagnostics(*state, std::move(result.diagnostics));
        return NativeCallbackStatus::OperationFailed;
    }
    *value = result.value;
    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::SetStateBoolFromManaged(
    std::int64_t contextHandle, const char* key, int value)
{
    if (key == nullptr)
        return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr)
        return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityStatePort, "State.SetBool"))
        return NativeCallbackStatus::CapabilityDenied;
    if (!RequireStatePort(*state, "State.SetBool"))
        return NativeCallbackStatus::WorldUnavailable;
    auto result = options.statePort->SetBool(StateContext(*state), key, value != 0);
    if (!result.Succeeded())
    {
        RecordWorldDiagnostics(*state, std::move(result.diagnostics));
        return NativeCallbackStatus::OperationFailed;
    }
    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::SetStateStringFromManaged(
    std::int64_t contextHandle, const char* key, const char* value)
{
    if (key == nullptr || value == nullptr)
        return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr)
        return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityStatePort, "State.SetString"))
        return NativeCallbackStatus::CapabilityDenied;
    if (!RequireStatePort(*state, "State.SetString"))
        return NativeCallbackStatus::WorldUnavailable;
    auto result = options.statePort->SetString(StateContext(*state), key, value);
    if (!result.Succeeded())
    {
        RecordWorldDiagnostics(*state, std::move(result.diagnostics));
        return NativeCallbackStatus::OperationFailed;
    }
    return NativeCallbackStatus::Ok;
}

int CSharpScriptHost::Impl::LogCallback(const std::int64_t contextHandle,
                                        const int level,
                                        const char* message) noexcept
{
    try
    {
        auto* impl = LookupContext(contextHandle);
        return impl == nullptr
                   ? static_cast<int>(NativeCallbackStatus::InvalidArgument)
                   : static_cast<int>(impl->LogFromManaged(contextHandle, level, message));
    }
    catch (...)
    {
        return static_cast<int>(NativeCallbackStatus::OperationFailed);
    }
}

int CSharpScriptHost::Impl::CreateEntityCallback(const std::int64_t contextHandle,
                                                 const char* name,
                                                 std::int64_t* entityHandle) noexcept
{
    try
    {
        auto* impl = LookupContext(contextHandle);
        return impl == nullptr
                   ? static_cast<int>(NativeCallbackStatus::InvalidArgument)
                   : static_cast<int>(
                         impl->CreateEntityFromManaged(contextHandle, name, entityHandle));
    }
    catch (...)
    {
        return static_cast<int>(NativeCallbackStatus::OperationFailed);
    }
}

int CSharpScriptHost::Impl::SetPositionCallback(const std::int64_t contextHandle,
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
                   : static_cast<int>(
                         impl->SetPositionFromManaged(contextHandle, entityHandle, x, y, z));
    }
    catch (...)
    {
        return static_cast<int>(NativeCallbackStatus::OperationFailed);
    }
}

int CSharpScriptHost::Impl::GetPositionCallback(const std::int64_t contextHandle,
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
                   : static_cast<int>(
                         impl->GetPositionFromManaged(contextHandle, entityHandle, x, y, z));
    }
    catch (...)
    {
        return static_cast<int>(NativeCallbackStatus::OperationFailed);
    }
}

int CSharpScriptHost::Impl::GetStateBoolCallback(std::int64_t contextHandle,
                                                 const char* key,
                                                 int* value) noexcept
{
    try
    {
        auto* impl = LookupContext(contextHandle);
        return impl == nullptr
                   ? 1
                   : static_cast<int>(impl->GetStateBoolFromManaged(contextHandle, key, value));
    }
    catch (...)
    {
        return static_cast<int>(NativeCallbackStatus::OperationFailed);
    }
}

int CSharpScriptHost::Impl::AddStateInt32Callback(std::int64_t contextHandle,
                                                  const char* key,
                                                  std::int32_t delta,
                                                  std::int32_t* value) noexcept
{
    try
    {
        auto* impl = LookupContext(contextHandle);
        return impl == nullptr
                   ? 1
                   : static_cast<int>(
                         impl->AddStateInt32FromManaged(contextHandle, key, delta, value));
    }
    catch (...)
    {
        return static_cast<int>(NativeCallbackStatus::OperationFailed);
    }
}

int CSharpScriptHost::Impl::SetStateBoolCallback(std::int64_t contextHandle,
                                                 const char* key,
                                                 int value) noexcept
{
    try
    {
        auto* impl = LookupContext(contextHandle);
        return impl == nullptr
                   ? 1
                   : static_cast<int>(impl->SetStateBoolFromManaged(contextHandle, key, value));
    }
    catch (...)
    {
        return static_cast<int>(NativeCallbackStatus::OperationFailed);
    }
}

int CSharpScriptHost::Impl::SetStateStringCallback(std::int64_t contextHandle,
                                                   const char* key,
                                                   const char* value) noexcept
{
    try
    {
        auto* impl = LookupContext(contextHandle);
        return impl == nullptr
                   ? 1
                   : static_cast<int>(impl->SetStateStringFromManaged(contextHandle, key, value));
    }
    catch (...)
    {
        return static_cast<int>(NativeCallbackStatus::OperationFailed);
    }
}

int CSharpScriptHost::Impl::StatePortAvailableCallback(std::int64_t contextHandle) noexcept
{
    try
    {
        auto* impl = LookupContext(contextHandle);
        return impl != nullptr && impl->FindInstanceByContext(contextHandle) != nullptr &&
                       impl->options.statePort != nullptr && impl->options.grantStatePortAccess
                   ? 1
                   : 0;
    }
    catch (...)
    {
        return 0;
    }
}

} // namespace SagaEngine::Scripting
