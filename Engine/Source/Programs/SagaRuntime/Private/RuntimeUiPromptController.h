// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "RuntimeStateStore.h"

#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <optional>
#include <string>

namespace SagaRuntimeApp
{

/// Joins verified input labels and contextual script state without sample text
/// living in engine core.
class RuntimeUiPromptController
{
public:
    [[nodiscard]] static std::optional<std::string> Compose(
        const SagaRuntime::Play::RuntimeUiPromptBinding& prompt,
        const SagaRuntime::Play::RuntimeInputMapArtifact& input,
        const RuntimeStateStore& state,
        SagaEngine::Scripting::ScriptEntityHandle entity);
};

} // namespace SagaRuntimeApp
