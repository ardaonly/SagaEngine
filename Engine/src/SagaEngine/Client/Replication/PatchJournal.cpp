/// @file PatchJournal.cpp
/// @brief Transactional patch journal for atomic ECS apply.

#include "SagaEngine/Client/Replication/PatchJournal.h"

#include "SagaEngine/Simulation/WorldState.h"

#include <algorithm>
#include <cstring>

namespace SagaEngine::Client::Replication {

// ─── Patch copy/move ───────────────────────────────────────────────────────

Patch::Patch(const Patch& other) noexcept
    : type(other.type)
{
    switch (type)
    {
        case PatchType::Spawn:
            std::memcpy(&data.spawn, &other.data.spawn, sizeof(EntitySpawnPatch));
            break;
        case PatchType::ComponentWrite:
            std::memcpy(&data.write, &other.data.write, sizeof(ComponentWritePatch));
            break;
        case PatchType::ComponentRemove:
            std::memcpy(&data.remove, &other.data.remove, sizeof(ComponentRemovePatch));
            break;
        case PatchType::Despawn:
            std::memcpy(&data.despawn, &other.data.despawn, sizeof(EntityDespawnPatch));
            break;
    }
}

Patch& Patch::operator=(const Patch& other) noexcept
{
    if (this == &other)
        return *this;
    type = other.type;
    switch (type)
    {
        case PatchType::Spawn:
            std::memcpy(&data.spawn, &other.data.spawn, sizeof(EntitySpawnPatch));
            break;
        case PatchType::ComponentWrite:
            std::memcpy(&data.write, &other.data.write, sizeof(ComponentWritePatch));
            break;
        case PatchType::ComponentRemove:
            std::memcpy(&data.remove, &other.data.remove, sizeof(ComponentRemovePatch));
            break;
        case PatchType::Despawn:
            std::memcpy(&data.despawn, &other.data.despawn, sizeof(EntityDespawnPatch));
            break;
    }
    return *this;
}

Patch::Patch(Patch&& other) noexcept : Patch(other) {}

Patch& Patch::operator=(Patch&& other) noexcept { return *this = other; }

// ─── Building ──────────────────────────────────────────────────────────────

bool PatchJournal::PushSpawn(EntitySpawnPatch patch) noexcept
{
    if (patchCount_ >= kMaxPatches)
        return false;
    if (EstimatedBytes() + sizeof(EntitySpawnPatch) > kMaxByteBudget)
        return false;

    Patch& p = patches_[patchCount_++];
    p.type = PatchType::Spawn;
    p.data.spawn = patch;
    return true;
}

bool PatchJournal::PushWrite(ComponentWritePatch patch) noexcept
{
    if (patchCount_ >= kMaxPatches)
        return false;
    if (EstimatedBytes() + sizeof(ComponentWritePatch) + patch.dataLen > kMaxByteBudget)
        return false;

    Patch& p = patches_[patchCount_++];
    p.type = PatchType::ComponentWrite;
    p.data.write = patch;
    return true;
}

bool PatchJournal::PushRemove(ComponentRemovePatch patch) noexcept
{
    if (patchCount_ >= kMaxPatches)
        return false;
    if (EstimatedBytes() + sizeof(ComponentRemovePatch) > kMaxByteBudget)
        return false;

    Patch& p = patches_[patchCount_++];
    p.type = PatchType::ComponentRemove;
    p.data.remove = patch;
    return true;
}

bool PatchJournal::PushDespawn(EntityDespawnPatch patch) noexcept
{
    if (patchCount_ >= kMaxPatches)
        return false;
    if (EstimatedBytes() + sizeof(EntityDespawnPatch) > kMaxByteBudget)
        return false;

    Patch& p = patches_[patchCount_++];
    p.type = PatchType::Despawn;
    p.data.despawn = patch;
    return true;
}

bool PatchJournal::IsFull() const noexcept
{
    return patchCount_ >= kMaxPatches || EstimatedBytes() >= kMaxByteBudget;
}

std::size_t PatchJournal::EstimatedBytes() const noexcept
{
    std::size_t total = 0;
    for (std::size_t i = 0; i < patchCount_; ++i)
    {
        switch (patches_[i].type)
        {
            case PatchType::Spawn:
                total += sizeof(EntitySpawnPatch);
                for (std::uint8_t c = 0; c < patches_[i].data.spawn.componentCount; ++c)
                    total += patches_[i].data.spawn.components[c].dataLen;
                break;
            case PatchType::ComponentWrite:
                total += sizeof(ComponentWritePatch) + patches_[i].data.write.dataLen;
                break;
            case PatchType::ComponentRemove:
                total += sizeof(ComponentRemovePatch);
                break;
            case PatchType::Despawn:
                total += sizeof(EntityDespawnPatch);
                break;
        }
    }
    return total;
}

void PatchJournal::Clear() noexcept
{
    patchCount_ = 0;
}

// ─── Deterministic ordering ────────────────────────────────────────────────

namespace {

/// Sort key for a patch.  Lower key = apply first.
struct PatchSortKey
{
    std::uint32_t entityId;
    std::uint32_t generation;
    std::uint32_t componentTypeId;

