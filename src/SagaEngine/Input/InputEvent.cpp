#include "SagaEngine/Input/Events/InputEvent.h"
#include <chrono>

namespace Saga::Input {

InputEvent::InputEvent()
: type(InputEventType::KeyDown)
, device(0)
, timestamp(static_cast<Timestamp>(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count()))
, key(0)
, pressed(false)
, mouseButton(0)
, mouseX(0.0f)
, mouseY(0.0f)
, axisId(0)
, axisValue(0.0f)
, handled(false)
{
}

}
