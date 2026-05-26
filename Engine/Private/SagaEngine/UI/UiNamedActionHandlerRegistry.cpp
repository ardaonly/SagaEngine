/// @file UiNamedActionHandlerRegistry.cpp
/// @brief Default backend-neutral UI named action handler registry.

#include "SagaEngine/UI/IUiNamedActionHandler.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace SagaEngine::UI
{
namespace
{

[[nodiscard]] UiNamedActionContext MakeContext(const UiNamedAction& action)
{
    UiNamedActionContext context;
    context.actionId = action.actionId;
    context.screenId = action.sourceEvent.screenId;
    context.documentId = action.sourceEvent.documentId;
    context.document = action.sourceEvent.document;
    context.elementId = action.sourceEvent.elementId;
    context.eventType = action.sourceEvent.type;
    context.text = action.sourceEvent.text;
    context.sourceEvent = action.sourceEvent;
    return context;
}

[[nodiscard]] UiNamedActionHandlerResult MakeResult(
    UiNamedActionHandlerDiagnosticCode code,
    UiDiagnosticSeverity severity,
    std::string message,
    UiNamedActionContext context = {})
{
    UiNamedActionHandlerResult result;
    result.code = code;
    result.severity = severity;
    result.message = std::move(message);
    result.context = std::move(context);
    return result;
}

class UiNamedActionHandlerRegistry final
    : public IUiNamedActionHandlerRegistry
{
public:
    [[nodiscard]] UiNamedActionHandlerResult RegisterHandler(
        UiActionId actionId,
        std::shared_ptr<IUiNamedActionHandler> handler) override
    {
        UiNamedActionContext context;
        context.actionId = actionId;

        if (!actionId.IsSet() || !handler)
        {
            return AddDiagnostic(MakeResult(
                UiNamedActionHandlerDiagnosticCode::InvalidAction,
                UiDiagnosticSeverity::Error,
                "UI named action handler registration is invalid.",
                std::move(context)));
        }

        if (handlers_.find(actionId.value) != handlers_.end())
        {
            return AddDiagnostic(MakeResult(
                UiNamedActionHandlerDiagnosticCode::DuplicateHandler,
                UiDiagnosticSeverity::Warning,
                "UI named action handler already exists.",
                std::move(context)));
        }

        handlers_.emplace(actionId.value, std::move(handler));
        return MakeResult(
            UiNamedActionHandlerDiagnosticCode::None,
            UiDiagnosticSeverity::Info,
            {},
            std::move(context));
    }

    [[nodiscard]] UiNamedActionHandlerResult DispatchNamedAction(
        const UiNamedAction& action) override
    {
        UiNamedActionContext context = MakeContext(action);
        if (!action.actionId.IsSet())
        {
            return AddDiagnostic(MakeResult(
                UiNamedActionHandlerDiagnosticCode::InvalidAction,
                UiDiagnosticSeverity::Error,
                "UI named action id is invalid.",
                std::move(context)));
        }

        const auto handlerIt = handlers_.find(action.actionId.value);
        if (handlerIt == handlers_.end())
        {
            return AddDiagnostic(MakeResult(
                UiNamedActionHandlerDiagnosticCode::MissingHandler,
                UiDiagnosticSeverity::Error,
                "No UI named action handler matched the action.",
                std::move(context)));
        }

        UiNamedActionHandlerResult result =
            handlerIt->second->Invoke(context);
        if (!result.context.actionId.IsSet())
        {
            result.context = std::move(context);
        }

        if (!result.Succeeded())
        {
            if (result.code == UiNamedActionHandlerDiagnosticCode::None)
            {
                result.code = UiNamedActionHandlerDiagnosticCode::HandlerFailed;
            }
            if (result.message.empty())
            {
                result.message = "UI named action handler failed.";
            }
            return AddDiagnostic(std::move(result));
        }

        return result;
    }

    [[nodiscard]] std::vector<UiNamedActionHandlerResult>
    DispatchNamedActions(const std::vector<UiNamedAction>& actions) override
    {
        std::vector<UiNamedActionHandlerResult> results;
        results.reserve(actions.size());
        for (const UiNamedAction& action : actions)
        {
            results.push_back(DispatchNamedAction(action));
        }
        return results;
    }

    [[nodiscard]] const std::vector<UiNamedActionHandlerDiagnostic>&
    Diagnostics() const noexcept override
    {
        return diagnostics_;
    }

    void ClearDiagnostics() noexcept override
    {
        diagnostics_.clear();
    }

private:
    [[nodiscard]] UiNamedActionHandlerResult AddDiagnostic(
        UiNamedActionHandlerResult diagnostic)
    {
        diagnostics_.push_back(diagnostic);
        return diagnostic;
    }

    std::unordered_map<std::string, std::shared_ptr<IUiNamedActionHandler>>
        handlers_;
    std::vector<UiNamedActionHandlerDiagnostic> diagnostics_;
};

} // namespace

std::unique_ptr<IUiNamedActionHandlerRegistry>
CreateUiNamedActionHandlerRegistry()
{
    return std::make_unique<UiNamedActionHandlerRegistry>();
}

} // namespace SagaEngine::UI
