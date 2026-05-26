/// @file CSharpUiNamedActionHandler.h
/// @brief UI named action handler backed by an existing C# SagaScript instance.

#pragma once

#include "SagaEngine/Scripting/ISagaScriptHost.hpp"
#include "SagaEngine/UI/IUiNamedActionHandler.h"

#include <string>
#include <utility>

namespace SagaEngine::UI
{

struct CSharpUiNamedActionHandlerOptions
{
    Scripting::ISagaScriptHost& host;
    Scripting::ScriptInstanceHandle instance;
    std::string methodName;
};

class CSharpUiNamedActionHandler final : public IUiNamedActionHandler
{
public:
    explicit CSharpUiNamedActionHandler(
        CSharpUiNamedActionHandlerOptions options)
        : host_(options.host)
        , instance_(options.instance)
        , methodName_(std::move(options.methodName))
    {
    }

    [[nodiscard]] UiNamedActionHandlerResult Invoke(
        const UiNamedActionContext& context) override
    {
        UiNamedActionHandlerResult result;
        result.context = context;

        if (!instance_.IsValid() || methodName_.empty())
        {
            result.code = UiNamedActionHandlerDiagnosticCode::HandlerFailed;
            result.severity = UiDiagnosticSeverity::Error;
            result.message =
                "C# UI named action handler target is invalid.";
            return result;
        }

        Scripting::ScriptUiNamedActionInvocation invocation;
        invocation.instance = instance_;
        invocation.methodName = methodName_;
        invocation.context.actionId = context.actionId.value;
        invocation.context.screenId = context.screenId.value;
        invocation.context.elementId = context.elementId.value;
        invocation.context.eventType = ToScriptEventType(context.eventType);
        invocation.context.text = context.text;

        Scripting::ScriptHostOperationResult scriptResult =
            host_.InvokeUiNamedAction(invocation);
        if (scriptResult.Succeeded())
        {
            result.code = UiNamedActionHandlerDiagnosticCode::None;
            result.severity = UiDiagnosticSeverity::Info;
            return result;
        }

        result.code = UiNamedActionHandlerDiagnosticCode::HandlerFailed;
        result.severity = UiDiagnosticSeverity::Error;
        if (scriptResult.diagnostics.empty())
        {
            result.message = "C# UI named action handler failed.";
            return result;
        }

        const auto bridgeError =
            scriptResult.diagnostics.front().metadata.find("bridgeError");
        result.message =
            bridgeError == scriptResult.diagnostics.front().metadata.end() ||
                bridgeError->second.empty()
                ? scriptResult.diagnostics.front().diagnostic.message
                : bridgeError->second;
        return result;
    }

private:
    [[nodiscard]] static Scripting::ScriptUiNamedActionEventType
    ToScriptEventType(const UiEventType eventType) noexcept
    {
        switch (eventType)
        {
        case UiEventType::Click:
            return Scripting::ScriptUiNamedActionEventType::Click;
        case UiEventType::Submit:
            return Scripting::ScriptUiNamedActionEventType::Submit;
        case UiEventType::TextChanged:
            return Scripting::ScriptUiNamedActionEventType::TextChanged;
        case UiEventType::FocusGained:
            return Scripting::ScriptUiNamedActionEventType::FocusGained;
        case UiEventType::FocusLost:
            return Scripting::ScriptUiNamedActionEventType::FocusLost;
        }

        return Scripting::ScriptUiNamedActionEventType::Click;
    }

    Scripting::ISagaScriptHost& host_;
    Scripting::ScriptInstanceHandle instance_;
    std::string methodName_;
};

} // namespace SagaEngine::UI
