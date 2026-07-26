// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "RuntimeHost.h"

#include <filesystem>

namespace SagaRuntimeApp
{
[[nodiscard]] RuntimeHostResult RunRuntimeFrameLoop(
    const std::filesystem::path& playEnvelope,
    bool headless,
    bool smokeInteractions,
    bool legacyGenericMode);
} // namespace SagaRuntimeApp
