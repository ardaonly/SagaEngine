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
