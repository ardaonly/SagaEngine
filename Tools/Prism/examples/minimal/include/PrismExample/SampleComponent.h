/// @file SampleComponent.h
/// @brief Small documented type used by Prism's minimal example.

#pragma once

namespace PrismExample {

/// Immutable component-like record with a name and numeric strength.
struct SampleComponent
{
    const char* name = nullptr;  ///< Stable display name for graph output.
    int         strength = 0;    ///< Example scalar value.
};

/// Compute a simple score from a sample component.
int ComputeScore(const SampleComponent& component);

} // namespace PrismExample
