// SPDX-License-Identifier: MPL-2.0

namespace SagaEngine.Scripting.Gameplay;

/// <summary>
/// Preallocated collectible lifecycle storage for measured high-density workloads.
/// Update and lifecycle methods mutate reusable value slots and allocate no objects.
/// </summary>
public sealed class CollectiblesLowLevelBatch
{
    private readonly CollectiblePhase[] phases;
    private readonly int[] spawnIndices;
    private readonly double[] respawnRemaining;
    private readonly double respawnDelaySeconds;
    private readonly int spawnCount;

    public CollectiblesLowLevelBatch(
        int capacity,
        double respawnDelaySeconds,
        int spawnCount)
    {
        if (capacity <= 0) throw new ArgumentOutOfRangeException(nameof(capacity));
        if (!double.IsFinite(respawnDelaySeconds) || respawnDelaySeconds < 0)
            throw new ArgumentOutOfRangeException(nameof(respawnDelaySeconds));
        if (spawnCount <= 0) throw new ArgumentOutOfRangeException(nameof(spawnCount));
        phases = new CollectiblePhase[capacity];
        spawnIndices = new int[capacity];
        respawnRemaining = new double[capacity];
        this.respawnDelaySeconds = respawnDelaySeconds;
        this.spawnCount = spawnCount;
        Reset();
    }

    public int Capacity => phases.Length;

    public int ActiveCount
    {
        get
        {
            var count = 0;
            for (var index = 0; index < phases.Length; ++index)
                if (phases[index] == CollectiblePhase.Active) ++count;
            return count;
        }
    }

    public void Reset()
    {
        for (var index = 0; index < phases.Length; ++index)
        {
            phases[index] = CollectiblePhase.Active;
            spawnIndices[index] = index % spawnCount;
            respawnRemaining[index] = 0;
        }
    }

    public bool Collect(int index)
    {
        ValidateIndex(index);
        if (phases[index] != CollectiblePhase.Active) return false;
        phases[index] = CollectiblePhase.Respawning;
        respawnRemaining[index] = respawnDelaySeconds;
        return true;
    }

    /// <summary>Advances all slots and returns the number completing respawn.</summary>
    public int Advance(double deltaSeconds, int selectionOrdinalBase)
    {
        if (!double.IsFinite(deltaSeconds) || deltaSeconds < 0)
            throw new ArgumentOutOfRangeException(nameof(deltaSeconds));
        var completed = 0;
        for (var index = 0; index < phases.Length; ++index)
        {
            if (phases[index] != CollectiblePhase.Respawning) continue;
            var remaining = Math.Max(0, respawnRemaining[index] - deltaSeconds);
            respawnRemaining[index] = remaining;
            if (remaining > 0) continue;
            phases[index] = CollectiblePhase.Active;
            spawnIndices[index] = CollectiblesHighLevel.SelectSpawnIndex(
                selectionOrdinalBase + completed, spawnCount);
            ++completed;
        }
        return completed;
    }

    public CollectiblePhase GetPhase(int index)
    {
        ValidateIndex(index);
        return phases[index];
    }

    public bool IsActive(int index)
    {
        ValidateIndex(index);
        return phases[index] == CollectiblePhase.Active;
    }

    public bool IsRespawning(int index)
    {
        ValidateIndex(index);
        return phases[index] == CollectiblePhase.Respawning;
    }

    public int GetSpawnIndex(int index)
    {
        ValidateIndex(index);
        return spawnIndices[index];
    }

    public double GetRespawnRemaining(int index)
    {
        ValidateIndex(index);
        return respawnRemaining[index];
    }

    private void ValidateIndex(int index)
    {
        if ((uint)index >= (uint)phases.Length)
            throw new ArgumentOutOfRangeException(nameof(index));
    }
}
