/// @file IUiScreenManager.h
/// @brief Backend-neutral runtime UI screen management contract.

#pragma once

#include "SagaEngine/UI/IUiBackend.h"
#include "SagaEngine/UI/UiResourceIds.h"
#include "SagaEngine/UI/UiTypes.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::UI
{

/// Loading policy declared by a screen descriptor.
enum class UiScreenLoadPolicy : std::uint8_t
{
    Manual = 0,
    Preload,
};

/// Runtime state tracked by the screen manager.
enum class UiScreenState : std::uint8_t
{
    Unregistered = 0,
    Registered,
    LoadedHidden,
    Visible,
};

/// Stable screen manager diagnostic codes.
enum class UiScreenDiagnosticCode : std::uint8_t
{
    None = 0,
    InvalidScreenId,
    InvalidDocumentId,
    DuplicateRegistration,
    ScreenNotRegistered,
    DuplicateLoad,
    BackendLoadFailed,
    BackendOperationFailed,
    InvalidStateTransition,
};

/// Result of one screen manager operation.
struct UiScreenOperationResult
{
    UiScreenDiagnosticCode code = UiScreenDiagnosticCode::None;
    UiDiagnosticSeverity severity = UiDiagnosticSeverity::Info;
    std::string message;
    UiScreenId screenId;
    UiDocumentId documentId;
    UiDocumentHandle document = UiDocumentHandle::kInvalid;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return code == UiScreenDiagnosticCode::None;
    }
};

/// Runtime screen registration descriptor.
struct UiScreenDescriptor
{
    UiScreenId screenId;
    UiDocumentId documentId;
    UiScreenLoadPolicy loadPolicy = UiScreenLoadPolicy::Manual;
    bool showOnLoad = false;
};

/// Snapshot row for one registered screen.
struct UiScreenSnapshotEntry
{
    UiScreenId screenId;
    UiDocumentId documentId;
    UiScreenState state = UiScreenState::Unregistered;
    UiDocumentHandle document = UiDocumentHandle::kInvalid;
    bool documentHandleValid = false;
    bool visible = false;
    std::size_t diagnosticCount = 0;
};

/// Backend-neutral screen manager snapshot.
struct UiScreenSnapshot
{
    std::vector<UiScreenSnapshotEntry> screens;
    std::size_t diagnosticCount = 0;
};

/// Owns runtime UI screen registration and backend document handles.
class IUiScreenManager
{
public:
    virtual ~IUiScreenManager() = default;

    [[nodiscard]] virtual UiScreenOperationResult RegisterScreen(
        UiScreenDescriptor descriptor) = 0;

    [[nodiscard]] virtual UiScreenOperationResult LoadScreen(
        const UiScreenId& screenId) = 0;

    [[nodiscard]] virtual UiScreenOperationResult ShowScreen(
        const UiScreenId& screenId) = 0;

    [[nodiscard]] virtual UiScreenOperationResult HideScreen(
        const UiScreenId& screenId) = 0;

    [[nodiscard]] virtual UiScreenOperationResult UnloadScreen(
        const UiScreenId& screenId) = 0;

    virtual void UnloadAll() = 0;

    [[nodiscard]] virtual UiScreenSnapshot Snapshot() const = 0;

    [[nodiscard]] virtual const std::vector<UiScreenOperationResult>&
    Diagnostics() const noexcept = 0;

    virtual void ClearDiagnostics() noexcept = 0;
};

/// Create the default backend-neutral runtime UI screen manager.
[[nodiscard]] std::unique_ptr<IUiScreenManager> CreateUiScreenManager(
    IUiBackend& backend);

} // namespace SagaEngine::UI
