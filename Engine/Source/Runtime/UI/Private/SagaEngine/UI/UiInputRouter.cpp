/// @file UiInputRouter.cpp
/// @brief Default backend-neutral UI input router implementation.

#include "SagaEngine/UI/IUiInputRouter.h"

#include <utility>

namespace SagaEngine::UI
{
namespace
{

[[nodiscard]] UiInputDevice DeviceForEvent(UiInputEventType type) noexcept
{
    switch (type)
    {
    case UiInputEventType::MouseMove:
    case UiInputEventType::MouseButton:
    case UiInputEventType::MouseWheel:
        return UiInputDevice::Mouse;
    case UiInputEventType::Key:
        return UiInputDevice::Keyboard;
    case UiInputEventType::Text:
        return UiInputDevice::Text;
    default:
        return UiInputDevice::Unknown;
    }
}

class UiInputRouter final : public IUiInputRouter
{
public:
    [[nodiscard]] UiInputRoutingResult RouteInput(
        const UiInputEvent& event) const override
    {
        UiInputRoutingResult result;
        result.device = DeviceForEvent(event.type);
        result.captureState = captureState_;

        switch (result.device)
        {
        case UiInputDevice::Mouse:
            if (captureState_.mouseCaptured)
            {
                result.decision =
                    UiInputRoutingDecision::BlockedByMouseCapture;
                result.uiReceivesInput = true;
                result.gameplayBlocked = true;
            }
            break;
        case UiInputDevice::Keyboard:
            if (captureState_.textInputActive)
            {
                result.decision = UiInputRoutingDecision::BlockedByTextInput;
                result.uiReceivesInput = true;
                result.gameplayBlocked = true;
            }
            else if (captureState_.keyboardCaptured)
            {
                result.decision =
                    UiInputRoutingDecision::BlockedByKeyboardCapture;
                result.uiReceivesInput = true;
                result.gameplayBlocked = true;
            }
            break;
        case UiInputDevice::Text:
            if (captureState_.textInputActive)
            {
                result.decision = UiInputRoutingDecision::BlockedByTextInput;
                result.uiReceivesInput = true;
                result.gameplayBlocked = true;
            }
            break;
        default:
            break;
        }

        return result;
    }

    [[nodiscard]] const UiInputCaptureState& CaptureState()
        const noexcept override
    {
        return captureState_;
    }

    void SetCaptureState(UiInputCaptureState state) override
    {
        captureState_ = std::move(state);
    }

    void ResetFrameCapture() noexcept override
    {
        captureState_ = {};
    }

private:
    UiInputCaptureState captureState_;
};

} // namespace

std::unique_ptr<IUiInputRouter> CreateUiInputRouter()
{
    return std::make_unique<UiInputRouter>();
}

} // namespace SagaEngine::UI
