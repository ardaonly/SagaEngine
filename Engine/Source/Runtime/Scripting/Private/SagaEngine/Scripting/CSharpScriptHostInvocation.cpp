// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostInvocation.cpp

#include "CSharpScriptHostImpl.hpp"

namespace SagaEngine::Scripting
{

[[nodiscard]] ScriptHostOperationResult CSharpScriptHost::Impl::InvokeLifecycle(
    const ScriptLifecycleInvocation& invocation)
{
    ScriptHostOperationResult result;
    const auto instanceIterator = instances.find(invocation.instance.value);
    if (instanceIterator == instances.end())
    {
        result.diagnostics.push_back(
            MakeDiagnostic(ScriptHostDiagnostics::InvalidInstanceHandle,
                           "Invalid script instance handle",
                           "C# script host cannot invoke lifecycle on an unknown instance."));
        return result;
    }

    instanceIterator->second.pendingDiagnostics.clear();
    std::array<char, kBridgeErrorBufferSize> error{};
    const auto status = static_cast<BridgeStatus>(
        invokeLifecycle(instanceIterator->second.bridgeInstanceHandle,
                        static_cast<int>(invocation.method),
                        static_cast<float>(invocation.deltaTimeSeconds),
                        error.data(),
                        static_cast<int>(error.size())));
    if (status == BridgeStatus::Ok)
    {
        result.diagnostics = std::move(instanceIterator->second.pendingDiagnostics);
        result.succeeded = result.diagnostics.empty();
        if (invocation.method == ScriptLifecycleMethod::OnDestroy && result.succeeded)
        {
            const InstanceIdentity identity{
                instanceIterator->second.selfEntity.value,
                instanceIterator->second.scriptId,
            };
            ReleaseContextHandle(instanceIterator->second.contextHandle);
            contextToInstance.erase(instanceIterator->second.contextHandle);
            activeIdentities.erase(identity);
            instances.erase(instanceIterator);
        }
        return result;
    }

    const auto code = CodeForBridgeStatus(status, "lifecycle");
    auto diagnostic = MakeDiagnostic(code,
                                     TitleForDiagnostic(code),
                                     "C# script host failed while invoking a lifecycle method.");
    diagnostic.metadata["lifecycleMethod"] = std::string(ToString(invocation.method));
    diagnostic.metadata["bridgeError"] = BridgeError(error);
    result.diagnostics.push_back(std::move(diagnostic));
    for (auto& pending : instanceIterator->second.pendingDiagnostics)
    {
        result.diagnostics.push_back(std::move(pending));
    }
    instanceIterator->second.pendingDiagnostics.clear();
    return result;
}

[[nodiscard]] ScriptHostOperationResult CSharpScriptHost::Impl::InvokeUiNamedAction(
    const ScriptUiNamedActionInvocation& invocation)
{
    ScriptHostOperationResult result;
    const auto instanceIterator = instances.find(invocation.instance.value);
    if (instanceIterator == instances.end())
    {
        result.diagnostics.push_back(
            MakeDiagnostic(ScriptHostDiagnostics::InvalidInstanceHandle,
                           "Invalid script instance handle",
                           "C# script host cannot invoke UI named action on an unknown instance."));
        return result;
    }

    instanceIterator->second.pendingDiagnostics.clear();
    std::array<char, kBridgeErrorBufferSize> error{};
    const auto status = static_cast<BridgeStatus>(
        invokeUiNamedAction(instanceIterator->second.bridgeInstanceHandle,
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
        result.diagnostics = std::move(instanceIterator->second.pendingDiagnostics);
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

[[nodiscard]] ScriptHostOperationResult CSharpScriptHost::Impl::InvokeNamedAction(
    const ScriptNamedActionInvocation& invocation)
{
    ScriptHostOperationResult result;
    const auto instanceIterator = instances.find(invocation.instance.value);
    if (instanceIterator == instances.end())
    {
        result.diagnostics.push_back(
            MakeDiagnostic(ScriptHostDiagnostics::InvalidInstanceHandle,
                           "Invalid script instance handle",
                           "C# script host cannot invoke a named action on an unknown instance."));
        return result;
    }

    instanceIterator->second.pendingDiagnostics.clear();
    std::array<char, kBridgeErrorBufferSize> error{};
    const auto status = static_cast<BridgeStatus>(
        invokeNamedAction(instanceIterator->second.bridgeInstanceHandle,
                          invocation.methodName.c_str(),
                          invocation.actionId.c_str(),
                          error.data(),
                          static_cast<int>(error.size())));
    if (status == BridgeStatus::Ok)
    {
        result.diagnostics = std::move(instanceIterator->second.pendingDiagnostics);
        result.succeeded = result.diagnostics.empty();
        return result;
    }

    const auto code = CodeForBridgeStatus(status, "namedAction");
    auto diagnostic = MakeDiagnostic(
        code,
        TitleForDiagnostic(code),
        "C# script host failed while invoking a verified named action.");
    diagnostic.scriptId = instanceIterator->second.scriptId;
    diagnostic.metadata["classId"] = instanceIterator->second.classId;
    diagnostic.metadata["scriptId"] = instanceIterator->second.scriptId;
    diagnostic.metadata["methodName"] = invocation.methodName;
    diagnostic.metadata["actionId"] = invocation.actionId;
    diagnostic.metadata["bridgeError"] = BridgeError(error);
    result.diagnostics.push_back(std::move(diagnostic));
    for (auto& pending : instanceIterator->second.pendingDiagnostics)
        result.diagnostics.push_back(std::move(pending));
    instanceIterator->second.pendingDiagnostics.clear();
    return result;
}

[[nodiscard]] CSharpInt32BinaryMethodInvocationResult
CSharpScriptHost::Impl::InvokeInt32BinaryMethod(const CSharpInt32BinaryMethodInvocation& invocation)
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
    const auto status = static_cast<BridgeStatus>(
        invokeInt32BinaryMethod(instanceIterator->second.bridgeInstanceHandle,
                                invocation.methodName.c_str(),
                                invocation.left,
                                invocation.right,
                                &value,
                                error.data(),
                                static_cast<int>(error.size())));
    if (status == BridgeStatus::Ok)
    {
        result.value = value;
        result.diagnostics = std::move(instanceIterator->second.pendingDiagnostics);
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

} // namespace SagaEngine::Scripting
