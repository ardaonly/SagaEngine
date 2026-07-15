/// @file RmlUiUiBackend.h
/// @brief Backend factory for the RmlUi runtime UI adapter.

#pragma once

#include "SagaEngine/UI/IUiBackend.h"

#include <memory>

namespace SagaEngine::UI::Backends
{

/// Create the RmlUi-backed runtime UI backend without exposing RmlUi types.
[[nodiscard]] std::unique_ptr<::SagaEngine::UI::IUiBackend>
CreateRmlUiUiBackend();

} // namespace SagaEngine::UI::Backends
