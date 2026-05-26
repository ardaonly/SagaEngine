/// @file IUiInputRouter.h
/// @brief Backend-neutral UI input capture and routing contract.

#pragma once

#include "SagaEngine/UI/UiTypes.h"

#include <cstdint>
#include <memory>

namespace SagaEngine::UI
{

/// Coarse input device classification used by UI routing policy.
enum class UiInputDevice : std::uint8_t
{
    Unknown = 0,
    Mouse,
    Keyboard,
    Text,
};

/// Reason a UI routing decision was made.
enum class UiInputRoutingDecision : std::uint8_t
{
    GameplayAllowed = 0,
    BlockedByMouseCapture,
    BlockedByKeyboardCapture,
    BlockedByTextInput,
};

/// Backend-neutral focused UI surface metadata.
struct UiFocusedSurface
{
    UiScreenId screenId;
    UiDocumentHandle document = UiDocumentHandle::kInvalid;

    [[nodiscard]] bool IsSet() const noexcept
    {
        return screenId.IsSet() || IsValid(document);
    }
};

/// Current UI input capture state supplied by runtime/backend policy.
struct UiInputCaptureState
{
    bool mouseCaptured = false;
    bool keyboardCaptured = false;
    bool textInputActive = false;
    UiFocusedSurface focusedSurface;

    [[nodiscard]] bool HasAnyCapture() const noexcept
    {
        return mouseCaptured || keyboardCaptured || textInputActive;
    }
};

/// Result of routing one UI input event.
struct UiInputRoutingResult
{
    UiInputDevice device = UiInputDevice::Unknown;
    UiInputRoutingDecision decision =
        UiInputRoutingDecision::GameplayAllowed;
    bool uiReceivesInput = false;
    bool gameplayBlocked = false;
    UiInputCaptureState captureState;
};

/// Deterministic UI input router used before gameplay consumes input.
class IUiInputRouter
{
public:
    virtual ~IUiInputRouter() = default;

    /// Route one backend-neutral UI input event through capture policy.
    [[nodiscard]] virtual UiInputRoutingResult RouteInput(
        const UiInputEvent& event) const = 0;

    /// Return the current capture state.
    [[nodiscard]] virtual const UiInputCaptureState& CaptureState()
        const noexcept = 0;

    /// Replace the current capture state with backend/runtime supplied data.
    virtual void SetCaptureState(UiInputCaptureState state) = 0;

    /// Clear capture and focused surface state deterministically.
    virtual void ResetFrameCapture() noexcept = 0;
};

/// Create the default backend-neutral UI input router.
[[nodiscard]] std::unique_ptr<IUiInputRouter> CreateUiInputRouter();

} // namespace SagaEngine::UI
