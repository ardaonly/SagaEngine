// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "SagaEngine/UI/IUiBackend.h"

#include <memory>

namespace SagaEngine::UI::Backends
{

/// Create the production RmlUi adapter without exposing RmlUi types.
[[nodiscard]] std::unique_ptr<::SagaEngine::UI::IUiBackend> CreateRmlUiUiBackend();

} // namespace SagaEngine::UI::Backends
