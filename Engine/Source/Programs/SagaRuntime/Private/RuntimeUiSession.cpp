// SPDX-License-Identifier: MPL-2.0
#include "RuntimeUiSession.h"

#include "RuntimeUiPromptController.h"

#include <SagaEngine/UI/UiBackendFactory.h>

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace SagaRuntimeApp
{
namespace UI = SagaEngine::UI;
namespace
{
std::string ValueText(const RuntimeStateValue& value)
{
    return std::visit([](const auto& item) -> std::string
    {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, bool>) return item ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::string>) return item;
        else if constexpr (std::is_same_v<T, double>)
        {
            auto text = std::to_string(item);
            while (text.size() > 1 && text.back() == '0') text.pop_back();
            if (!text.empty() && text.back() == '.') text.pop_back();
            return text;
        }
        else return std::to_string(item);
    }, value);
}

std::string Format(std::string pattern, std::string_view token,
                   std::string_view value)
{
    const auto position = pattern.find(token);
    if (position != std::string::npos)
        pattern.replace(position, token.size(), value);
    return pattern;
}
}

bool RuntimeUiSession::Initialize(
    const SagaRuntime::Play::RuntimeUiSetArtifact& ui,
    const SagaRuntime::Play::RuntimeInputMapArtifact& input,
    RuntimeStateStore& state,
    const SagaEngine::Scripting::ScriptEntityHandle promptEntity,
    SagaEngine::Render::Backend::IRenderOverlaySubmission& submission,
    const std::uint32_t width, const std::uint32_t height)
{
    Shutdown();
    if (ui.documents.empty() || ui.stylesheets.empty() ||
        ui.fonts.empty() || !promptEntity.IsValid() ||
        (ui.schemaVersion == 1 && (ui.documents.size() != 1 ||
                                   ui.prompts.size() != 1)))
        return false;
    backend_ = UI::Backends::CreateRmlUiUiBackend();
    UI::UiBackendConfig config;
    config.viewport = {width, height, 1.0F};
    config.assetRoot = ui.documents.front().path.parent_path();
    config.overlaySubmission = &submission;
    for (const auto& document : ui.documents)
        config.verifiedResourcePaths.push_back(document.path);
    for (const auto& stylesheet : ui.stylesheets)
        config.verifiedResourcePaths.push_back(stylesheet.path);
    for (const auto& font : ui.fonts)
    {
        config.fontPaths.push_back(font.path);
        config.verifiedResourcePaths.push_back(font.path);
    }
    std::sort(config.verifiedResourcePaths.begin(),
              config.verifiedResourcePaths.end());
    if (std::adjacent_find(config.verifiedResourcePaths.begin(),
                           config.verifiedResourcePaths.end()) !=
        config.verifiedResourcePaths.end())
        return false;
    if (!backend_ || !backend_->Initialize(config)) { Shutdown(); return false; }
    for (const auto& document : ui.documents)
    {
        UI::UiDocumentRequest request = UI::UiDocumentRequest::FromDebugPath(
            document.path, true, UI::UiScreenId::FromString(
                ui.screenId + "." + document.documentId));
        const auto handle = backend_->LoadDocument(request);
        if (!UI::IsValid(handle) ||
            !documents_.emplace(document.documentId, handle).second)
        { Shutdown(); return false; }
    }
    prompt_ = ui.prompts.empty() ? nullptr : &ui.prompts.front();
    ui_ = &ui;
    input_ = &input;
    state_ = &state;
    promptEntity_ = promptEntity;
    if (prompt_)
    {
        const auto text = RuntimeUiPromptController::Compose(
            *prompt_, *input_, *state_, promptEntity_);
        if (!text || !backend_->SetDocumentElementText(
                documents_.begin()->second, prompt_->elementId, *text))
        { Shutdown(); return false; }
        promptText_ = *text;
    }
    for (const auto& binding : ui.inputLabelBindings)
    {
        const auto action = std::find_if(input.actions.begin(), input.actions.end(),
            [&](const auto& item) { return item.actionId == binding.actionId; });
        const auto document = documents_.find(binding.documentId);
        if (action == input.actions.end() || action->bindings.empty() ||
            document == documents_.end() ||
            !backend_->SetDocumentElementText(document->second, binding.elementId,
                Format(binding.format, "{label}",
                       action->bindings.front().displayLabel)))
        { Shutdown(); return false; }
    }
    initialized_ = true;
    return true;
}

