// SPDX-License-Identifier: MPL-2.0

using SagaEngine.Scripting.Gameplay;
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
var ready = CollectiblesHighLevel.SetRespawnRemaining(respawning, 0);
var active = CollectiblesHighLevel.CompleteRespawn(ready, 7);
Require(active.Phase == CollectiblePhase.Active, "respawn returns to active");
Require(active.SpawnIndex == 0, "single-spawn selection is deterministic");

Console.WriteLine("Gameplay contract managed tests passed");
