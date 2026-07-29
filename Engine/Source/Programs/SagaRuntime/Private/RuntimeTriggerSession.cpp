// SPDX-License-Identifier: MPL-2.0
#include "RuntimeTriggerSession.h"

#include "RuntimeScriptSession.h"

#include <SagaEngine/Physics/PhysicsWorld.h>

#include <algorithm>
#include <iterator>

namespace SagaRuntimeApp
{
namespace Play = SagaRuntime::Play;
namespace Physics = SagaEngine::Physics;

std::vector<Play::RuntimeDiagnostic> RuntimeTriggerSession::Tick()
{
    struct BodyBinding
    {
        Physics::BodyId body = Physics::BodyId::kInvalid;
        std::string entityId;
        std::string category;
        bool volume = false;
    };

    Physics::PhysicsWorld physics({.gravity = 0.0F});
    std::vector<BodyBinding> bindings;
    std::set<std::pair<std::string, std::string>> current;
    const auto& entities = world_.Scene().entities;
    bindings.reserve(entities.size());
    for (const auto& entity : entities)
    {
        if (!entity.active) continue;
        Physics::RigidBodyDesc descriptor;
        descriptor.position = {
            static_cast<float>(entity.transform.position[0]),
            static_cast<float>(entity.transform.position[1]),
            static_cast<float>(entity.transform.position[2])};
        descriptor.isTrigger = true;
        if (entity.triggerVolume && entity.triggerVolume->enabled)
        {
            descriptor.type = Physics::BodyType::Static;
            descriptor.shape = Physics::CollisionShape::Box(
                static_cast<float>(entity.triggerVolume->bounds[0] * 0.5),
                static_cast<float>(entity.triggerVolume->bounds[1] * 0.5),
                static_cast<float>(entity.triggerVolume->bounds[2] * 0.5));
            bindings.push_back({physics.AddBody(descriptor), entity.entityId,
                entity.triggerVolume->acceptedCategory, true});
        }
        if (entity.triggerParticipant)
        {
            descriptor.type = Physics::BodyType::Kinematic;
            descriptor.shape = Physics::CollisionShape::Box(
                static_cast<float>(entity.triggerParticipant->bounds[0] * 0.5),
                static_cast<float>(entity.triggerParticipant->bounds[1] * 0.5),
                static_cast<float>(entity.triggerParticipant->bounds[2] * 0.5));
            bindings.push_back({physics.AddBody(descriptor), entity.entityId,
                entity.triggerParticipant->category, false});
        }
    }
    physics.Step(1.0F / 60.0F);
    const auto bindingFor = [&](const Physics::BodyId body)
        -> const BodyBinding*
    {
        const auto found = std::find_if(bindings.begin(), bindings.end(),
            [body](const BodyBinding& binding) { return binding.body == body; });
        return found == bindings.end() ? nullptr : &*found;
    };
    for (const auto& contact : physics.GetContacts())
    {
        const auto* first = bindingFor(contact.bodyA);
        const auto* second = bindingFor(contact.bodyB);
        if (first == nullptr || second == nullptr ||
            first->volume == second->volume) continue;
        const auto* volume = first->volume ? first : second;
        const auto* participant = first->volume ? second : first;
        if (volume->category == participant->category)
            current.emplace(volume->entityId, participant->entityId);
    }

    std::vector<std::pair<std::string, std::string>> entered;
    std::vector<std::pair<std::string, std::string>> exited;
    std::set_difference(current.begin(), current.end(),
        overlaps_.begin(), overlaps_.end(), std::back_inserter(entered));
    std::set_difference(overlaps_.begin(), overlaps_.end(),
        current.begin(), current.end(), std::back_inserter(exited));
    overlaps_ = std::move(current);

    std::vector<Play::RuntimeDiagnostic> diagnostics;
    for (const auto& [source, _] : entered)
    {
        auto dispatched = dispatcher_.Dispatch("TriggerEnter", source);
        diagnostics.insert(diagnostics.end(),
            std::make_move_iterator(dispatched.begin()),
            std::make_move_iterator(dispatched.end()));
    }
    for (const auto& [source, _] : exited)
    {
        auto dispatched = dispatcher_.Dispatch("TriggerExit", source);
        diagnostics.insert(diagnostics.end(),
            std::make_move_iterator(dispatched.begin()),
            std::make_move_iterator(dispatched.end()));
    }
    return diagnostics;
}
} // namespace SagaRuntimeApp
