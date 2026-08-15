// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostCallbacks.cpp

#include "CSharpScriptHostImpl.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <nlohmann/json.hpp>

namespace SagaEngine::Scripting
{
namespace
{
NativeCallbackStatus CopyString(const std::string& value, char* buffer, const int length)
{
    if (buffer == nullptr || length <= 0 ||
        value.size() >= static_cast<std::size_t>(length))
        return NativeCallbackStatus::InvalidArgument;
    std::memcpy(buffer, value.data(), value.size());
    buffer[value.size()] = '\0';
    return NativeCallbackStatus::Ok;
}
}

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

[[nodiscard]] bool CSharpScriptHost::Impl::AdmitOperation(
    InstanceState& state, const SagaShared::Api::SagaOperationId& operationId,
    const std::string_view runtimeBinding)
{
    if (state.operationCatalog != nullptr)
    {
        const auto decision = OperationAdmissionEvaluator::Evaluate(
            {*state.operationCatalog, state.grantedCapabilities,
             state.packageGenerationId, state.packageGenerationHash,
             runtimeBinding}, operationId);
        if (decision.allowed)
            return true;
        auto diagnostic = MakeDiagnostic(
            ScriptHostDiagnostics::OperationAdmissionDenied,
            "Semantic operation denied",
            "Managed script operation was denied by the verified semantic operation policy.");
        diagnostic.metadata["operationId"] = operationId.value;
        diagnostic.metadata["denialReason"] = std::string(ToString(decision.failure));
        if (!decision.missingCapability.empty())
            diagnostic.metadata["capability"] = std::string(decision.missingCapability);
        RecordDiagnostic(state, std::move(diagnostic));
        return false;
    }
    // The manifest-based package loader predates verified generations. Keep its
    // bounded transform capability behavior while canonical launches require the
    // verified semantic catalog above. Unknown operations still fail closed.
    if (operationId.value == kGetPositionOperation.value)
        return RequireCapability(state, kCapabilityTransformRead, "Entity.GetPosition");
    if (operationId.value == kSetPositionOperation.value)
        return RequireCapability(state, kCapabilityTransformWrite, "Entity.SetPosition");
    auto diagnostic = MakeDiagnostic(
        ScriptHostDiagnostics::OperationAdmissionDenied,
        "Semantic operation denied",
        "Managed script operation has no verified semantic operation catalog.");
    diagnostic.metadata["operationId"] = operationId.value;
    diagnostic.metadata["denialReason"] = "UnknownOperation";
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

    if (!AdmitOperation(*state, kSetPositionOperation, "Entity.SetPosition"))
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

    if (!AdmitOperation(*state, kGetPositionOperation, "Entity.GetPosition"))
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

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::GetConfigurationInt32FromManaged(
    const std::int64_t contextHandle, const char* key, std::int32_t* value)
{
    if (key == nullptr || value == nullptr) return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityConfiguration, "Configuration.GetInt32"))
        return NativeCallbackStatus::CapabilityDenied;
    try
    {
        const auto configuration =
            nlohmann::json::parse(state->instanceConfigurationCanonicalJson);
        const auto item = configuration.find(key);
        if (item == configuration.end() || !item->is_number_integer())
            return NativeCallbackStatus::OperationFailed;
        *value = item->get<std::int32_t>();
        return NativeCallbackStatus::Ok;
    }
    catch (const nlohmann::json::exception&)
    {
        return NativeCallbackStatus::OperationFailed;
    }
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::GetConfigurationDoubleFromManaged(
    const std::int64_t contextHandle, const char* key, double* value)
{
    if (key == nullptr || value == nullptr) return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityConfiguration, "Configuration.GetDouble"))
        return NativeCallbackStatus::CapabilityDenied;
    try
    {
        const auto configuration =
            nlohmann::json::parse(state->instanceConfigurationCanonicalJson);
        const auto item = configuration.find(key);
        if (item == configuration.end() || !item->is_number())
            return NativeCallbackStatus::OperationFailed;
        *value = item->get<double>();
        return std::isfinite(*value) ? NativeCallbackStatus::Ok
                                     : NativeCallbackStatus::OperationFailed;
    }
    catch (const nlohmann::json::exception&)
    {
        return NativeCallbackStatus::OperationFailed;
    }
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::GetConfigurationStringFromManaged(
    const std::int64_t contextHandle, const char* key, char* buffer, const int length)
{
    if (key == nullptr) return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityConfiguration, "Configuration.GetString"))
        return NativeCallbackStatus::CapabilityDenied;
    try
    {
        const auto configuration =
            nlohmann::json::parse(state->instanceConfigurationCanonicalJson);
        const auto item = configuration.find(key);
        return item != configuration.end() && item->is_string()
            ? CopyString(item->get<std::string>(), buffer, length)
            : NativeCallbackStatus::OperationFailed;
    }
    catch (const nlohmann::json::exception&)
    {
        return NativeCallbackStatus::OperationFailed;
    }
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::QueryInputFromManaged(
    const std::int64_t contextHandle, const char* actionId, const int inputState, int* value)
{
    if (actionId == nullptr || value == nullptr || inputState < 0 || inputState > 2)
        return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityInput, "Input.Query"))
        return NativeCallbackStatus::CapabilityDenied;
    if (options.inputPort == nullptr) return NativeCallbackStatus::WorldUnavailable;
    const auto result = options.inputPort->Query(
        StateContext(*state), actionId,
        static_cast<ScriptLogicalInputState>(inputState));
    if (!result.Succeeded()) return NativeCallbackStatus::OperationFailed;
    *value = result.value ? 1 : 0;
    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::GetInputValueFromManaged(
    const std::int64_t contextHandle, const char* actionId, double* value)
{
    if (actionId == nullptr || value == nullptr) return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityInput, "Input.GetValue"))
        return NativeCallbackStatus::CapabilityDenied;
    if (options.inputPort == nullptr) return NativeCallbackStatus::WorldUnavailable;
    const auto result = options.inputPort->GetValue(StateContext(*state), actionId);
    if (!result.Succeeded()) return NativeCallbackStatus::OperationFailed;
    *value = result.value;
    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::FindEntityFromManaged(
    const std::int64_t contextHandle, const char* stableId, std::int64_t* entityHandle)
{
    if (stableId == nullptr || entityHandle == nullptr)
        return NativeCallbackStatus::InvalidArgument;
    *entityHandle = 0;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityEntityLookup, "World.FindEntity"))
        return NativeCallbackStatus::CapabilityDenied;
    if (!RequireWorld(*state, "World.FindEntity"))
        return NativeCallbackStatus::WorldUnavailable;
    const auto result = options.world->FindEntity(stableId);
    if (!result.Succeeded()) return NativeCallbackStatus::InvalidEntity;
    *entityHandle = static_cast<std::int64_t>(result.entity.value);
    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::SetEntityActiveFromManaged(
    const std::int64_t contextHandle, const std::int64_t entityHandle, const int active)
{
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr || entityHandle <= 0) return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityEntityActive, "Entity.SetActive"))
        return NativeCallbackStatus::CapabilityDenied;
    if (!RequireWorld(*state, "Entity.SetActive"))
        return NativeCallbackStatus::WorldUnavailable;
    return options.world->SetActive(
               {static_cast<std::uint64_t>(entityHandle)}, active != 0).Succeeded()
        ? NativeCallbackStatus::Ok : NativeCallbackStatus::InvalidEntity;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::GetEntityActiveFromManaged(
    const std::int64_t contextHandle, const std::int64_t entityHandle, int* active)
{
    if (active == nullptr) return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr || entityHandle <= 0) return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityEntityActive, "Entity.GetActive"))
        return NativeCallbackStatus::CapabilityDenied;
    if (!RequireWorld(*state, "Entity.GetActive"))
        return NativeCallbackStatus::WorldUnavailable;
    const auto result =
        options.world->GetActive({static_cast<std::uint64_t>(entityHandle)});
    if (!result.Succeeded()) return NativeCallbackStatus::InvalidEntity;
    *active = result.value ? 1 : 0;
    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::GetStateInt32FromManaged(
    const std::int64_t contextHandle, const char* key, std::int32_t* value)
{
    if (key == nullptr || value == nullptr) return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr || !RequireCapability(*state, kCapabilityStatePort, "State.GetInt32"))
        return state == nullptr ? NativeCallbackStatus::InvalidArgument
                                : NativeCallbackStatus::CapabilityDenied;
    if (!RequireStatePort(*state, "State.GetInt32"))
        return NativeCallbackStatus::WorldUnavailable;
    const auto result = options.statePort->GetInt32(StateContext(*state), key);
    if (!result.Succeeded()) return NativeCallbackStatus::OperationFailed;
    *value = result.value;
    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::GetStateDoubleFromManaged(
    const std::int64_t contextHandle, const char* key, double* value)
{
    if (key == nullptr || value == nullptr) return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr || !RequireCapability(*state, kCapabilityStatePort, "State.GetDouble"))
        return state == nullptr ? NativeCallbackStatus::InvalidArgument
                                : NativeCallbackStatus::CapabilityDenied;
    if (!RequireStatePort(*state, "State.GetDouble"))
        return NativeCallbackStatus::WorldUnavailable;
    const auto result = options.statePort->GetDouble(StateContext(*state), key);
    if (!result.Succeeded()) return NativeCallbackStatus::OperationFailed;
    *value = result.value;
    return NativeCallbackStatus::Ok;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::GetStateStringFromManaged(
    const std::int64_t contextHandle, const char* key, char* buffer, const int length)
{
    if (key == nullptr) return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr || !RequireCapability(*state, kCapabilityStatePort, "State.GetString"))
        return state == nullptr ? NativeCallbackStatus::InvalidArgument
                                : NativeCallbackStatus::CapabilityDenied;
    if (!RequireStatePort(*state, "State.GetString"))
        return NativeCallbackStatus::WorldUnavailable;
    const auto result = options.statePort->GetString(StateContext(*state), key);
    return result.Succeeded() ? CopyString(result.value, buffer, length)
                              : NativeCallbackStatus::OperationFailed;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::SetStateInt32FromManaged(
    const std::int64_t contextHandle, const char* key, const std::int32_t value)
{
    if (key == nullptr) return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr || !RequireCapability(*state, kCapabilityStatePort, "State.SetInt32"))
        return state == nullptr ? NativeCallbackStatus::InvalidArgument
                                : NativeCallbackStatus::CapabilityDenied;
    if (!RequireStatePort(*state, "State.SetInt32"))
        return NativeCallbackStatus::WorldUnavailable;
    return options.statePort->SetInt32(StateContext(*state), key, value).Succeeded()
        ? NativeCallbackStatus::Ok : NativeCallbackStatus::OperationFailed;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::SetStateDoubleFromManaged(
    const std::int64_t contextHandle, const char* key, const double value)
{
    if (key == nullptr || !std::isfinite(value)) return NativeCallbackStatus::InvalidArgument;
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr || !RequireCapability(*state, kCapabilityStatePort, "State.SetDouble"))
        return state == nullptr ? NativeCallbackStatus::InvalidArgument
                                : NativeCallbackStatus::CapabilityDenied;
    if (!RequireStatePort(*state, "State.SetDouble"))
        return NativeCallbackStatus::WorldUnavailable;
    return options.statePort->SetDouble(StateContext(*state), key, value).Succeeded()
        ? NativeCallbackStatus::Ok : NativeCallbackStatus::OperationFailed;
}

