/// @file RmlUiUiBackend.h
/// @brief Backend factory for the RmlUi runtime UI adapter.

#pragma once

#include "SagaEngine/UI/IUiBackend.h"

#include <memory>

namespace SagaEngine::Backends::UI
{

/// Create the RmlUi-backed runtime UI backend without exposing RmlUi types.
[[nodiscard]] std::unique_ptr<::SagaEngine::UI::IUiBackend>
CreateRmlUiUiBackend();

} // namespace SagaEngine::Backends::UI
