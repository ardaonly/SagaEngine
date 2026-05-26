/// @file IUiActionDispatcher.h
/// @brief Backend-neutral UI event to action binding contract.

#pragma once

#include "SagaEngine/UI/IUiEventQueue.h"
#include "SagaEngine/UI/IUiScreenManager.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace SagaEngine::UI
{

/// Stable runtime UI action id.
struct UiActionId
{
    std::string value;

    [[nodiscard]] static UiActionId FromString(std::string value)
    {
        return UiActionId{std::move(value)};
    }

    [[nodiscard]] bool IsSet() const noexcept
    {
        return !value.empty();
    }
};

/// Built-in action behavior supported by UI Action Binding V1.
enum class UiActionType : std::uint8_t
{
    OpenScreen = 0,
    CloseScreen,
    ToggleScreen,
    EmitNamedAction,
};

/// One event-to-action binding.
struct UiActionBinding
{
    UiScreenId screenId;
    UiElementId elementId;
    UiEventType eventType = UiEventType::Click;
    UiActionId actionId;
    UiActionType actionType = UiActionType::EmitNamedAction;
    UiScreenId targetScreenId;
};

/// Action map used for batch registration.
struct UiActionMap
{
    std::vector<UiActionBinding> bindings;
};

/// Stable action binding diagnostic/result codes.
enum class UiActionDiagnosticCode : std::uint8_t
{
    None = 0,
    InvalidBinding,
    DuplicateBinding,
    MissingBinding,
    InvalidTargetScreen,
    ScreenOperationFailed,
    NamedActionEmitted,
};

/// Result of binding lookup or action dispatch.
struct UiActionDispatchResult
{
    UiActionDiagnosticCode code = UiActionDiagnosticCode::None;
    UiDiagnosticSeverity severity = UiDiagnosticSeverity::Info;
    std::string message;
    UiEvent event;
    UiActionId actionId;
    UiActionType actionType = UiActionType::EmitNamedAction;
    UiScreenId targetScreenId;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return severity != UiDiagnosticSeverity::Error;
    }
};

/// Structured action binding diagnostic.
using UiActionDiagnostic = UiActionDispatchResult;

/// Named action emitted for later runtime/product/script consumption.
struct UiNamedAction
{
    UiActionId actionId;
    UiEvent sourceEvent;
};

/// Runtime UI action dispatcher.
class IUiActionDispatcher
{
public:
    virtual ~IUiActionDispatcher() = default;

    /// Register one event-to-action binding.
    [[nodiscard]] virtual UiActionDispatchResult RegisterBinding(
        UiActionBinding binding) = 0;

    /// Register all bindings in a map, preserving individual diagnostics.
    [[nodiscard]] virtual std::vector<UiActionDispatchResult> RegisterActionMap(
        const UiActionMap& actionMap) = 0;

    /// Dispatch one UI event through the registered action bindings.
    [[nodiscard]] virtual UiActionDispatchResult DispatchEvent(
        const UiEvent& event) = 0;

    /// Dispatch UI events in order.
    [[nodiscard]] virtual std::vector<UiActionDispatchResult> DispatchEvents(
        const std::vector<UiEvent>& events) = 0;

    /// Return and clear named actions emitted by EmitNamedAction.
    [[nodiscard]] virtual std::vector<UiNamedAction> DrainNamedActions() = 0;

    /// Return accumulated action diagnostics.
    [[nodiscard]] virtual const std::vector<UiActionDiagnostic>& Diagnostics()
        const noexcept = 0;

    /// Clear accumulated action diagnostics.
    virtual void ClearDiagnostics() noexcept = 0;
};

/// Create the default backend-neutral UI action dispatcher.
[[nodiscard]] std::unique_ptr<IUiActionDispatcher> CreateUiActionDispatcher(
    IUiScreenManager& screenManager);

} // namespace SagaEngine::UI
