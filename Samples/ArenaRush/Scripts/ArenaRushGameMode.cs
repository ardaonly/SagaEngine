// SPDX-License-Identifier: MIT

using SagaEngine.Scripting;
using SagaEngine.Scripting.Gameplay;

namespace ArenaRush;

/// <summary>Compiled owner of all Arena Rush rules; native code supplies primitives only.</summary>
public sealed class ArenaRushGameMode : SagaBehavior
{
    private const float PlayerSpeed = 6.0F;
    private const int EnergyCoreScore = 10;
    private const double EnergyCoreRespawnSeconds = 2.0;
    private static readonly string[] CoreIds =
        ["core-01", "core-02", "core-03", "core-04", "core-05"];
    private static readonly string[] SpawnIds =
        ["spawn-01", "spawn-02", "spawn-03", "spawn-04", "spawn-05"];

    private ChallengeState challenge;
    private int coreScore;
    private int activeCollectibleCount;
    private EnergyCoreController.EnergyCoreState[] cores = [];
    private ScriptEntity player = null!;
    private ScriptEntity door = null!;
    private ScriptEntity[] coreEntities = [];
    private ScriptEntity[] spawnEntities = [];
    private Vector3Value initialPlayerPosition;
    private int spawnOrdinal;
    private bool initialized;

    public override void OnStart()
    {
        if (!Context.Configuration.TryGetDouble(
                "challengeDurationSeconds", out var duration) ||
            !Context.Configuration.TryGetInt32("targetProgress", out var target) ||
            !Context.Configuration.TryGetInt32(
                "activeCollectibleCount", out var authoredActiveCount))
            throw new InvalidOperationException(
                "Arena Rush authored instance configuration is incomplete.");
        Initialize(duration, target, EnergyCoreScore, authoredActiveCount,
            EnergyCoreRespawnSeconds);

        player = RequiredEntity("player");
        door = RequiredEntity("arena-door");
        coreEntities =
        [
            RequiredEntity("core-01"),
            RequiredEntity("core-02"),
            RequiredEntity("core-03"),
            RequiredEntity("core-04"),
            RequiredEntity("core-05"),
        ];
        spawnEntities =
        [
            RequiredEntity("spawn-01"),
            RequiredEntity("spawn-02"),
            RequiredEntity("spawn-03"),
            RequiredEntity("spawn-04"),
            RequiredEntity("spawn-05"),
        ];
        initialPlayerPosition = player.TryGetPosition(out var position)
            ? new Vector3Value(position.X, position.Y, position.Z)
            : default;
        ResetWorld();
        Publish();
    }

    public override void OnUpdate(float deltaTime)
    {
        if (!initialized || !float.IsFinite(deltaTime) || deltaTime < 0) return;

        if (Context.Input.IsPressed("Resume")) Resume();
        if (challenge.Phase == ChallengePhase.Running)
        {
            MovePlayer(deltaTime);
            challenge = ChallengeHighLevel.AdvanceTime(challenge, deltaTime);
            AdvanceRespawns(deltaTime);
        }
        Publish();
    }

    [SagaAction("StartChallenge")]
    public void StartChallenge()
    {
        if (!initialized) return;
        challenge = ChallengeHighLevel.Arm(challenge);
        if (challenge.Phase == ChallengePhase.Armed && door is not null)
            door.TrySetActive(false);
        Publish();
    }

    [SagaAction("EnterChallengeStartVolume")]
    public void EnterChallengeStartVolume()
    {
        if (!initialized) return;
        challenge = ChallengeHighLevel.StartOnce(challenge);
        Publish();
    }

    [SagaAction("FocusLost")]
    public void FocusLost()
    {
        if (!initialized) return;
        challenge = ChallengeHighLevel.Suspend(challenge);
        Publish();
    }

    [SagaAction("Resume")]
    public void Resume()
    {
        if (!initialized) return;
        challenge = ChallengeHighLevel.Resume(challenge);
        Publish();
    }

    [SagaAction("Restart")]
    public void Restart()
    {
        if (!initialized) return;
        challenge = ChallengeHighLevel.Restart(challenge);
        spawnOrdinal = 0;
        for (var index = 0; index < cores.Length; ++index)
            cores[index] = EnergyCoreController.Reset(cores[index]);
        ResetWorld();
        Publish();
    }

    [SagaAction("Exit")]
    public void Exit() => Context.Runtime.RequestExit();

    [SagaAction("CollectCore01")]
    public void CollectCore01() => CollectCore(0);
    [SagaAction("CollectCore02")]
    public void CollectCore02() => CollectCore(1);
    [SagaAction("CollectCore03")]
    public void CollectCore03() => CollectCore(2);
    [SagaAction("CollectCore04")]
    public void CollectCore04() => CollectCore(3);
    [SagaAction("CollectCore05")]
    public void CollectCore05() => CollectCore(4);

    internal void InitializeForTests(
        double duration, int target, int value, int activeCount, double respawn) =>
        Initialize(duration, target, value, activeCount, respawn);

    private void Initialize(
        double duration, int target, int value, int activeCount, double respawn)
    {
        if (value <= 0) throw new ArgumentOutOfRangeException(nameof(value));
        if (activeCount is < 1 or > 5)
            throw new ArgumentOutOfRangeException(nameof(activeCount));
        challenge = ChallengeHighLevel.Create(new ChallengeDefinition(duration, target));
        coreScore = value;
        activeCollectibleCount = activeCount;
        cores = new EnergyCoreController.EnergyCoreState[CoreIds.Length];
        for (var index = 0; index < cores.Length; ++index)
            cores[index] =
                EnergyCoreController.Create(respawn, SpawnIds.Length, index);
        initialized = true;
    }

