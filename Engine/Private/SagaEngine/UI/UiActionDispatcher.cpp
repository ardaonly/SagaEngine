/// @file UiActionDispatcher.cpp
/// @brief Default backend-neutral UI action binding implementation.

#include "SagaEngine/UI/IUiActionDispatcher.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

namespace SagaEngine::UI
{
namespace
{

[[nodiscard]] std::string BindingKey(
    const UiScreenId& screenId,
    const UiElementId& elementId,
    UiEventType eventType)
{
    return screenId.value + "\n" + elementId.value + "\n" +
           std::to_string(static_cast<int>(eventType));
}

[[nodiscard]] bool IsValidBinding(const UiActionBinding& binding)
{
    if (!binding.screenId.IsSet() ||
        !binding.elementId.IsSet() ||
        !binding.actionId.IsSet())
    {
        return false;
    }

    if (binding.actionType != UiActionType::EmitNamedAction &&
        !binding.targetScreenId.IsSet())
    {
        return false;
    }

    return true;
}

[[nodiscard]] UiActionDispatchResult MakeResult(
    UiActionDiagnosticCode code,
    UiDiagnosticSeverity severity,
    std::string message,
    UiEvent event = {},
    UiActionId actionId = {},
    UiActionType actionType = UiActionType::EmitNamedAction,
    UiScreenId targetScreenId = {})
{
    UiActionDispatchResult result;
    result.code = code;
    result.severity = severity;
    result.message = std::move(message);
    result.event = std::move(event);
    result.actionId = std::move(actionId);
    result.actionType = actionType;
    result.targetScreenId = std::move(targetScreenId);
    return result;
}

[[nodiscard]] const UiScreenSnapshotEntry* FindScreen(
    const UiScreenSnapshot& snapshot,
    const UiScreenId& screenId)
{
    const auto it = std::find_if(
        snapshot.screens.begin(),
        snapshot.screens.end(),
        [&screenId](const UiScreenSnapshotEntry& entry)
        {
            return entry.screenId == screenId;
        });
    return it == snapshot.screens.end() ? nullptr : &*it;
}

class UiActionDispatcher final : public IUiActionDispatcher
{
public:
    explicit UiActionDispatcher(IUiScreenManager& screenManager)
        : screenManager_(screenManager)
    {
    }

    [[nodiscard]] UiActionDispatchResult RegisterBinding(
        UiActionBinding binding) override
    {
        if (!IsValidBinding(binding))
        {
            return AddDiagnostic(MakeResult(
                UiActionDiagnosticCode::InvalidBinding,
                UiDiagnosticSeverity::Error,
                "UI action binding is invalid.",
                {},
                binding.actionId,
                binding.actionType,
                binding.targetScreenId));
        }

        const std::string key =
            BindingKey(binding.screenId, binding.elementId, binding.eventType);
        if (bindings_.find(key) != bindings_.end())
        {
            return AddDiagnostic(MakeResult(
                UiActionDiagnosticCode::DuplicateBinding,
                UiDiagnosticSeverity::Warning,
                "UI action binding already exists.",
                {},
                binding.actionId,
                binding.actionType,
                binding.targetScreenId));
        }

        UiActionDispatchResult result = MakeResult(
            UiActionDiagnosticCode::None,
            UiDiagnosticSeverity::Info,
            {},
            {},
            binding.actionId,
            binding.actionType,
            binding.targetScreenId);
        bindings_.emplace(std::move(key), std::move(binding));
        return result;
    }

    [[nodiscard]] std::vector<UiActionDispatchResult> RegisterActionMap(
        const UiActionMap& actionMap) override
    {
        std::vector<UiActionDispatchResult> results;
        results.reserve(actionMap.bindings.size());
        for (const UiActionBinding& binding : actionMap.bindings)
        {
            results.push_back(RegisterBinding(binding));
        }
        return results;
    }

    [[nodiscard]] UiActionDispatchResult DispatchEvent(
        const UiEvent& event) override
    {
        const std::string key =
            BindingKey(event.screenId, event.elementId, event.type);
        const auto bindingIt = bindings_.find(key);
        if (bindingIt == bindings_.end())
        {
            return MakeResult(
                UiActionDiagnosticCode::MissingBinding,
                UiDiagnosticSeverity::Info,
                "No UI action binding matched the event.",
                event);
        }

        const UiActionBinding& binding = bindingIt->second;
        switch (binding.actionType)
        {
        case UiActionType::OpenScreen:
            return DispatchScreenAction(
                event,
                binding,
                &IUiScreenManager::ShowScreen);
        case UiActionType::CloseScreen:
            return DispatchScreenAction(
                event,
                binding,
                &IUiScreenManager::HideScreen);
        case UiActionType::ToggleScreen:
            return DispatchToggleScreen(event, binding);
        case UiActionType::EmitNamedAction:
            namedActions_.push_back(UiNamedAction{binding.actionId, event});
            return MakeResult(
                UiActionDiagnosticCode::NamedActionEmitted,
                UiDiagnosticSeverity::Info,
                "UI named action emitted.",
                event,
                binding.actionId,
                binding.actionType,
                binding.targetScreenId);
        default:
            return AddDiagnostic(MakeResult(
                UiActionDiagnosticCode::InvalidBinding,
                UiDiagnosticSeverity::Error,
                "UI action type is invalid.",
                event,
                binding.actionId,
                binding.actionType,
                binding.targetScreenId));
        }
    }

