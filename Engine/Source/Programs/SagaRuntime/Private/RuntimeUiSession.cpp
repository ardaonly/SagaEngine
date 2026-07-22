// SPDX-License-Identifier: MPL-2.0
#include "RuntimeUiSession.h"

#include "RuntimeUiPromptController.h"

#include <SagaEngine/UI/UiBackendFactory.h>

namespace SagaRuntimeApp
{
namespace UI = SagaEngine::UI;

bool RuntimeUiSession::Initialize(
    const SagaRuntime::Play::RuntimeUiSetArtifact& ui,
    const SagaRuntime::Play::RuntimeInputMapArtifact& input,
    RuntimeStateStore& state,
    const SagaEngine::Scripting::ScriptEntityHandle promptEntity,
    SagaEngine::Render::Backend::IRenderOverlaySubmission& submission,
    const std::uint32_t width, const std::uint32_t height)
{
    Shutdown();
    if (ui.documents.size() != 1 || ui.prompts.size() != 1 ||
        ui.fonts.empty() || !promptEntity.IsValid()) return false;
    backend_ = UI::Backends::CreateRmlUiUiBackend();
    UI::UiBackendConfig config;
    config.viewport = {width, height, 1.0F};
    config.assetRoot = ui.documents.front().path.parent_path();
    config.overlaySubmission = &submission;
    for (const auto& font : ui.fonts) config.fontPaths.push_back(font.path);
    if (!backend_ || !backend_->Initialize(config)) { Shutdown(); return false; }
    UI::UiDocumentRequest request = UI::UiDocumentRequest::FromDebugPath(
        ui.documents.front().path, true, UI::UiScreenId::FromString(ui.screenId));
    document_ = backend_->LoadDocument(request);
    if (!UI::IsValid(document_)) { Shutdown(); return false; }
    prompt_ = &ui.prompts.front();
    input_ = &input;
    state_ = &state;
    promptEntity_ = promptEntity;
    const auto text = RuntimeUiPromptController::Compose(
        *prompt_, *input_, *state_, promptEntity_);
    if (!text || !backend_->SetDocumentElementText(
            document_, prompt_->elementId, *text)) { Shutdown(); return false; }
    promptText_ = *text;
    initialized_ = true;
    return true;
}

bool RuntimeUiSession::Render(const std::uint32_t width, const std::uint32_t height,
                              const double deltaSeconds)
{
    if (!initialized_ || width == 0 || height == 0) return width == 0 || height == 0;
    backend_->SetViewport({width, height, 1.0F});
    const auto text = RuntimeUiPromptController::Compose(
        *prompt_, *input_, *state_, promptEntity_);
    if (!text || (*text != promptText_ && !backend_->SetDocumentElementText(
            document_, prompt_->elementId, *text))) return false;
    promptText_ = *text;
    backend_->Update(deltaSeconds);
    return backend_->Render();
}

void RuntimeUiSession::Shutdown() noexcept
{
    if (backend_)
    {
        if (UI::IsValid(document_)) static_cast<void>(backend_->UnloadDocument(document_));
        backend_->Shutdown();
    }
    backend_.reset();
    document_ = UI::UiDocumentHandle::kInvalid;
    prompt_ = nullptr;
    input_ = nullptr;
    state_ = nullptr;
    promptEntity_ = {};
    promptText_.clear();
    initialized_ = false;
}

} // namespace SagaRuntimeApp
