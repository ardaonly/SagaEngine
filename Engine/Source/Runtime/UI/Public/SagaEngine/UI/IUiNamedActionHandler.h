/// @file IUiNamedActionHandler.h
/// @brief Backend-neutral UI named action handler contract.

#pragma once

#include "SagaEngine/UI/IUiActionDispatcher.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace SagaEngine::UI
{

/// Context passed from a UI named action to runtime/product/script handlers.
struct UiNamedActionContext
{
    UiActionId actionId;
    UiScreenId screenId;
    UiDocumentId documentId;
    UiDocumentHandle document = UiDocumentHandle::kInvalid;
    UiElementId elementId;
    UiEventType eventType = UiEventType::Click;
    std::string text;
    UiEvent sourceEvent;
};

/// Stable named action handler diagnostic/result codes.
enum class UiNamedActionHandlerDiagnosticCode : std::uint8_t
{
    None = 0,
    InvalidAction,
    DuplicateHandler,
    MissingHandler,
    HandlerFailed,
};

/// Result returned by named action handler registration or invocation.
struct UiNamedActionHandlerResult
{
    UiNamedActionHandlerDiagnosticCode code =
        UiNamedActionHandlerDiagnosticCode::None;
    UiDiagnosticSeverity severity = UiDiagnosticSeverity::Info;
    std::string message;
    UiNamedActionContext context;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return severity != UiDiagnosticSeverity::Error;
    }
};

/// Structured named action handler diagnostic.
using UiNamedActionHandlerDiagnostic = UiNamedActionHandlerResult;

/// Runtime/product/script-facing named action handler.
class IUiNamedActionHandler
{
public:
    virtual ~IUiNamedActionHandler() = default;

    /// Invoke this handler for one emitted named action context.
    [[nodiscard]] virtual UiNamedActionHandlerResult Invoke(
        const UiNamedActionContext& context) = 0;
};

/// Registry and dispatcher for emitted UI named actions.
class IUiNamedActionHandlerRegistry
{
public:
    virtual ~IUiNamedActionHandlerRegistry() = default;

    /// Register a handler for one action id.
    [[nodiscard]] virtual UiNamedActionHandlerResult RegisterHandler(
        UiActionId actionId,
        std::shared_ptr<IUiNamedActionHandler> handler) = 0;

    /// Dispatch one emitted named action to its registered handler.
    [[nodiscard]] virtual UiNamedActionHandlerResult DispatchNamedAction(
        const UiNamedAction& action) = 0;

    /// Dispatch emitted named actions in order.
    [[nodiscard]] virtual std::vector<UiNamedActionHandlerResult>
    DispatchNamedActions(const std::vector<UiNamedAction>& actions) = 0;

    /// Return accumulated named action handler diagnostics.
    [[nodiscard]] virtual const std::vector<UiNamedActionHandlerDiagnostic>&
    Diagnostics() const noexcept = 0;

    /// Clear accumulated named action handler diagnostics.
    virtual void ClearDiagnostics() noexcept = 0;
};

/// Create the default backend-neutral named action handler registry.
[[nodiscard]] std::unique_ptr<IUiNamedActionHandlerRegistry>
CreateUiNamedActionHandlerRegistry();

} // namespace SagaEngine::UI
