// SPDX-License-Identifier: MPL-2.0
#include "RuntimeUiPromptController.h"

namespace SagaRuntimeApp
{

std::optional<std::string> RuntimeUiPromptController::Compose(
    const SagaRuntime::Play::RuntimeUiPromptBinding& prompt,
    const SagaRuntime::Play::RuntimeInputMapArtifact& input,
    const RuntimeStateStore& state,
    const SagaEngine::Scripting::ScriptEntityHandle entity)
{
    const SagaRuntime::Play::RuntimeInputAction* matched = nullptr;
    for (const auto& action : input.actions)
        if (action.actionId == prompt.actionId)
        {
            if (matched != nullptr) return std::nullopt;
            matched = &action;
        }
    if (matched == nullptr || matched->bindings.size() != 1 ||
        matched->bindings.front().displayLabel.empty()) return std::nullopt;
    const auto snapshot = state.Snapshot(entity, prompt.behaviorId, prompt.stateKey);
    if (!snapshot) return std::nullopt;
    return "[" + matched->bindings.front().displayLabel + "] " +
        (snapshot->value ? prompt.trueLabel : prompt.falseLabel);
}

} // namespace SagaRuntimeApp
