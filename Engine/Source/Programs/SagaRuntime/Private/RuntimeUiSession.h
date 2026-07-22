// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "RuntimeStateStore.h"

#include <SagaEngine/Render/Backend/RenderBackendFactory.h>
#include <SagaEngine/UI/IUiBackend.h>
#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <memory>
#include <string>
#include <vector>

namespace SagaRuntimeApp
{

/// Owns the graphical runtime UI context below the verified UI artifact.
class RuntimeUiSession
{
public:
    [[nodiscard]] bool Initialize(
        const SagaRuntime::Play::RuntimeUiSetArtifact& ui,
        const SagaRuntime::Play::RuntimeInputMapArtifact& input,
        RuntimeStateStore& state,
        SagaEngine::Scripting::ScriptEntityHandle promptEntity,
        SagaEngine::Render::Backend::IRenderOverlaySubmission& submission,
        std::uint32_t width, std::uint32_t height);
    [[nodiscard]] bool Render(std::uint32_t width, std::uint32_t height,
                              double deltaSeconds);
    void Shutdown() noexcept;
    [[nodiscard]] const std::string& PromptText() const noexcept { return promptText_; }

private:
    std::unique_ptr<SagaEngine::UI::IUiBackend> backend_;
    SagaEngine::UI::UiDocumentHandle document_ =
        SagaEngine::UI::UiDocumentHandle::kInvalid;
    const SagaRuntime::Play::RuntimeUiPromptBinding* prompt_ = nullptr;
    const SagaRuntime::Play::RuntimeInputMapArtifact* input_ = nullptr;
    RuntimeStateStore* state_ = nullptr;
    SagaEngine::Scripting::ScriptEntityHandle promptEntity_;
    std::string promptText_;
    bool initialized_ = false;
};

} // namespace SagaRuntimeApp
