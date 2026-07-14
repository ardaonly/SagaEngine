/// @file UiScreenManager.cpp
/// @brief Backend-neutral runtime UI screen manager implementation.

#include "SagaEngine/UI/IUiScreenManager.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace SagaEngine::UI
{
namespace
{

[[nodiscard]] UiScreenOperationResult MakeSuccess(
    UiScreenId screenId,
    UiDocumentId documentId,
    UiDocumentHandle document = UiDocumentHandle::kInvalid)
{
    UiScreenOperationResult result;
    result.screenId = std::move(screenId);
    result.documentId = std::move(documentId);
    result.document = document;
    return result;
}

[[nodiscard]] UiScreenOperationResult MakeDiagnostic(
    UiScreenDiagnosticCode code,
    UiDiagnosticSeverity severity,
    std::string message,
    UiScreenId screenId = {},
    UiDocumentId documentId = {},
    UiDocumentHandle document = UiDocumentHandle::kInvalid)
{
    UiScreenOperationResult result;
    result.code = code;
    result.severity = severity;
    result.message = std::move(message);
    result.screenId = std::move(screenId);
    result.documentId = std::move(documentId);
    result.document = document;
    return result;
}

[[nodiscard]] bool IsValidScreenId(const UiScreenId& id)
{
    return id.IsSet() && ValidateUiResourceName(id.value).Succeeded();
}

class UiScreenManager final : public IUiScreenManager
{
public:
    explicit UiScreenManager(IUiBackend& backend)
        : backend_(backend)
    {
    }

    [[nodiscard]] UiScreenOperationResult RegisterScreen(
        UiScreenDescriptor descriptor) override
    {
        if (!IsValidScreenId(descriptor.screenId))
        {
            return AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::InvalidScreenId,
                UiDiagnosticSeverity::Error,
                "UI screen id is invalid.",
                descriptor.screenId,
                descriptor.documentId));
        }

        const UiResourceIdValidationResult documentResult =
            ValidateUiDocumentId(descriptor.documentId);
        if (!documentResult.Succeeded())
        {
            return AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::InvalidDocumentId,
                UiDiagnosticSeverity::Error,
                documentResult.diagnostic,
                descriptor.screenId,
                descriptor.documentId));
        }

        if (screens_.find(descriptor.screenId.value) != screens_.end())
        {
            return AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::DuplicateRegistration,
                UiDiagnosticSeverity::Warning,
                "UI screen is already registered.",
                descriptor.screenId,
                descriptor.documentId));
        }

        const UiScreenId screenId = descriptor.screenId;
        const UiDocumentId documentId = descriptor.documentId;
        screens_.emplace(
            screenId.value,
            ScreenEntry{std::move(descriptor), UiScreenState::Registered});

        if (screens_.at(screenId.value).descriptor.loadPolicy ==
            UiScreenLoadPolicy::Preload)
        {
            return LoadScreen(screenId);
        }

        return MakeSuccess(screenId, documentId);
    }

    [[nodiscard]] UiScreenOperationResult LoadScreen(
        const UiScreenId& screenId) override
    {
        ScreenEntry* screen = FindScreen(screenId);
        if (!screen)
        {
            return LastDiagnostic();
        }

        return LoadScreen(*screen, screen->descriptor.showOnLoad);
    }

    [[nodiscard]] UiScreenOperationResult ShowScreen(
        const UiScreenId& screenId) override
    {
        ScreenEntry* screen = FindScreen(screenId);
        if (!screen)
        {
            return LastDiagnostic();
        }

        if (screen->state == UiScreenState::Registered)
        {
            return LoadScreen(*screen, true);
        }

        if (screen->state == UiScreenState::Visible)
        {
            return MakeSuccess(
                screen->descriptor.screenId,
                screen->descriptor.documentId,
                screen->document);
        }

        if (screen->state != UiScreenState::LoadedHidden)
        {
            return AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::InvalidStateTransition,
                UiDiagnosticSeverity::Error,
                "UI screen cannot be shown from its current state.",
                screen->descriptor.screenId,
                screen->descriptor.documentId,
                screen->document));
        }

        if (!backend_.ShowDocument(screen->document))
        {
            return AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::BackendOperationFailed,
                UiDiagnosticSeverity::Error,
                "UI backend failed to show the screen document.",
                screen->descriptor.screenId,
                screen->descriptor.documentId,
                screen->document));
        }

        screen->state = UiScreenState::Visible;
        return MakeSuccess(
            screen->descriptor.screenId,
            screen->descriptor.documentId,
            screen->document);
    }

    [[nodiscard]] UiScreenOperationResult HideScreen(
        const UiScreenId& screenId) override
    {
        ScreenEntry* screen = FindScreen(screenId);
        if (!screen)
        {
            return LastDiagnostic();
        }

        if (screen->state == UiScreenState::Registered)
        {
            return AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::InvalidStateTransition,
                UiDiagnosticSeverity::Error,
                "UI screen cannot be hidden before it is loaded.",
                screen->descriptor.screenId,
                screen->descriptor.documentId));
        }

        if (screen->state == UiScreenState::LoadedHidden)
        {
            return MakeSuccess(
                screen->descriptor.screenId,
                screen->descriptor.documentId,
                screen->document);
        }

        if (!backend_.HideDocument(screen->document))
        {
            return AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::BackendOperationFailed,
                UiDiagnosticSeverity::Error,
                "UI backend failed to hide the screen document.",
                screen->descriptor.screenId,
                screen->descriptor.documentId,
                screen->document));
        }

        screen->state = UiScreenState::LoadedHidden;
        return MakeSuccess(
            screen->descriptor.screenId,
            screen->descriptor.documentId,
            screen->document);
    }

    [[nodiscard]] UiScreenOperationResult UnloadScreen(
        const UiScreenId& screenId) override
    {
        ScreenEntry* screen = FindScreen(screenId);
        if (!screen)
        {
            return LastDiagnostic();
        }

        if (screen->state == UiScreenState::Registered)
        {
            return MakeSuccess(
                screen->descriptor.screenId,
                screen->descriptor.documentId);
        }

        const UiDocumentHandle document = screen->document;
        if (!backend_.UnloadDocument(document))
        {
            return AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::BackendOperationFailed,
                UiDiagnosticSeverity::Error,
                "UI backend failed to unload the screen document.",
                screen->descriptor.screenId,
                screen->descriptor.documentId,
                document));
        }

        screen->document = UiDocumentHandle::kInvalid;
        screen->state = UiScreenState::Registered;
        return MakeSuccess(
            screen->descriptor.screenId,
            screen->descriptor.documentId,
            document);
    }

    void UnloadAll() override
    {
        for (auto& [id, screen] : screens_)
        {
            (void)id;
            if (IsValid(screen.document))
            {
                (void)backend_.UnloadDocument(screen.document);
            }
            screen.document = UiDocumentHandle::kInvalid;
            screen.state = UiScreenState::Registered;
        }
    }

    [[nodiscard]] UiScreenSnapshot Snapshot() const override
    {
        UiScreenSnapshot snapshot;
        snapshot.diagnosticCount = diagnostics_.size();
        snapshot.screens.reserve(screens_.size());

        for (const auto& [id, screen] : screens_)
        {
            (void)id;
            snapshot.screens.push_back(UiScreenSnapshotEntry{
                screen.descriptor.screenId,
                screen.descriptor.documentId,
                screen.state,
                screen.document,
                IsValid(screen.document),
                screen.state == UiScreenState::Visible,
                diagnostics_.size(),
            });
        }

        std::sort(
            snapshot.screens.begin(),
            snapshot.screens.end(),
            [](const UiScreenSnapshotEntry& lhs, const UiScreenSnapshotEntry& rhs)
            {
                return lhs.screenId.value < rhs.screenId.value;
            });

        return snapshot;
    }

    [[nodiscard]] const std::vector<UiScreenOperationResult>& Diagnostics()
        const noexcept override
    {
        return diagnostics_;
    }

    void ClearDiagnostics() noexcept override
    {
        diagnostics_.clear();
    }

