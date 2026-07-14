/// @file WorldStateScriptWorld.cpp
/// @brief Implements the SagaScript facade over Simulation::WorldState.

#include <SagaEngine/Scripting/WorldStateScriptWorld.hpp>

#include "SagaShared/Diagnostics/DiagnosticCategory.hpp"
#include "SagaShared/Diagnostics/DiagnosticSeverity.hpp"
#include "SagaShared/Diagnostics/DiagnosticSource.hpp"

#include "SagaEngine/ECS/Components/TransformComponent.h"
#include "SagaEngine/Simulation/WorldState.h"

#include <string>
#include <utility>

namespace SagaEngine::Scripting
{

namespace
{

[[nodiscard]] ScriptDiagnostic MakeDiagnostic(
    std::string code,
    std::string title,
    std::string message)
{
    ScriptDiagnostic diagnostic;
    diagnostic.diagnostic.severity =
        SagaShared::Diagnostics::DiagnosticSeverity::Error;
    diagnostic.diagnostic.category =
        SagaShared::Diagnostics::DiagnosticCategory::Script;
    diagnostic.diagnostic.source =
        SagaShared::Diagnostics::DiagnosticSource::Runtime;
    diagnostic.diagnostic.code.value = std::move(code);
    diagnostic.diagnostic.title = std::move(title);
    diagnostic.diagnostic.message = std::move(message);
    return diagnostic;
}

[[nodiscard]] ScriptHostOperationResult InvalidEntity(
    ScriptEntityHandle entity,
    std::string operation)
{
    ScriptHostOperationResult result;
    auto diagnostic = MakeDiagnostic(
        ScriptHostDiagnostics::InvalidEntityHandle,
        "Invalid script entity handle",
        "Script world operation received an invalid entity handle.");
    diagnostic.metadata["entityHandle"] = std::to_string(entity.value);
    diagnostic.metadata["operation"] = std::move(operation);
    result.diagnostics.push_back(std::move(diagnostic));
    return result;
}

} // namespace

WorldStateScriptWorld::WorldStateScriptWorld(Simulation::WorldState& world)
    : world_(world)
{
}

ScriptEntityCreateResult WorldStateScriptWorld::CreateEntity(
    const std::string& name)
{
    const auto entity = world_.CreateEntity();
    const auto scriptHandle = nextEntityHandle_++;
    entities_.emplace(
        scriptHandle,
        EntityBinding{entity.id, entity.version, name});

    ScriptEntityCreateResult result;
    result.succeeded = true;
    result.entity.value = scriptHandle;
    return result;
}

ScriptHostOperationResult WorldStateScriptWorld::SetPosition(
    const ScriptEntityHandle entity,
    const ScriptVector3 position)
{
    const auto* binding = Resolve(entity);
    if (binding == nullptr || !IsAlive(*binding))
    {
        return InvalidEntity(entity, "SetPosition");
    }

    using SagaEngine::ECS::TransformComponent;
    auto* transform = world_.GetComponent<TransformComponent>(binding->entityId);
    if (transform == nullptr)
    {
        transform = &world_.AddComponent<TransformComponent>(binding->entityId);
    }

    transform->transform.position.x = position.x;
    transform->transform.position.y = position.y;
    transform->transform.position.z = position.z;
    transform->MarkPositionDirty();

    ScriptHostOperationResult result;
    result.succeeded = true;
    return result;
}

ScriptEntityPositionResult WorldStateScriptWorld::GetPosition(
    const ScriptEntityHandle entity) const
{
    const auto* binding = Resolve(entity);
    if (binding == nullptr || !IsAlive(*binding))
    {
        ScriptEntityPositionResult result;
        auto invalid = InvalidEntity(entity, "GetPosition");
        result.diagnostics = std::move(invalid.diagnostics);
        return result;
    }

    ScriptEntityPositionResult result;
    using SagaEngine::ECS::TransformComponent;
    const auto* transform =
        world_.GetComponent<TransformComponent>(binding->entityId);
    if (transform != nullptr)
    {
        result.position.x = transform->transform.position.x;
        result.position.y = transform->transform.position.y;
        result.position.z = transform->transform.position.z;
    }

    result.succeeded = true;
    return result;
}

const WorldStateScriptWorld::EntityBinding* WorldStateScriptWorld::Resolve(
    const ScriptEntityHandle entity) const noexcept
{
    const auto iterator = entities_.find(entity.value);
    return iterator == entities_.end() ? nullptr : &iterator->second;
}

WorldStateScriptWorld::EntityBinding* WorldStateScriptWorld::Resolve(
    const ScriptEntityHandle entity) noexcept
{
    const auto iterator = entities_.find(entity.value);
    return iterator == entities_.end() ? nullptr : &iterator->second;
}

bool WorldStateScriptWorld::IsAlive(
    const EntityBinding& binding) const noexcept
{
    return world_.IsValid(SagaEngine::ECS::EntityHandle{
        binding.entityId,
        binding.version,
    });
}

} // namespace SagaEngine::Scripting
