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
unarmedMode.EnterChallengeStartVolume();
Require(
    State(unarmedMode).Phase == ChallengePhase.PreStart,
    "start-volume entry cannot bypass arming");

Console.WriteLine("Arena Rush composition managed tests passed");