[[nodiscard]] NativeCallbackStatus CSharpScriptHost::Impl::RequestExitFromManaged(
    const std::int64_t contextHandle)
{
    auto* state = FindInstanceByContext(contextHandle);
    if (state == nullptr) return NativeCallbackStatus::InvalidArgument;
    if (!RequireCapability(*state, kCapabilityRuntimeExit, "Runtime.RequestExit"))
        return NativeCallbackStatus::CapabilityDenied;
    return options.runtimePort != nullptr &&
        options.runtimePort->RequestExit(StateContext(*state)).Succeeded()
        ? NativeCallbackStatus::Ok : NativeCallbackStatus::OperationFailed;
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

#define SAGA_CALLBACK_BODY(expression)                                                \
    try                                                                               \
    {                                                                                 \
        auto* impl = LookupContext(contextHandle);                                    \
        return impl == nullptr                                                        \
            ? static_cast<int>(NativeCallbackStatus::InvalidArgument)                 \
            : static_cast<int>(expression);                                           \
    }                                                                                 \
    catch (...)                                                                       \
    {                                                                                 \
        return static_cast<int>(NativeCallbackStatus::OperationFailed);               \
    }

int CSharpScriptHost::Impl::GetConfigurationInt32Callback(
    const std::int64_t contextHandle, const char* key, std::int32_t* value) noexcept
{
    SAGA_CALLBACK_BODY(impl->GetConfigurationInt32FromManaged(contextHandle, key, value))
}

int CSharpScriptHost::Impl::GetConfigurationDoubleCallback(
    const std::int64_t contextHandle, const char* key, double* value) noexcept
{
    SAGA_CALLBACK_BODY(impl->GetConfigurationDoubleFromManaged(contextHandle, key, value))
}

int CSharpScriptHost::Impl::GetConfigurationStringCallback(
    const std::int64_t contextHandle, const char* key, char* value, const int length) noexcept
{
    SAGA_CALLBACK_BODY(
        impl->GetConfigurationStringFromManaged(contextHandle, key, value, length))
}

int CSharpScriptHost::Impl::QueryInputCallback(
    const std::int64_t contextHandle, const char* actionId,
    const int state, int* value) noexcept
{
    SAGA_CALLBACK_BODY(impl->QueryInputFromManaged(contextHandle, actionId, state, value))
}

int CSharpScriptHost::Impl::GetInputValueCallback(
    const std::int64_t contextHandle, const char* actionId, double* value) noexcept
{
    SAGA_CALLBACK_BODY(impl->GetInputValueFromManaged(contextHandle, actionId, value))
}

int CSharpScriptHost::Impl::FindEntityCallback(
    const std::int64_t contextHandle, const char* stableId,
    std::int64_t* entityHandle) noexcept
{
    SAGA_CALLBACK_BODY(impl->FindEntityFromManaged(contextHandle, stableId, entityHandle))
}

int CSharpScriptHost::Impl::SetEntityActiveCallback(
    const std::int64_t contextHandle, const std::int64_t entityHandle,
    const int active) noexcept
{
    SAGA_CALLBACK_BODY(
        impl->SetEntityActiveFromManaged(contextHandle, entityHandle, active))
}

int CSharpScriptHost::Impl::GetEntityActiveCallback(
    const std::int64_t contextHandle, const std::int64_t entityHandle,
    int* active) noexcept
{
    SAGA_CALLBACK_BODY(
        impl->GetEntityActiveFromManaged(contextHandle, entityHandle, active))
}

int CSharpScriptHost::Impl::GetStateInt32Callback(
    const std::int64_t contextHandle, const char* key, std::int32_t* value) noexcept
{
    SAGA_CALLBACK_BODY(impl->GetStateInt32FromManaged(contextHandle, key, value))
}

int CSharpScriptHost::Impl::GetStateDoubleCallback(
    const std::int64_t contextHandle, const char* key, double* value) noexcept
{
    SAGA_CALLBACK_BODY(impl->GetStateDoubleFromManaged(contextHandle, key, value))
}

int CSharpScriptHost::Impl::GetStateStringCallback(
    const std::int64_t contextHandle, const char* key, char* value,
    const int length) noexcept
{
    SAGA_CALLBACK_BODY(impl->GetStateStringFromManaged(contextHandle, key, value, length))
}

int CSharpScriptHost::Impl::SetStateInt32Callback(
    const std::int64_t contextHandle, const char* key, const std::int32_t value) noexcept
{
    SAGA_CALLBACK_BODY(impl->SetStateInt32FromManaged(contextHandle, key, value))
}

int CSharpScriptHost::Impl::SetStateDoubleCallback(
    const std::int64_t contextHandle, const char* key, const double value) noexcept
{
    SAGA_CALLBACK_BODY(impl->SetStateDoubleFromManaged(contextHandle, key, value))
}

int CSharpScriptHost::Impl::RequestExitCallback(
    const std::int64_t contextHandle) noexcept
{
    SAGA_CALLBACK_BODY(impl->RequestExitFromManaged(contextHandle))
}

#undef SAGA_CALLBACK_BODY

} // namespace SagaEngine::Scripting