    private void CollectCore(int index)
    {
        if (!initialized || challenge.Phase != ChallengePhase.Running ||
            index < 0 || index >= cores.Length ||
            cores[index].Value.Phase != CollectiblePhase.Active) return;
        cores[index] = EnergyCoreController.Collect(cores[index]);
        if (index < coreEntities.Length) coreEntities[index].TrySetActive(false);
        challenge = ChallengeHighLevel.AddProgress(challenge, coreScore);
        Publish();
    }

    private void AdvanceRespawns(double deltaSeconds)
    {
        for (var index = 0; index < cores.Length; ++index)
        {
            var (state, completed) =
                EnergyCoreController.Advance(cores[index], deltaSeconds, spawnOrdinal);
            cores[index] = state;
            if (!completed) continue;
            ++spawnOrdinal;
            if (index < coreEntities.Length && state.Value.SpawnIndex < spawnEntities.Length &&
                spawnEntities[state.Value.SpawnIndex].TryGetPosition(out var spawn))
            {
                coreEntities[index].TrySetPosition(spawn.X, spawn.Y, spawn.Z);
                coreEntities[index].TrySetActive(true);
            }
        }
    }

    private void MovePlayer(float deltaTime)
    {
        if (player is null || !player.TryGetPosition(out var position)) return;
        var x = (Context.Input.IsHeld("MoveRight") ? 1.0F : 0.0F) -
                (Context.Input.IsHeld("MoveLeft") ? 1.0F : 0.0F);
        var z = (Context.Input.IsHeld("MoveBack") ? 1.0F : 0.0F) -
                (Context.Input.IsHeld("MoveForward") ? 1.0F : 0.0F);
        if (x != 0 && z != 0)
        {
            x *= 0.70710678F;
            z *= 0.70710678F;
        }
        player.TrySetPosition(
            Math.Clamp(position.X + x * PlayerSpeed * deltaTime, -9.0F, 9.0F),
            position.Y,
            Math.Clamp(position.Z + z * PlayerSpeed * deltaTime, -9.0F, 9.0F));
    }

    private void ResetWorld()
    {
        if (door is not null) door.TrySetActive(true);
        if (player is not null)
        {
            player.TrySetPosition(
                initialPlayerPosition.X, initialPlayerPosition.Y, initialPlayerPosition.Z);
            player.TrySetActive(false);
        }
        for (var index = 0; index < coreEntities.Length; ++index)
            coreEntities[index].TrySetActive(index < activeCollectibleCount);
    }

    private void Publish()
    {
        if (!initialized) return;
        Context.State.TrySetString("challenge.phase", challenge.Phase.ToString());
        Context.State.TrySetInt32("challenge.score", challenge.Progress);
        Context.State.TrySetInt32("challenge.target", challenge.Definition.TargetProgress);
        Context.State.TrySetDouble("challenge.remaining-seconds", challenge.RemainingSeconds);
        Context.State.TrySetString("challenge.result-title", challenge.Phase switch
        {
            ChallengePhase.Won => "Arena Cleared",
            ChallengePhase.Timeout => "Time Expired",
            _ => string.Empty,
        });
        Context.State.TrySetBool(
            "challenge.player-enabled",
            challenge.Phase is ChallengePhase.Armed or ChallengePhase.Running);
        Context.State.TrySetBool(
            "challenge.door-open", challenge.Phase is ChallengePhase.Armed or ChallengePhase.Running);
        var activeCores = 0;
        var pendingRespawns = 0;
        foreach (var core in cores)
        {
            if (core.Value.Phase == CollectiblePhase.Active) ++activeCores;
            if (core.Value.Phase == CollectiblePhase.Respawning) ++pendingRespawns;
        }
        Context.State.TrySetInt32("challenge.active-cores", activeCores);
        Context.State.TrySetInt32(
            "challenge.pending-respawns", pendingRespawns);
        if (player is not null)
            player.TrySetActive(
                challenge.Phase is ChallengePhase.Armed or ChallengePhase.Running);
    }

    private ScriptEntity RequiredEntity(string stableId)
    {
        if (stableId == "player" &&
            Context.World.TryFindEntity("player", out var playerEntity))
            return playerEntity;
        if (stableId == "arena-door" &&
            Context.World.TryFindEntity("arena-door", out var doorEntity))
            return doorEntity;
        if (stableId == "core-01" &&
            Context.World.TryFindEntity("core-01", out var core01)) return core01;
        if (stableId == "core-02" &&
            Context.World.TryFindEntity("core-02", out var core02)) return core02;
        if (stableId == "core-03" &&
            Context.World.TryFindEntity("core-03", out var core03)) return core03;
        if (stableId == "core-04" &&
            Context.World.TryFindEntity("core-04", out var core04)) return core04;
        if (stableId == "core-05" &&
            Context.World.TryFindEntity("core-05", out var core05)) return core05;
        if (stableId == "spawn-01" &&
            Context.World.TryFindEntity("spawn-01", out var spawn01)) return spawn01;
        if (stableId == "spawn-02" &&
            Context.World.TryFindEntity("spawn-02", out var spawn02)) return spawn02;
        if (stableId == "spawn-03" &&
            Context.World.TryFindEntity("spawn-03", out var spawn03)) return spawn03;
        if (stableId == "spawn-04" &&
            Context.World.TryFindEntity("spawn-04", out var spawn04)) return spawn04;
        if (stableId == "spawn-05" &&
            Context.World.TryFindEntity("spawn-05", out var spawn05)) return spawn05;
        throw new InvalidOperationException($"Missing runtime entity: {stableId}");
    }
}