private:
    struct ScreenEntry
    {
        UiScreenDescriptor descriptor;
        UiScreenState state = UiScreenState::Registered;
        UiDocumentHandle document = UiDocumentHandle::kInvalid;
    };

    [[nodiscard]] ScreenEntry* FindScreen(const UiScreenId& screenId)
    {
        const auto it = screens_.find(screenId.value);
        if (it == screens_.end())
        {
            (void)AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::ScreenNotRegistered,
                UiDiagnosticSeverity::Error,
                "UI screen is not registered.",
                screenId));
            return nullptr;
        }

        return &it->second;
    }

    [[nodiscard]] UiScreenOperationResult LoadScreen(
        ScreenEntry& screen,
        bool showImmediately)
    {
        if (IsValid(screen.document))
        {
            return AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::DuplicateLoad,
                UiDiagnosticSeverity::Warning,
                "UI screen document is already loaded.",
                screen.descriptor.screenId,
                screen.descriptor.documentId,
                screen.document));
        }

        const UiDocumentHandle document = backend_.LoadDocument(
            UiDocumentRequest::FromDocumentId(
                screen.descriptor.documentId,
                showImmediately,
                screen.descriptor.screenId));
        if (!IsValid(document))
        {
            return AddDiagnostic(MakeDiagnostic(
                UiScreenDiagnosticCode::BackendLoadFailed,
                UiDiagnosticSeverity::Error,
                "UI backend failed to load the screen document.",
                screen.descriptor.screenId,
                screen.descriptor.documentId));
        }

        screen.document = document;
        screen.state =
            showImmediately ? UiScreenState::Visible : UiScreenState::LoadedHidden;
        return MakeSuccess(
            screen.descriptor.screenId,
            screen.descriptor.documentId,
            document);
    }

    [[nodiscard]] UiScreenOperationResult AddDiagnostic(
        UiScreenOperationResult diagnostic)
    {
        diagnostics_.push_back(diagnostic);
        return diagnostic;
    }

    [[nodiscard]] UiScreenOperationResult LastDiagnostic() const
    {
        if (!diagnostics_.empty())
        {
            return diagnostics_.back();
        }

        return MakeDiagnostic(
            UiScreenDiagnosticCode::ScreenNotRegistered,
            UiDiagnosticSeverity::Error,
            "UI screen is not registered.");
    }

    IUiBackend& backend_;
    std::unordered_map<std::string, ScreenEntry> screens_;
    std::vector<UiScreenOperationResult> diagnostics_;
};

} // namespace

std::unique_ptr<IUiScreenManager> CreateUiScreenManager(IUiBackend& backend)
{
    return std::make_unique<UiScreenManager>(backend);
}

} // namespace SagaEngine::UI
