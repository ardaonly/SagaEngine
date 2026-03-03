#include "SagaEngine/Input/Mapping/InputMapping.h"

namespace Saga::Input {

Binding::Binding(std::string a, InputEventType s, int c, float sc)
: actionName(std::move(a))
, sourceType(s)
, code(c)
, scale(sc)
{
}

MappingContext::MappingContext(std::string n, int p)
: name(std::move(n))
, priority(p)
{
}

}