bool RuntimeUiSession::Render(const std::uint32_t width, const std::uint32_t height,
                              const double deltaSeconds)
{
    return PrepareLayout(width, height, 1.0F, deltaSeconds) && SubmitOverlay();
}

bool RuntimeUiSession::PrepareLayout(
    const std::uint32_t width, const std::uint32_t height,
    const float pixelRatio, const double deltaSeconds)
{
    layoutPrepared_ = false;
    if (!initialized_ || width == 0 || height == 0 ||
        !std::isfinite(pixelRatio) || pixelRatio <= 0.0F)
        return width == 0 || height == 0;
    pixelRatio_ = pixelRatio;
    backend_->SetViewport({width, height, pixelRatio_});
    if (prompt_)
    {
        const auto text = RuntimeUiPromptController::Compose(
            *prompt_, *input_, *state_, promptEntity_);
        if (!text || (*text != promptText_ && !backend_->SetDocumentElementText(
                documents_.begin()->second, prompt_->elementId, *text))) return false;
        promptText_ = *text;
    }
    if (ui_ && ui_->schemaVersion == 2)
    {
        visibleDocuments_.clear();
        for (const auto& binding : ui_->textBindings)
        {
            const auto document = documents_.find(binding.documentId);
            const auto snapshot = state_->SnapshotValue(
                promptEntity_, binding.behaviorId, binding.stateKey);
            if (document == documents_.end() || !snapshot ||
                !backend_->SetDocumentElementText(document->second,
                    binding.elementId, Format(binding.format, "{value}",
                                              ValueText(snapshot->value))))
                return false;
        }
        for (const auto& resource : ui_->documents)
        {
            const auto document = documents_.find(resource.documentId);
            const auto snapshot = state_->SnapshotValue(
                promptEntity_, resource.visibilityBehaviorId,
                resource.visibilityStateKey);
            if (document == documents_.end() || !snapshot ||
                !std::holds_alternative<std::string>(snapshot->value))
                return false;
            const auto& value = std::get<std::string>(snapshot->value);
            const bool visible = std::find(resource.visibilityEqualsAny.begin(),
                resource.visibilityEqualsAny.end(), value) !=
                resource.visibilityEqualsAny.end();
            if (!(visible ? backend_->ShowDocument(document->second)
                          : backend_->HideDocument(document->second)))
                return false;
            if (visible) visibleDocuments_.push_back(resource.documentId);
        }
    }
    backend_->Update(deltaSeconds);
    layoutPrepared_ = ui_ && ui_->schemaVersion == 2
        ? true : PromptBounds().has_value();
    return layoutPrepared_;
}

bool RuntimeUiSession::SubmitOverlay()
{
    if (!layoutPrepared_) return false;
    layoutPrepared_ = false;
    return backend_->Render();
}

std::optional<UI::UiElementBounds> RuntimeUiSession::PromptBounds() const
{
    if (!initialized_ || !backend_) return std::nullopt;
    if (prompt_)
        return backend_->GetDocumentElementBounds(
            documents_.begin()->second, prompt_->elementId);
    if (ui_ && !ui_->textBindings.empty())
    {
        const auto document = documents_.find(ui_->textBindings.front().documentId);
        if (document != documents_.end())
            return backend_->GetDocumentElementBounds(
                document->second, ui_->textBindings.front().elementId);
    }
    return std::nullopt;
}

void RuntimeUiSession::Shutdown() noexcept
{
    if (backend_)
    {
        for (const auto& [_, document] : documents_)
            if (UI::IsValid(document))
                static_cast<void>(backend_->UnloadDocument(document));
        backend_->Shutdown();
    }
    backend_.reset();
    documents_.clear();
    ui_ = nullptr;
    prompt_ = nullptr;
    input_ = nullptr;
    state_ = nullptr;
    promptEntity_ = {};
    promptText_.clear();
    visibleDocuments_.clear();
    pixelRatio_ = 1.0F;
    layoutPrepared_ = false;
    initialized_ = false;
}

} // namespace SagaRuntimeApp
