/// @file SampleComponent.cpp
/// @brief Implementation for Prism's minimal documented C++ example.

#include "PrismExample/SampleComponent.h"

namespace PrismExample {

int ComputeScore(const SampleComponent& component)
{
    return component.strength * 2;
}

} // namespace PrismExample
