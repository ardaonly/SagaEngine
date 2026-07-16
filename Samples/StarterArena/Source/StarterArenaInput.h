// SPDX-License-Identifier: Apache-2.0

/// @file StarterArenaInput.h
/// @brief App-local input providers for the visible StarterArena slice.

#pragma once

#include "StarterArenaSmoke.h"

#include <SagaEngine/Input/Backends/IPlatformInputBackend.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace SagaRuntimeApp
{

enum class StarterArenaInputSource
{
    Invalid,
    Scene,
    Synthetic,
    Keyboard,
};

struct StarterArenaInputActions
{
    bool moveLeft = false;
    bool moveRight = false;
    bool moveUp = false;
    bool moveDown = false;
    bool quit = false;

    [[nodiscard]] bool Any() const noexcept;
};

struct StarterArenaInputFrame
{
    StarterArenaInputSource source = StarterArenaInputSource::Invalid;
    std::uint32_t tick = 0;
    StarterArenaInputActions actions;
    StarterArenaVec2 movementVelocity;
};

struct StarterArenaInputEvidence
{
    std::string status = "NotRun";
    StarterArenaInputSource source = StarterArenaInputSource::Scene;
    std::uint32_t moveLeftCount = 0;
    std::uint32_t moveRightCount = 0;
    std::uint32_t moveUpCount = 0;
    std::uint32_t moveDownCount = 0;
    std::uint32_t quitCount = 0;
    std::uint32_t framesWithInput = 0;
    bool realDeviceObserved = false;
    std::filesystem::path syntheticScriptPath;
};

class IStarterArenaInputProvider
{
public:
    virtual ~IStarterArenaInputProvider() = default;
    [[nodiscard]] virtual StarterArenaInputSource Source() const noexcept = 0;
    [[nodiscard]] virtual StarterArenaInputFrame Read(std::uint32_t tick) = 0;
    virtual void Shutdown() noexcept {}
};

[[nodiscard]] const char* ToString(StarterArenaInputSource source) noexcept;

[[nodiscard]] std::unique_ptr<IStarterArenaInputProvider> CreateSceneInputProvider(
    StarterArenaVec2 authoredVelocity);

[[nodiscard]] std::unique_ptr<IStarterArenaInputProvider> CreateSyntheticInputProvider(
    const std::filesystem::path& scriptPath,
    std::string& error);

[[nodiscard]] std::unique_ptr<IStarterArenaInputProvider> CreateKeyboardInputProvider(
    std::unique_ptr<SagaEngine::Platform::IPlatformInputBackend> backend,
    std::string& error);

void RecordStarterArenaInput(const StarterArenaInputFrame& frame,
                             StarterArenaInputEvidence& evidence) noexcept;

void FinalizeStarterArenaInputEvidence(StarterArenaInputEvidence& evidence) noexcept;

} // namespace SagaRuntimeApp
