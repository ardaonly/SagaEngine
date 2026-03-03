#include "SagaEngine/Input/Mapping/InputAction.h"

namespace Saga::Input {

InputAction::InputAction(std::string n, ActionCallback cb)
: name(std::move(n))
, callback(std::move(cb))
{
}

}
