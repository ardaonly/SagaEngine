#pragma once

/// @file InputActionMap.h
/// @brief Maps physical input state to gameplay action events.
///
/// Layer  : Input / Mapping
/// Purpose: Given an InputState (which keys/buttons/axes are active),
///          produces a list of InputActionEvents for this frame.
///
///          This is the single layer where "W pressed" becomes "MoveForward".
///          Below this layer: physical codes. Above this layer: action IDs only.
///
/// Design notes:
///   - Maps are named and swappable at runtime (menu map, gameplay map, etc.).
///   - Multiple chords can bind to the same action (keyboard + gamepad).
///   - Map priority: if multiple maps are active, higher priority wins for
///     consumed inputs.
///   - InputActionMap is completely stateless regarding InputState — it
///     only reads from the state passed to Resolve() each frame.
///   - InputActionMap has NO knowledge of:
///       * Network, serialization, tick numbers
///       * SDL or any backend
///       * Game simulation

#include "SagaEngine/Input/Mapping/InputAction.h"
#include "SagaEngine/Input/Core/InputState.h"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>

namespace SagaEngine::Input
{

class InputActionMap
{
public:
    explicit InputActionMap(std::string name);

    // Map identity

    [[nodiscard]] std::string_view GetName() const noexcept { return m_name; }

    // Binding management

    /// Add a binding. Multiple bindings per action are allowed.
    void Bind(InputActionId action, InputChord chord, bool consumeInput = true);

    /// Remove all bindings for an action.
    void Unbind(InputActionId action);

    /// Remove a specific chord binding.
    void UnbindChord(InputActionId action, const InputChord& chord);

    /// Remove all bindings.
    void Clear();

    // Serialization

    /// Load bindings from a simple key-value config format.
    /// Actual format TBD per project. Returns false on parse error.
    bool LoadFromConfig(std::string_view configText);

    // Resolution

    /// Given the current InputState, produce all active action events.
    /// Call once per frame after all devices have been updated.
    /// outEvents is appended to (not cleared) — caller may pre-populate.
    ///
    /// @param state        The current input state (keyboard + mouse + gamepad)
    /// @param tick         Simulation tick to stamp on each event
    /// @param outEvents    Output vector — events are appended
    void Resolve(
        const InputState& state,
        uint32_t          tick,
        std::vector<InputActionEvent>& outEvents
    ) const;

    // Diagnostics

    [[nodiscard]] size_t GetBindingCount() const noexcept { return m_bindings.size(); }

private:
    bool ResolveChord(
        const InputChord&  chord,
        const InputState&  state,
        ActionPhase&       outPhase,
        float&             outValue
    ) const noexcept;

    std::string                    m_name;
    std::vector<InputBinding>      m_bindings;
};

// Stack of maps
/// Multiple maps can be active simultaneously (e.g. vehicle + global bindings).
/// Consumed inputs in higher-priority maps block lower maps.

class InputActionMapStack
{
public:
    /// Push a map. Highest push order = highest priority.
    void Push(std::shared_ptr<InputActionMap> map);

    /// Pop the top map.
    void Pop();

    /// Remove a specific map by name.
    void Remove(std::string_view name);

    /// Resolve all active maps in priority order.
    [[nodiscard]] std::vector<InputActionEvent> ResolveAll(
        const InputState& state,
        uint32_t          tick
    ) const;

    void Clear();

private:
    std::vector<std::shared_ptr<InputActionMap>> m_stack;
};

} // namespace SagaEngine::Input