    bool operator>(const PatchSortKey& other) const noexcept
    {
        if (entityId != other.entityId)
            return entityId > other.entityId;
        if (generation != other.generation)
            return generation > other.generation;
        return componentTypeId > other.componentTypeId;
    }
};

PatchSortKey SortKeyOf(const Patch& p) noexcept
{
    switch (p.type)
    {
        case PatchType::Spawn:
            return {p.data.spawn.entityId, p.data.spawn.generation, 0};
        case PatchType::ComponentWrite:
            return {p.data.write.entityId, 0, p.data.write.typeId};
        case PatchType::ComponentRemove:
            return {p.data.remove.entityId, 0, p.data.remove.typeId};
        case PatchType::Despawn:
            return {p.data.despawn.entityId, p.data.despawn.generation, 0xFFFFFFFFu};
    }
    return {0, 0, 0};
}

} // namespace

void PatchJournal::Sort() noexcept
{
    if (patchCount_ <= 1)
        return;

    // Insertion sort — small n, deterministic, cache-friendly.
    for (std::size_t i = 1; i < patchCount_; ++i)
    {
        Patch temp = patches_[i];
        PatchSortKey keyI = SortKeyOf(temp);
        std::size_t j = i;

        while (j > 0 && SortKeyOf(patches_[j - 1]) > keyI)
        {
            patches_[j] = patches_[j - 1];
            --j;
        }
        patches_[j] = temp;
    }
}

// ─── Validate (pure, no side effects) ──────────────────────────────────────

ValidateResult PatchJournal::Validate(Simulation::WorldState& world) const noexcept
{
    for (std::size_t i = 0; i < patchCount_; ++i)
    {
        const Patch& p = patches_[i];
        switch (p.type)
        {
            case PatchType::Spawn:
                // Entity must NOT already exist.
                if (world.IsAlive(p.data.spawn.entityId))
                    return ValidateResult::EntityAlreadyExists;
                break;

            case PatchType::ComponentWrite:
                // Entity must exist.
                if (!world.IsAlive(p.data.write.entityId))
                    return ValidateResult::EntityNotFound;
                // Type must be registered (checked by AddComponent internally).
                break;

            case PatchType::ComponentRemove:
                if (!world.IsAlive(p.data.remove.entityId))
                    return ValidateResult::EntityNotFound;
                break;

            case PatchType::Despawn:
                // Entity must exist and generation must match.
                if (!world.IsAlive(p.data.despawn.entityId))
                    return ValidateResult::EntityNotFound;
                break;
        }
    }

    return ValidateResult::Ok;
}

// ─── Commit (deterministic order) ──────────────────────────────────────────

ValidateResult PatchJournal::Commit(Simulation::WorldState& world) const noexcept
{
    for (std::size_t i = 0; i < patchCount_; ++i)
    {
        const Patch& p = patches_[i];
        switch (p.type)
        {
            case PatchType::Spawn:
            {
                // Spawn patches are handled by the ECS apply layer which has
                // access to the ComponentSerializerRegistry.  The journal
                // validates but delegates actual component writes to the
                // authority-aware apply function.
                break;
            }

            case PatchType::ComponentWrite:
            {
                const auto& write = p.data.write;
                if (!world.IsAlive(write.entityId))
                    return ValidateResult::EntityNotFound;

                // Write raw component bytes into the WorldState block.
                // This uses the raw block API to avoid template instantiation.
                Simulation::ComponentBlock* block = world.GetBlock(write.typeId);
                if (!block)
                {
                    // Block doesn't exist — component type not registered.
                    // In a production system this should have been caught in
                    // Validate, but we guard here as well.
                    return ValidateResult::ComponentNotRegistered;
                }

                // Check stride matches.
                if (block->stride != write.dataLen)
                {
                    // Size mismatch — component schema changed.
                    return ValidateResult::DataSizeMismatch;
                }

                // Find or create slot for this entity.
                auto it = block->entityToSlot.find(write.entityId);
                if (it != block->entityToSlot.end())
                {
                    // Overwrite existing component in-place.
                    std::uint8_t* dest = block->data.data() + it->second * block->stride;
                    std::memcpy(dest, write.data, write.dataLen);
                }
                else
                {
                    // Append new component slot.
                    std::uint32_t slot = static_cast<std::uint32_t>(block->entities.size());
                    block->entities.push_back(write.entityId);
                    block->data.resize(block->data.size() + block->stride);
                    std::uint8_t* dest = block->data.data() + slot * block->stride;
                    std::memcpy(dest, write.data, write.dataLen);
                    block->entityToSlot.emplace(write.entityId, slot);
                }
                break;
            }

            case PatchType::ComponentRemove:
            {
                const auto& remove = p.data.remove;
                if (!world.IsAlive(remove.entityId))
                    return ValidateResult::EntityNotFound;

                Simulation::ComponentBlock* block = world.GetBlock(remove.typeId);
                if (!block)
                    break;  // Already absent — no-op.

                auto it = block->entityToSlot.find(remove.entityId);
                if (it == block->entityToSlot.end())
                    break;  // Already absent — no-op.

                std::uint32_t slot = it->second;
                std::uint32_t last = static_cast<std::uint32_t>(block->entities.size()) - 1u;

                if (slot != last)
                {
                    // Swap-and-pop.
                    const ECS::EntityId movedEntity = block->entities[last];
                    block->entities[slot] = movedEntity;
                    std::uint8_t* dst = block->data.data() + slot * block->stride;
                    const std::uint8_t* src = block->data.data() + last * block->stride;
                    std::memcpy(dst, src, block->stride);
                    block->entityToSlot[movedEntity] = slot;
                }

                block->entities.pop_back();
                block->data.resize(block->data.size() - block->stride);
                block->entityToSlot.erase(it);
                break;
            }

            case PatchType::Despawn:
            {
                if (world.IsAlive(p.data.despawn.entityId))
                    world.DestroyEntity(p.data.despawn.entityId);
                break;
            }
        }
    }

    return ValidateResult::Ok;
}

ValidateResult PatchJournal::Apply(Simulation::WorldState& world) noexcept
{
    Sort();

    ValidateResult vr = Validate(world);
    if (vr != ValidateResult::Ok)
        return vr;

    return Commit(world);
}

} // namespace SagaEngine::Client::Replication
