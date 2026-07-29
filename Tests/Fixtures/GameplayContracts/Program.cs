// SPDX-License-Identifier: MPL-2.0

using SagaEngine.Scripting.Gameplay;
using SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics;
using GameplayContractsFixture;

static void Require(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}

var initial = ChallengeConsumer.Create();
Require(initial.Phase == ChallengePhase.PreStart, "create preserves declared initial phase");
var armed = ChallengeHighLevel.Arm(initial);
Require(armed.Phase == ChallengePhase.Armed, "arm transitions from pre-start");
Require(armed.RemainingSeconds == 30, "arming does not start time");
var running = ChallengeHighLevel.StartOnce(armed);
Require(running.Phase == ChallengePhase.Running, "only armed state starts");
Require(ChallengeHighLevel.AdvanceTime(running, 0) == running, "zero time is a no-op");
var advanced = ChallengeHighLevel.AdvanceTime(running, 0.5);
Require(advanced.RemainingSeconds == 29.5, "running time advances deterministically");
Require(
    ChallengeHighLevel.AdvanceTime(advanced, 100).Phase == ChallengePhase.Timeout,
    "time clamps at zero and times out");
Require(ChallengeHighLevel.StartOnce(initial) == initial, "pre-start cannot start");
Require(ChallengeHighLevel.StartOnce(running) == running, "repeated entry is idempotent");
var won = ChallengeHighLevel.AddProgress(running, 5);
Require(won.Phase == ChallengePhase.Won, "target progress wins");
Require(ChallengeHighLevel.AddProgress(won, 1) == won, "won state stops progression");
Require(ChallengeHighLevel.Timeout(won) == won, "won state cannot time out");
var timedOut = ChallengeHighLevel.Timeout(running);
Require(timedOut.Phase == ChallengePhase.Timeout, "running state times out");
Require(ChallengeHighLevel.AddProgress(timedOut, 1) == timedOut, "timeout stops progression");
Require(ChallengeHighLevel.Restart(timedOut) == initial, "restart restores initial state");

var collectible = CollectibleConsumer.Create();
var collected = CollectiblesHighLevel.Collect(collectible);
var inactive = CollectiblesHighLevel.Deactivate(collected);
var respawning = CollectiblesHighLevel.ScheduleRespawn(inactive);
Require(respawning.RespawnRemainingSeconds == 1.5, "respawn uses declared delay");
Require(
    CollectiblesHighLevel.CompleteRespawn(respawning, 0) == respawning,
    "respawn cannot complete before its timer reaches zero");
var ready = CollectiblesHighLevel.AdvanceRespawnTime(respawning, 1.5);
var active = CollectiblesHighLevel.CompleteRespawn(ready, 7);
Require(active.Phase == CollectiblePhase.Active, "respawn returns to active");
Require(active.SpawnIndex == 0, "single-spawn selection is deterministic");
Require(
    CollectiblesHighLevel.Reset(active) == collectible,
    "collectible reset restores its authored initial state");

var counters = CollectibleInstrumentation.Create(1);
counters = CollectibleInstrumentation.RecordCollect(counters, collected != collectible);
counters = CollectibleInstrumentation.RecordDeactivation(counters);
counters = CollectibleInstrumentation.RecordRespawnScheduled(counters);
counters = CollectibleInstrumentation.RecordRespawnCompleted(counters);
Require(counters.CollectAttempts == 1 && counters.CollectSuccesses == 1,
    "optional diagnostics count collect attempts and successes");
Require(counters.ActiveCount == 1 && counters.RespawnsCompleted == 1,
    "optional diagnostics preserve the gameplay lifecycle result");
Require(active == CollectiblesHighLevel.CompleteRespawn(
        CollectiblesHighLevel.AdvanceRespawnTime(
            CollectiblesHighLevel.ScheduleRespawn(
                CollectiblesHighLevel.Deactivate(
                    CollectiblesHighLevel.Collect(collectible))), 1.5), 7),
    "instrumentation does not alter gameplay output");

Console.WriteLine("Gameplay contract managed tests passed");
