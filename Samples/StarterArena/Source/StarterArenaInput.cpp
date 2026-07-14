/// @file StarterArenaInput.cpp
/// @brief App-local StarterArena scene, synthetic, and keyboard input providers.

#include "StarterArenaInput.h"

#include <SagaEngine/Input/Core/InputManager.h>
#include <SagaEngine/Input/Devices/InputDevice.h>
#include <SagaEngine/Input/Devices/KeyboardDevice.h>
#include <SagaEngine/Input/Mapping/InputActionMap.h>
#include <SagaEngine/Input/Mapping/GameplayInputFrame.h>
#include <SagaEngine/Input/Types/InputTypes.h>
#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace SagaRuntimeApp
{
namespace
{

constexpr double kMovementSpeed = 2.0;
constexpr double kInvSqrt2 = 0.70710678118654752440;
constexpr SagaEngine::Input::InputActionId kMoveLeft = 1;
constexpr SagaEngine::Input::InputActionId kMoveRight = 2;
constexpr SagaEngine::Input::InputActionId kMoveUp = 3;
constexpr SagaEngine::Input::InputActionId kMoveDown = 4;
constexpr SagaEngine::Input::InputActionId kQuit = 5;

StarterArenaVec2 MovementForActions(const StarterArenaInputActions& actions) noexcept
{
    double x = static_cast<double>(actions.moveRight) -
               static_cast<double>(actions.moveLeft);
    double y = static_cast<double>(actions.moveUp) -
               static_cast<double>(actions.moveDown);
    if (x != 0.0 && y != 0.0)
    {
        x *= kInvSqrt2;
        y *= kInvSqrt2;
    }
    return {x * kMovementSpeed, y * kMovementSpeed};
}

class SceneInputProvider final : public IStarterArenaInputProvider
{
public:
    explicit SceneInputProvider(StarterArenaVec2 velocity) : velocity_(velocity) {}

    StarterArenaInputSource Source() const noexcept override
    {
        return StarterArenaInputSource::Scene;
    }

    StarterArenaInputFrame Read(std::uint32_t tick) override
    {
        StarterArenaInputFrame frame;
        frame.source = Source();
        frame.tick = tick;
        frame.movementVelocity = velocity_;
        return frame;
    }

private:
    StarterArenaVec2 velocity_;
};

struct SyntheticSegment
{
    std::uint32_t startTick = 0;
    std::uint32_t endTickExclusive = 0;
    StarterArenaInputActions actions;
};

class SyntheticInputProvider final : public IStarterArenaInputProvider
{
public:
    explicit SyntheticInputProvider(std::vector<SyntheticSegment> segments)
        : segments_(std::move(segments))
    {
    }

    StarterArenaInputSource Source() const noexcept override
    {
        return StarterArenaInputSource::Synthetic;
    }

    StarterArenaInputFrame Read(std::uint32_t tick) override
    {
        StarterArenaInputFrame frame;
        frame.source = Source();
        frame.tick = tick;
        for (const SyntheticSegment& segment : segments_)
        {
            if (tick >= segment.startTick && tick < segment.endTickExclusive)
            {
                frame.actions = segment.actions;
                break;
            }
            if (segment.startTick > tick)
            {
                break;
            }
        }
        frame.movementVelocity = MovementForActions(frame.actions);
        return frame;
    }

private:
    std::vector<SyntheticSegment> segments_;
};

class KeyboardInputProvider final : public IStarterArenaInputProvider
{
public:
    explicit KeyboardInputProvider(
        std::unique_ptr<SagaEngine::Platform::IPlatformInputBackend> backend)
    {
        using namespace SagaEngine::Input;
        manager_.SetBackend(std::move(backend));
        manager_.RegisterDevice(MakeKeyboardDevice(1));

        auto actionMap = std::make_shared<InputActionMap>("StarterArenaKeyboard");
        actionMap->Bind(kMoveLeft, KeyChord{KeyCode::A});
        actionMap->Bind(kMoveRight, KeyChord{KeyCode::D});
        actionMap->Bind(kMoveUp, KeyChord{KeyCode::W});
        actionMap->Bind(kMoveDown, KeyChord{KeyCode::S});
        actionMap->Bind(kQuit, KeyChord{KeyCode::Escape});
        manager_.PushActionMap(std::move(actionMap));
    }

    bool Initialize() { return manager_.Initialize(); }

    StarterArenaInputSource Source() const noexcept override
    {
        return StarterArenaInputSource::Keyboard;
    }

    StarterArenaInputFrame Read(std::uint32_t tick) override
    {
        manager_.Update(tick);
        const SagaEngine::Input::GameplayInputFrame& gameplay = manager_.GetCurrentFrame();
        StarterArenaInputFrame frame;
        frame.source = Source();
        frame.tick = tick;
        frame.actions.moveLeft = gameplay.IsHeld(kMoveLeft);
        frame.actions.moveRight = gameplay.IsHeld(kMoveRight);
        frame.actions.moveUp = gameplay.IsHeld(kMoveUp);
        frame.actions.moveDown = gameplay.IsHeld(kMoveDown);
        frame.actions.quit = gameplay.IsHeld(kQuit);
        frame.movementVelocity = MovementForActions(frame.actions);
        return frame;
    }

    void Shutdown() noexcept override { manager_.Shutdown(); }

private:
    SagaEngine::Input::InputManager manager_;
};

bool ReadUnsigned(const nlohmann::json& value, std::uint32_t& output)
{
    if (!value.is_number_unsigned())
    {
        return false;
    }
    const auto parsed = value.get<std::uint64_t>();
    if (parsed > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    output = static_cast<std::uint32_t>(parsed);
    return true;
}

bool ParseAction(std::string_view name, StarterArenaInputActions& actions)
{
    if (name == "MoveLeft") actions.moveLeft = true;
    else if (name == "MoveRight") actions.moveRight = true;
    else if (name == "MoveUp") actions.moveUp = true;
    else if (name == "MoveDown") actions.moveDown = true;
    else return false;
    return true;
}

std::optional<std::vector<SyntheticSegment>> ParseSyntheticScript(
    const std::filesystem::path& path,
    std::string& error)
{
    std::ifstream input(path);
    if (!input)
    {
        error = "Failed to open synthetic input script: " + path.generic_string();
        return std::nullopt;
    }

    nlohmann::json document;
    try
    {
        input >> document;
    }
    catch (const nlohmann::json::exception& exception)
    {
        error = "Synthetic input script is not valid JSON: " + std::string(exception.what());
        return std::nullopt;
    }
    if (!document.is_object() || !document.contains("schemaVersion") ||
        !document["schemaVersion"].is_number_unsigned() ||
        document["schemaVersion"].get<std::uint64_t>() != 1u ||
        !document.contains("sourceKind") || !document["sourceKind"].is_string() ||
        document["sourceKind"].get<std::string>() != "StarterArenaSyntheticInput" ||
        !document.contains("segments") || !document["segments"].is_array())
    {
        error = "Synthetic input script requires schemaVersion 1, sourceKind "
                "StarterArenaSyntheticInput, and a segments array.";
        return std::nullopt;
    }

    std::vector<SyntheticSegment> segments;
    std::uint32_t previousEnd = 0;
    for (const nlohmann::json& value : document["segments"])
    {
        SyntheticSegment segment;
        if (!value.is_object() || !value.contains("startTick") ||
            !value.contains("endTickExclusive") || !value.contains("actions") ||
            !ReadUnsigned(value["startTick"], segment.startTick) ||
            !ReadUnsigned(value["endTickExclusive"], segment.endTickExclusive) ||
            !value["actions"].is_array() || segment.endTickExclusive <= segment.startTick ||
            (!segments.empty() && segment.startTick < previousEnd))
        {
            error = "Synthetic input segments must use ordered, non-overlapping unsigned tick "
                    "ranges with endTickExclusive greater than startTick.";
            return std::nullopt;
        }
        std::unordered_set<std::string> uniqueActions;
        for (const nlohmann::json& actionValue : value["actions"])
        {
            if (!actionValue.is_string())
            {
                error = "Synthetic input action names must be strings.";
                return std::nullopt;
            }
            const std::string action = actionValue.get<std::string>();
            if (!uniqueActions.insert(action).second)
            {
                error = "Synthetic input actions must be unique within each segment.";
                return std::nullopt;
            }
            if (!ParseAction(action, segment.actions))
            {
                error = "Unknown synthetic input action: " + action;
                return std::nullopt;
            }
        }
        previousEnd = segment.endTickExclusive;
        segments.push_back(segment);
    }
    return segments;
}

} // namespace

bool StarterArenaInputActions::Any() const noexcept
{
    return moveLeft || moveRight || moveUp || moveDown || quit;
}

const char* ToString(StarterArenaInputSource source) noexcept
{
    switch (source)
    {
        case StarterArenaInputSource::Invalid: return "invalid";
        case StarterArenaInputSource::Scene: return "scene";
        case StarterArenaInputSource::Synthetic: return "synthetic";
        case StarterArenaInputSource::Keyboard: return "keyboard";
    }
    return "invalid";
}

std::unique_ptr<IStarterArenaInputProvider> CreateSceneInputProvider(
    StarterArenaVec2 authoredVelocity)
{
    return std::make_unique<SceneInputProvider>(authoredVelocity);
}

std::unique_ptr<IStarterArenaInputProvider> CreateSyntheticInputProvider(
    const std::filesystem::path& scriptPath,
    std::string& error)
{
    auto segments = ParseSyntheticScript(scriptPath, error);
    if (!segments)
    {
        return nullptr;
    }
    return std::make_unique<SyntheticInputProvider>(std::move(*segments));
}

std::unique_ptr<IStarterArenaInputProvider> CreateKeyboardInputProvider(
    std::unique_ptr<SagaEngine::Platform::IPlatformInputBackend> backend,
    std::string& error)
{
    if (!backend)
    {
        error = "Keyboard input requires a platform input backend.";
        return nullptr;
    }
    auto provider = std::make_unique<KeyboardInputProvider>(std::move(backend));
    if (!provider->Initialize())
    {
        error = "The keyboard input backend failed to initialize.";
        return nullptr;
    }
    return provider;
}

void RecordStarterArenaInput(const StarterArenaInputFrame& frame,
                             StarterArenaInputEvidence& evidence) noexcept
{
    evidence.source = frame.source;
    evidence.moveLeftCount += static_cast<std::uint32_t>(frame.actions.moveLeft);
    evidence.moveRightCount += static_cast<std::uint32_t>(frame.actions.moveRight);
    evidence.moveUpCount += static_cast<std::uint32_t>(frame.actions.moveUp);
    evidence.moveDownCount += static_cast<std::uint32_t>(frame.actions.moveDown);
    evidence.quitCount += static_cast<std::uint32_t>(frame.actions.quit);
    const bool movementObserved = frame.movementVelocity.x != 0.0 ||
                                  frame.movementVelocity.y != 0.0;
    if (frame.actions.Any() || movementObserved)
    {
        ++evidence.framesWithInput;
    }
    if (frame.source == StarterArenaInputSource::Keyboard && frame.actions.Any())
    {
        evidence.realDeviceObserved = true;
    }
}

void FinalizeStarterArenaInputEvidence(StarterArenaInputEvidence& evidence) noexcept
{
    if (evidence.status == "Failed")
    {
        return;
    }
    evidence.status = evidence.source == StarterArenaInputSource::Keyboard &&
                              !evidence.realDeviceObserved
                          ? "NoInputObserved"
                          : "Passed";
}

} // namespace SagaRuntimeApp
