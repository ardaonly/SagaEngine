/// @file InputActionMap.cpp
/// @brief Resolves physical input chords into gameplay action events.

#include "SagaEngine/Input/Mapping/InputActionMap.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>

namespace SagaEngine::Input
{
namespace
{
    /// Compare two input chords by value without requiring operator== in the header.
    [[nodiscard]] bool ChordsEqual(const InputChord& lhs, const InputChord& rhs) noexcept
    {
        return std::visit(
            [](const auto& a, const auto& b) -> bool
            {
                using A = std::decay_t<decltype(a)>;
                using B = std::decay_t<decltype(b)>;

                if constexpr (!std::is_same_v<A, B>)
                {
                    return false;
                }
                else if constexpr (std::is_same_v<A, KeyChord>)
                {
                    return a.key == b.key && a.modifiers == b.modifiers;
                }
                else if constexpr (std::is_same_v<A, MouseButtonChord>)
                {
                    return a.button == b.button && a.modifiers == b.modifiers;
                }
                else if constexpr (std::is_same_v<A, GamepadButtonChord>)
                {
                    return a.button == b.button;
                }
                else if constexpr (std::is_same_v<A, GamepadAxisChord>)
                {
                    return a.axis == b.axis
                        && a.threshold == b.threshold
                        && a.direction == b.direction;
                }
                else
                {
                    return false;
                }
            },
            lhs,
            rhs);
    }

    /// Resolve a digital input transition into an action phase and value.
    [[nodiscard]] bool ResolveDigitalTransition(
        bool curDown,
        bool prevDown,
        ActionPhase& outPhase,
        float& outValue) noexcept
    {
        if (!curDown && !prevDown)
        {
            return false;
        }

        if (curDown && !prevDown)
        {
            outPhase = ActionPhase::Pressed;
            outValue = 1.f;
        }
        else if (!curDown && prevDown)
        {
            outPhase = ActionPhase::Released;
            outValue = 0.f;
        }
        else
        {
            outPhase = ActionPhase::Held;
            outValue = 1.f;
        }

        return true;
    }

    /// Check whether the required keyboard modifiers are currently held.
    [[nodiscard]] bool ModifiersSatisfied(
        ModifierFlags required,
        const KeyboardState& keyboard) noexcept
    {
        if (required == ModifierFlags::None)
        {
            return true;
        }

        if (HasModifier(required, ModifierFlags::Shift)
            && !keyboard.IsDown(KeyCode::LShift)
            && !keyboard.IsDown(KeyCode::RShift))
        {
            return false;
        }

        if (HasModifier(required, ModifierFlags::Ctrl)
            && !keyboard.IsDown(KeyCode::LCtrl)
            && !keyboard.IsDown(KeyCode::RCtrl))
        {
            return false;
        }

        if (HasModifier(required, ModifierFlags::Alt)
            && !keyboard.IsDown(KeyCode::LAlt)
            && !keyboard.IsDown(KeyCode::RAlt))
        {
            return false;
        }

        if (HasModifier(required, ModifierFlags::Meta)
            && !keyboard.IsDown(KeyCode::LMeta)
            && !keyboard.IsDown(KeyCode::RMeta))
        {
            return false;
        }

        return true;
    }

