// SPDX-License-Identifier: MIT

using SagaEngine.Scripting;
using SagaEngine.Scripting.Gameplay;

namespace ArenaRush;

/// <summary>Sample-owned composition around the reusable collectible value contract.</summary>
public sealed class EnergyCoreController : SagaBehavior
{
    public readonly record struct EnergyCoreState(CollectibleState Value);

    public static EnergyCoreState Create(double respawnDelaySeconds, int spawnCount, int index) =>
        new(CollectiblesHighLevel.Create(
            new CollectibleDefinition(respawnDelaySeconds, spawnCount, index)));

    public static EnergyCoreState Reset(EnergyCoreState state) =>
        new(CollectiblesHighLevel.Reset(state.Value));

    public static EnergyCoreState Collect(EnergyCoreState state) =>
        new(CollectiblesHighLevel.ScheduleRespawn(
            CollectiblesHighLevel.Deactivate(
                CollectiblesHighLevel.Collect(state.Value))));

    public static (EnergyCoreState State, bool Completed) Advance(
        EnergyCoreState state, double deltaSeconds, int selectionOrdinal)
    {
        var advanced = CollectiblesHighLevel.AdvanceRespawnTime(state.Value, deltaSeconds);
        var completed = CollectiblesHighLevel.CompleteRespawn(advanced, selectionOrdinal);
        return (new EnergyCoreState(completed),
            advanced.Phase == CollectiblePhase.Respawning &&
            completed.Phase == CollectiblePhase.Active);
    }
}
