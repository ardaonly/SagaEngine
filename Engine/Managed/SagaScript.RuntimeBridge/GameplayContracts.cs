// SPDX-License-Identifier: MPL-2.0

namespace SagaEngine.Scripting.Gameplay;

/// <summary>The deterministic lifecycle of a bounded high-level challenge.</summary>
public enum ChallengePhase
{
    PreStart = 0,
    Armed = 1,
    Running = 2,
    Suspended = 3,
    Won = 4,
    Timeout = 5,
}

/// <summary>Immutable authoring values used to create or restart a challenge.</summary>
public readonly record struct ChallengeDefinition(
    double DurationSeconds,
    int TargetProgress,
    ChallengePhase InitialPhase = ChallengePhase.PreStart);

/// <summary>Value-only challenge state. No engine clock or service is captured.</summary>
public readonly record struct ChallengeState(
    ChallengeDefinition Definition,
    ChallengePhase Phase,
    int Progress,
    double RemainingSeconds);

/// <summary>Stable terminal snapshot suitable for presentation by a consumer.</summary>
public readonly record struct ChallengeResult(
    ChallengePhase Phase,
    int Progress,
    int TargetProgress,
    double RemainingSeconds)
{
    public bool Won => Phase == ChallengePhase.Won;
}

/// <summary>Pure, deterministic high-level challenge transitions.</summary>
public static class ChallengeHighLevel
{
    public static ChallengeState Create(ChallengeDefinition definition)
    {
        var normalized = Normalize(definition);
        return new ChallengeState(
            normalized,
            normalized.InitialPhase,
            0,
            normalized.DurationSeconds);
    }

    public static ChallengeState Reset(ChallengeState state) => Create(state.Definition);

    public static ChallengeState Arm(ChallengeState state) =>
        state.Phase == ChallengePhase.PreStart
            ? state with { Phase = ChallengePhase.Armed }
            : state;

    public static ChallengeState StartOnce(ChallengeState state) =>
        state.Phase == ChallengePhase.Armed
            ? state with { Phase = ChallengePhase.Running }
            : state;

    public static ChallengeState Suspend(ChallengeState state) =>
        state.Phase == ChallengePhase.Running
            ? state with { Phase = ChallengePhase.Suspended }
            : state;

    public static ChallengeState Resume(ChallengeState state) =>
        state.Phase == ChallengePhase.Suspended
            ? state with { Phase = ChallengePhase.Running }
            : state;

    public static ChallengeState AddProgress(ChallengeState state, int amount)
    {
        if (state.Phase != ChallengePhase.Running || amount <= 0)
        {
            return state;
        }

        var progress = (int)Math.Min(
            state.Definition.TargetProgress,
            (long)state.Progress + amount);
        return state with
        {
            Progress = progress,
            Phase = progress >= state.Definition.TargetProgress
                ? ChallengePhase.Won
                : ChallengePhase.Running,
        };
    }

    public static ChallengeState Complete(ChallengeState state) =>
        state.Phase is ChallengePhase.Running or ChallengePhase.Suspended
            ? state with
            {
                Phase = ChallengePhase.Won,
                Progress = state.Definition.TargetProgress,
            }
            : state;

    public static ChallengeState Timeout(ChallengeState state) =>
        state.Phase is ChallengePhase.Running or ChallengePhase.Suspended
            ? state with { Phase = ChallengePhase.Timeout, RemainingSeconds = 0 }
            : state;

    public static ChallengeState SetRemainingTime(ChallengeState state, double remainingSeconds)
    {
        if (state.Phase != ChallengePhase.Running)
        {
            return state;
        }

        var remaining = Math.Clamp(remainingSeconds, 0, state.Definition.DurationSeconds);
        return remaining <= 0
            ? Timeout(state)
            : state with { RemainingSeconds = remaining };
    }

    /// <summary>Advances the deterministic challenge clock while it is running.</summary>
    public static ChallengeState AdvanceTime(ChallengeState state, double deltaSeconds)
    {
        if (!double.IsFinite(deltaSeconds) || deltaSeconds < 0)
        {
            throw new ArgumentOutOfRangeException(
                nameof(deltaSeconds),
                "Delta time must be finite and non-negative.");
        }
        if (state.Phase != ChallengePhase.Running || deltaSeconds == 0)
        {
            return state;
        }

        var remaining = Math.Max(0, state.RemainingSeconds - deltaSeconds);
        return remaining == 0
            ? Timeout(state)
            : state with { RemainingSeconds = remaining };
    }

    public static ChallengeState Restart(ChallengeState state) => Reset(state);

    public static ChallengeResult CreateResult(ChallengeState state) =>
        new(state.Phase, state.Progress, state.Definition.TargetProgress, state.RemainingSeconds);

