// SPDX-License-Identifier: MIT

using System;
using System.Linq;
using SagaEngine.Scripting.Gameplay;

namespace ArenaRush;

// SAGA-COLLECTIBLE-SURFACE-BEGIN
/// <summary>High-level collectible authority used by the default sample source.</summary>
public sealed class ArenaRushCollectibleStore
{
    private readonly CollectibleState[] states;
    private readonly bool[] completed;

    public ArenaRushCollectibleStore(int capacity, double respawnDelaySeconds, int spawnCount)
    {
        if (capacity <= 0) throw new ArgumentOutOfRangeException(nameof(capacity));
        states = new CollectibleState[capacity];
        completed = new bool[capacity];
        for (var index = 0; index < states.Length; ++index)
            states[index] = CollectiblesHighLevel.Create(
                new CollectibleDefinition(respawnDelaySeconds, spawnCount, index));
    }

    public int Count => states.Length;
    public int ActiveCount => states.Count(state => state.Phase == CollectiblePhase.Active);
    public int PendingRespawnCount => states.Count(
        state => state.Phase == CollectiblePhase.Respawning);

    public void Reset()
    {
        for (var index = 0; index < states.Length; ++index)
        {
            states[index] = CollectiblesHighLevel.Reset(states[index]);
            completed[index] = false;
        }
    }

    public bool TryCollect(int index)
    {
        if ((uint)index >= (uint)states.Length ||
            states[index].Phase != CollectiblePhase.Active) return false;
        states[index] = CollectiblesHighLevel.ScheduleRespawn(
            CollectiblesHighLevel.Deactivate(CollectiblesHighLevel.Collect(states[index])));
        return true;
    }

    public int AdvanceAll(double deltaSeconds, int selectionOrdinalBase)
    {
        var completionCount = 0;
        for (var index = 0; index < states.Length; ++index)
        {
            completed[index] = false;
            var advanced = CollectiblesHighLevel.AdvanceRespawnTime(states[index], deltaSeconds);
            var next = CollectiblesHighLevel.CompleteRespawn(
                advanced, selectionOrdinalBase + completionCount);
            completed[index] = advanced.Phase == CollectiblePhase.Respawning &&
                next.Phase == CollectiblePhase.Active;
            if (completed[index]) ++completionCount;
            states[index] = next;
        }
        return completionCount;
    }

    public bool CompletedRespawn(int index) => completed[index];
    public int SpawnIndex(int index) => states[index].SpawnIndex;
}
// SAGA-COLLECTIBLE-SURFACE-END
