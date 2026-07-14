/// @file Math.h
/// @brief Umbrella include for all SagaEngine math types and helpers.
///
/// Include this header from any engine subsystem that needs Vec3, Quat,
/// Transform, scalar helpers, or engine-wide math constants.
///
/// Do NOT include glm headers directly outside the Math module — the engine
/// relies on this header as the single GLM-free public surface.
///
/// Note: `DeterministicMath.h` and `MathSimd.h` are intentionally NOT pulled
/// in here.  They are specialised paths used only by simulation and batch
/// code; pulling them into every translation unit would bloat compile times
/// and hide their opt-in semantics.

#pragma once

#include "SagaEngine/Math/MathConstants.h"
#include "SagaEngine/Math/MathScalar.h"
#include "SagaEngine/Math/Vec3.h"
#include "SagaEngine/Math/Quat.h"
#include "SagaEngine/Math/Transform.h"