    /// Clamp a normalized axis value into [0, 1].
    [[nodiscard]] float ClampUnit(float value) noexcept
    {
        return std::clamp(value, 0.f, 1.f);
    }
} // namespace

// ─── Binding Management ──────────────────────────────────────────────────────

InputActionMap::InputActionMap(std::string name)
    : m_name(std::move(name))
{
}

void InputActionMap::Bind(InputActionId action, InputChord chord, bool consumeInput)
{
    m_bindings.push_back(InputBinding{ action, std::move(chord), consumeInput });
}

void InputActionMap::Unbind(InputActionId action)
{
    std::erase_if(m_bindings,
        [action](const InputBinding& binding)
        {
            return binding.id == action;
        });
}

void InputActionMap::UnbindChord(InputActionId action, const InputChord& chord)
{
    std::erase_if(m_bindings,
        [&](const InputBinding& binding)
        {
            return binding.id == action && ChordsEqual(binding.chord, chord);
        });
}

void InputActionMap::Clear()
{
    m_bindings.clear();
}

bool InputActionMap::LoadFromConfig(std::string_view /*configText*/)
{
    // Project-level parsing is intentionally left to the game's data layer.
    return false;
}

// ─── Resolution ─────────────────────────────────────────────────────────────

void InputActionMap::Resolve(
    const InputState& state,
    uint32_t tick,
    std::vector<InputActionEvent>& outEvents) const
{
    for (const auto& binding : m_bindings)
    {
        ActionPhase phase = ActionPhase::Pressed;
        float value = 0.f;

        if (!ResolveChord(binding.chord, state, phase, value))
        {
            continue;
        }

        outEvents.push_back(InputActionEvent{
            .id = binding.id,
            .phase = phase,
            .value = value,
            .tick = tick
        });
    }
}

/// Resolve one chord against the current input snapshot.
bool InputActionMap::ResolveChord(
    const InputChord& chord,
    const InputState& state,
    ActionPhase& outPhase,
    float& outValue) const noexcept
{
    return std::visit(
        [&](const auto& c) -> bool
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, KeyChord>)
            {
                if (!ModifiersSatisfied(c.modifiers, state.keyboard))
                {
                    return false;
                }

                const bool curDown = state.keyboard.IsDown(c.key);
                const bool prevDown = state.keyboard.WasDown(c.key);
                return ResolveDigitalTransition(curDown, prevDown, outPhase, outValue);
            }
            else if constexpr (std::is_same_v<T, MouseButtonChord>)
            {
                if (!ModifiersSatisfied(c.modifiers, state.keyboard))
                {
                    return false;
                }

                const bool curDown = state.mouse.IsButtonDown(c.button);
                const bool prevDown = state.mouse.buttonsPrevious.test(
                    static_cast<size_t>(c.button));

                return ResolveDigitalTransition(curDown, prevDown, outPhase, outValue);
            }
            else if constexpr (std::is_same_v<T, GamepadButtonChord>)
            {
                if (!state.gamepad.connected)
                {
                    return false;
                }

                const bool curDown = state.gamepad.IsButtonDown(c.button);
                const bool prevDown = state.gamepad.buttonsPrevious.test(
                    static_cast<size_t>(c.button));

                return ResolveDigitalTransition(curDown, prevDown, outPhase, outValue);
            }
            else if constexpr (std::is_same_v<T, GamepadAxisChord>)
            {
                if (!state.gamepad.connected)
                {
                    return false;
                }

                const float threshold = std::clamp(c.threshold, 0.f, 0.999f);
                const float raw = state.gamepad.GetAxis(c.axis);
                const float effective = raw * c.direction;

                if (effective <= threshold)
                {
                    return false;
                }

                const float normalized = (effective - threshold) / (1.f - threshold);
                outValue = ClampUnit(normalized);
                outPhase = ActionPhase::Analog;
                return true;
            }

            return false;
        },
        chord);
}

// ─── Stack Resolution ────────────────────────────────────────────────────────

void InputActionMapStack::Push(std::shared_ptr<InputActionMap> map)
{
    m_stack.push_back(std::move(map));
}

void InputActionMapStack::Pop()
{
    if (!m_stack.empty())
    {
        m_stack.pop_back();
    }
}

void InputActionMapStack::Remove(std::string_view name)
{
    std::erase_if(m_stack,
        [name](const std::shared_ptr<InputActionMap>& map)
        {
            return map && map->GetName() == name;
        });
}

void InputActionMapStack::Clear()
{
    m_stack.clear();
}

/// Resolve maps from highest priority to lowest priority.
///
/// The current API exposes resolved actions, not the underlying chord route,
/// so consumption is tracked at action-id granularity here. That is the best
/// API-compatible approximation until per-binding routing is surfaced.
std::vector<InputActionEvent> InputActionMapStack::ResolveAll(
    const InputState& state,
    uint32_t tick) const
{
    std::vector<InputActionEvent> events;
    events.reserve(16);

    std::unordered_set<InputActionId> consumedActions;

    for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it)
    {
        if (!*it)
        {
            continue;
        }

        std::vector<InputActionEvent> mapEvents;
        (*it)->Resolve(state, tick, mapEvents);

        for (const auto& ev : mapEvents)
        {
            if (consumedActions.contains(ev.id))
            {
                continue;
            }

            events.push_back(ev);
            consumedActions.insert(ev.id);
        }
    }

    return events;
}

} // namespace SagaEngine::Input