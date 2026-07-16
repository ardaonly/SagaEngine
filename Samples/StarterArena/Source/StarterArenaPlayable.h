// SPDX-License-Identifier: Apache-2.0

/// @file StarterArenaPlayable.h
/// @brief Visible StarterArena runtime command contract.

#pragma once

#include "RuntimeCommandLine.h"

#include <cstdint>

namespace SagaRuntimeApp
{

/// Keyboard capture may finish early through Escape after presenting a frame.
/// Evidence validation still independently requires observed input and movement.
[[nodiscard]] constexpr bool StarterArenaFrameExpectationSatisfied(
    bool keyboardInput,
    std::uint32_t requestedFrames,
    std::uint32_t presentedFrames,
    std::uint32_t quitCount) noexcept
{
    return presentedFrames > 0u &&
        (requestedFrames == 0u || presentedFrames == requestedFrames ||
         (keyboardInput && quitCount > 0u));
}

[[nodiscard]] int RunStarterArenaPlayable(const RuntimeCommandLine& commandLine);

} // namespace SagaRuntimeApp
