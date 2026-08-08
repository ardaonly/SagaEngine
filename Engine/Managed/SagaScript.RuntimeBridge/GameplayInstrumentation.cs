// SPDX-License-Identifier: MPL-2.0

namespace SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics;

/// <summary>
/// Immutable optional counters for validating a collectible integration.
/// Gameplay transitions remain owned by CollectiblesHighLevel.
/// </summary>
public readonly record struct CollectibleInstrumentationState(
    int CollectAttempts,
    int CollectSuccesses,
    int Deactivations,
    int RespawnsScheduled,
    int RespawnsCompleted,
    int ActiveCount);

public static class CollectibleInstrumentation
{
    public static CollectibleInstrumentationState Create(int activeCount)
    {
        if (activeCount < 0) throw new ArgumentOutOfRangeException(nameof(activeCount));
        return new CollectibleInstrumentationState(0, 0, 0, 0, 0, activeCount);
    }

    public static CollectibleInstrumentationState RecordCollect(
        CollectibleInstrumentationState state, bool succeeded) =>
        state with
        {
            CollectAttempts = checked(state.CollectAttempts + 1),
            CollectSuccesses = checked(state.CollectSuccesses + (succeeded ? 1 : 0)),
        };

    public static CollectibleInstrumentationState RecordDeactivation(
        CollectibleInstrumentationState state) =>
        state with
        {
            Deactivations = checked(state.Deactivations + 1),
            ActiveCount = Math.Max(0, state.ActiveCount - 1),
        };

    public static CollectibleInstrumentationState RecordRespawnScheduled(
        CollectibleInstrumentationState state) =>
        state with { RespawnsScheduled = checked(state.RespawnsScheduled + 1) };

    public static CollectibleInstrumentationState RecordRespawnCompleted(
        CollectibleInstrumentationState state) =>
        state with
        {
            RespawnsCompleted = checked(state.RespawnsCompleted + 1),
            ActiveCount = checked(state.ActiveCount + 1),
        };
}

/// <summary>Availability of an optional benchmark counter.</summary>
public enum MeasurementCounterAvailability
{
    Available,
    Unavailable,
}

/// <summary>Stable summary emitted after a measured run.</summary>
public readonly record struct CollectibleMeasurementSnapshot(
    long UpdateSamples,
    long LifecycleTransitions,
    long TotalUpdateNanoseconds,
    MeasurementCounterAvailability AllocationCounter,
    MeasurementCounterAvailability MemoryCounter);

/// <summary>
/// Preallocated measurement recorder. Runtime callers provide steady-clock
/// update durations; unavailable counters are never estimated.
/// </summary>
public sealed class CollectibleMeasurementRecorder
{
    private readonly long[] rawUpdateNanoseconds;
    private int sampleCount;
    private long lifecycleTransitions;
    private long totalUpdateNanoseconds;

    public CollectibleMeasurementRecorder(int sampleCapacity)
    {
        if (sampleCapacity <= 0)
            throw new ArgumentOutOfRangeException(nameof(sampleCapacity));
        rawUpdateNanoseconds = new long[sampleCapacity];
    }

    public int Capacity => rawUpdateNanoseconds.Length;
    public int SampleCount => sampleCount;

    public void RecordUpdate(long steadyClockNanoseconds)
    {
        if (steadyClockNanoseconds < 0)
            throw new ArgumentOutOfRangeException(nameof(steadyClockNanoseconds));
        if (sampleCount >= rawUpdateNanoseconds.Length)
            throw new InvalidOperationException("Measurement sample capacity was exhausted.");
        rawUpdateNanoseconds[sampleCount++] = steadyClockNanoseconds;
        totalUpdateNanoseconds = checked(totalUpdateNanoseconds + steadyClockNanoseconds);
    }

    public void RecordLifecycleTransitions(int count)
    {
        if (count < 0) throw new ArgumentOutOfRangeException(nameof(count));
        lifecycleTransitions = checked(lifecycleTransitions + count);
    }

    public long GetRawUpdateNanoseconds(int index)
    {
        if ((uint)index >= (uint)sampleCount)
            throw new ArgumentOutOfRangeException(nameof(index));
        return rawUpdateNanoseconds[index];
    }

    public CollectibleMeasurementSnapshot Snapshot() => new(
        sampleCount,
        lifecycleTransitions,
        totalUpdateNanoseconds,
        MeasurementCounterAvailability.Unavailable,
        MeasurementCounterAvailability.Unavailable);
}
