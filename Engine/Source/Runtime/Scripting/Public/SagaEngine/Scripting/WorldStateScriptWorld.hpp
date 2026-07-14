/// @file WorldStateScriptWorld.hpp
/// @brief Adapts Simulation::WorldState to the minimal SagaScript world facade.

#pragma once

#include "SagaEngine/Scripting/Namespace.hpp"
#include "SagaEngine/Scripting/ISagaScriptHost.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace SagaEngine::Simulation
{
class WorldState;
}

namespace SagaEngine::Scripting
{

/// Script-facing facade over the authoritative simulation WorldState.
class WorldStateScriptWorld final : public ISagaScriptWorld
{
public:
    explicit WorldStateScriptWorld(Simulation::WorldState& world);

    [[nodiscard]] ScriptEntityCreateResult CreateEntity(
        const std::string& name) override;

    [[nodiscard]] ScriptHostOperationResult SetPosition(
        ScriptEntityHandle entity,
        ScriptVector3 position) override;

    [[nodiscard]] ScriptEntityPositionResult GetPosition(
        ScriptEntityHandle entity) const override;

private:
    struct EntityBinding
    {
        std::uint32_t entityId = 0;
        std::uint16_t version = 0;
        std::string name;
    };

    [[nodiscard]] const EntityBinding* Resolve(
        ScriptEntityHandle entity) const noexcept;
    [[nodiscard]] EntityBinding* Resolve(
        ScriptEntityHandle entity) noexcept;
    [[nodiscard]] bool IsAlive(const EntityBinding& binding) const noexcept;

    Simulation::WorldState& world_;
    std::uint64_t nextEntityHandle_ = 1;
    std::unordered_map<std::uint64_t, EntityBinding> entities_;
};

} // namespace SagaEngine::Scripting