    private static ChallengeDefinition Normalize(ChallengeDefinition definition)
    {
        if (!double.IsFinite(definition.DurationSeconds) || definition.DurationSeconds <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(definition), "Duration must be finite and positive.");
        }
        if (definition.TargetProgress <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(definition), "Target progress must be positive.");
        }
        if (definition.InitialPhase is ChallengePhase.Won or ChallengePhase.Timeout)
        {
            throw new ArgumentOutOfRangeException(nameof(definition), "Initial phase cannot be terminal.");
        }
        return definition;
    }
}

/// <summary>The deterministic lifecycle of a reusable collectible.</summary>
public enum CollectiblePhase
{
    Active = 0,
    Collected = 1,
    Inactive = 2,
    Respawning = 3,
}

/// <summary>Immutable authoring values for a generic collectible lifecycle.</summary>
public readonly record struct CollectibleDefinition(
    double RespawnDelaySeconds,
    int SpawnCount,
    int InitialSpawnIndex = 0);

/// <summary>Value-only collectible state with no entity, pool, or clock ownership.</summary>
public readonly record struct CollectibleState(
    CollectibleDefinition Definition,
    CollectiblePhase Phase,
    int SpawnIndex,
    double RespawnRemainingSeconds);

/// <summary>Pure, deterministic high-level collectible transitions.</summary>
public static class CollectiblesHighLevel
{
    public static CollectibleState Create(CollectibleDefinition definition)
    {
        var normalized = Normalize(definition);
        return new CollectibleState(
            normalized,
            CollectiblePhase.Active,
            normalized.InitialSpawnIndex,
            0);
    }

    public static CollectibleState Collect(CollectibleState state) =>
        state.Phase == CollectiblePhase.Active
            ? state with { Phase = CollectiblePhase.Collected }
            : state;

    public static CollectibleState Reset(CollectibleState state) =>
        Create(state.Definition);

    public static CollectibleState Deactivate(CollectibleState state) =>
        state.Phase == CollectiblePhase.Collected
            ? state with { Phase = CollectiblePhase.Inactive }
            : state;

    public static CollectibleState ScheduleRespawn(CollectibleState state) =>
        state.Phase == CollectiblePhase.Inactive
            ? state with
            {
                Phase = CollectiblePhase.Respawning,
                RespawnRemainingSeconds = state.Definition.RespawnDelaySeconds,
            }
            : state;

    public static CollectibleState SetRespawnRemaining(
        CollectibleState state,
        double remainingSeconds)
    {
        if (state.Phase != CollectiblePhase.Respawning)
        {
            return state;
        }
        return state with
        {
            RespawnRemainingSeconds = Math.Clamp(
                remainingSeconds,
                0,
                state.Definition.RespawnDelaySeconds),
        };
    }

    /// <summary>Advances a scheduled respawn without completing it implicitly.</summary>
    public static CollectibleState AdvanceRespawnTime(
        CollectibleState state,
        double deltaSeconds)
    {
        if (!double.IsFinite(deltaSeconds) || deltaSeconds < 0)
        {
            throw new ArgumentOutOfRangeException(
                nameof(deltaSeconds),
                "Delta time must be finite and non-negative.");
        }
        if (state.Phase != CollectiblePhase.Respawning || deltaSeconds == 0)
        {
            return state;
        }
        return state with
        {
            RespawnRemainingSeconds = Math.Max(
                0,
                state.RespawnRemainingSeconds - deltaSeconds),
        };
    }

    public static CollectibleState CompleteRespawn(
        CollectibleState state,
        int selectionOrdinal)
    {
        if (state.Phase != CollectiblePhase.Respawning ||
            state.RespawnRemainingSeconds > 0)
        {
            return state;
        }
        return state with
        {
            Phase = CollectiblePhase.Active,
            SpawnIndex = SelectSpawnIndex(selectionOrdinal, state.Definition.SpawnCount),
            RespawnRemainingSeconds = 0,
        };
    }

    public static int SelectSpawnIndex(int selectionOrdinal, int spawnCount)
    {
        if (spawnCount <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(spawnCount));
        }
        var remainder = selectionOrdinal % spawnCount;
        return remainder < 0 ? remainder + spawnCount : remainder;
    }

    private static CollectibleDefinition Normalize(CollectibleDefinition definition)
    {
        if (!double.IsFinite(definition.RespawnDelaySeconds) ||
            definition.RespawnDelaySeconds < 0)
        {
            throw new ArgumentOutOfRangeException(
                nameof(definition),
                "Respawn delay must be finite and non-negative.");
        }
        if (definition.SpawnCount <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(definition), "Spawn count must be positive.");
        }
        return definition with
        {
            InitialSpawnIndex = SelectSpawnIndex(
                definition.InitialSpawnIndex,
                definition.SpawnCount),
        };
    }
}