    [[nodiscard]] std::vector<UiActionDispatchResult> DispatchEvents(
        const std::vector<UiEvent>& events) override
    {
        std::vector<UiActionDispatchResult> results;
        results.reserve(events.size());
        for (const UiEvent& event : events)
        {
            results.push_back(DispatchEvent(event));
        }
        return results;
    }

    [[nodiscard]] std::vector<UiNamedAction> DrainNamedActions() override
    {
        std::vector<UiNamedAction> drained;
        drained.swap(namedActions_);
        return drained;
    }

    [[nodiscard]] const std::vector<UiActionDiagnostic>& Diagnostics()
        const noexcept override
    {
        return diagnostics_;
    }

    void ClearDiagnostics() noexcept override
    {
        diagnostics_.clear();
    }

private:
    using ScreenOperation = UiScreenOperationResult (
        IUiScreenManager::*)(const UiScreenId&);

    [[nodiscard]] UiActionDispatchResult DispatchScreenAction(
        const UiEvent& event,
        const UiActionBinding& binding,
        ScreenOperation operation)
    {
        const UiScreenSnapshot snapshot = screenManager_.Snapshot();
        if (!FindScreen(snapshot, binding.targetScreenId))
        {
            return AddDiagnostic(MakeResult(
                UiActionDiagnosticCode::InvalidTargetScreen,
                UiDiagnosticSeverity::Error,
                "UI action target screen is not registered.",
                event,
                binding.actionId,
                binding.actionType,
                binding.targetScreenId));
        }

        const UiScreenOperationResult screenResult =
            (screenManager_.*operation)(binding.targetScreenId);
        if (!screenResult.Succeeded())
        {
            return AddDiagnostic(MakeResult(
                UiActionDiagnosticCode::ScreenOperationFailed,
                screenResult.severity,
                screenResult.message,
                event,
                binding.actionId,
                binding.actionType,
                binding.targetScreenId));
        }

        return MakeResult(
            UiActionDiagnosticCode::None,
            UiDiagnosticSeverity::Info,
            {},
            event,
            binding.actionId,
            binding.actionType,
            binding.targetScreenId);
    }

    [[nodiscard]] UiActionDispatchResult DispatchToggleScreen(
        const UiEvent& event,
        const UiActionBinding& binding)
    {
        const UiScreenSnapshot snapshot = screenManager_.Snapshot();
        const UiScreenSnapshotEntry* screen =
            FindScreen(snapshot, binding.targetScreenId);
        if (!screen)
        {
            return AddDiagnostic(MakeResult(
                UiActionDiagnosticCode::InvalidTargetScreen,
                UiDiagnosticSeverity::Error,
                "UI action target screen is not registered.",
                event,
                binding.actionId,
                binding.actionType,
                binding.targetScreenId));
        }

        const UiScreenOperationResult screenResult =
            screen->visible
                ? screenManager_.HideScreen(binding.targetScreenId)
                : screenManager_.ShowScreen(binding.targetScreenId);
        if (!screenResult.Succeeded())
        {
            return AddDiagnostic(MakeResult(
                UiActionDiagnosticCode::ScreenOperationFailed,
                screenResult.severity,
                screenResult.message,
                event,
                binding.actionId,
                binding.actionType,
                binding.targetScreenId));
        }

        return MakeResult(
            UiActionDiagnosticCode::None,
            UiDiagnosticSeverity::Info,
            {},
            event,
            binding.actionId,
            binding.actionType,
            binding.targetScreenId);
    }

    [[nodiscard]] UiActionDispatchResult AddDiagnostic(
        UiActionDispatchResult diagnostic)
    {
        diagnostics_.push_back(diagnostic);
        return diagnostic;
    }

    IUiScreenManager& screenManager_;
    std::unordered_map<std::string, UiActionBinding> bindings_;
    std::vector<UiActionDiagnostic> diagnostics_;
    std::vector<UiNamedAction> namedActions_;
};

} // namespace

std::unique_ptr<IUiActionDispatcher> CreateUiActionDispatcher(
    IUiScreenManager& screenManager)
{
    return std::make_unique<UiActionDispatcher>(screenManager);
}

} // namespace SagaEngine::UI
