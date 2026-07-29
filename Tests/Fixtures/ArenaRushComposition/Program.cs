// SPDX-License-Identifier: MPL-2.0

using System.Reflection;
using ArenaRush;
using SagaEngine.Scripting.Gameplay;

static void Require(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}

static ChallengeState State(ArenaRushGameMode mode)
{
    var field = typeof(ArenaRushGameMode).GetField(
        "challenge",
        BindingFlags.Instance | BindingFlags.NonPublic);
    return (ChallengeState)(field?.GetValue(mode)
        ?? throw new InvalidOperationException("Arena Rush challenge state is unavailable."));
}

var mode = new ArenaRushGameMode();
mode.InitializeForTests(60, 100, 10, 5, 2);
Require(State(mode).Phase == ChallengePhase.PreStart, "initial state is pre-start");

mode.StartChallenge();
var armed = State(mode);
Require(armed.Phase == ChallengePhase.Armed, "start action arms the challenge");
Require(armed.RemainingSeconds == 60, "start action does not advance the timer");

mode.EnterChallengeStartVolume();
var running = State(mode);
Require(running.Phase == ChallengePhase.Running, "start-volume entry starts an armed challenge");

mode.EnterChallengeStartVolume();
Require(State(mode) == running, "repeated start-volume entry is idempotent");

var unarmedMode = new ArenaRushGameMode();
unarmedMode.InitializeForTests(60, 100, 10, 5, 2);
unarmedMode.EnterChallengeStartVolume();
Require(
    State(unarmedMode).Phase == ChallengePhase.PreStart,
    "start-volume entry cannot bypass arming");

var timed = new ArenaRushGameMode();
timed.InitializeForTests(1, 100, 10, 5, 0.5);
timed.StartChallenge();
timed.EnterChallengeStartVolume();
timed.OnUpdate(1);
Require(State(timed).Phase == ChallengePhase.Timeout, "fixed delta reaches timeout");
timed.Restart();
Require(State(timed).Phase == ChallengePhase.PreStart, "restart resets terminal state");

var winning = new ArenaRushGameMode();
winning.InitializeForTests(60, 20, 20, 5, 0.5);
winning.StartChallenge();
winning.EnterChallengeStartVolume();
winning.CollectCore01();
Require(State(winning).Phase == ChallengePhase.Won, "authored core value reaches target");

Console.WriteLine("Arena Rush composition managed tests passed");
