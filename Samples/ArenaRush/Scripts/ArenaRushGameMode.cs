// SPDX-License-Identifier: MIT

using SagaEngine.Scripting;
using SagaEngine.Scripting.Gameplay;

namespace ArenaRush;

public sealed class ArenaRushGameMode : SagaBehavior
{
    private static readonly ChallengeDefinition Definition = new(60, 100);
    private ChallengeState challenge = ChallengeHighLevel.Create(Definition);

    [SagaAction("StartChallenge")]
    public void StartChallenge()
    {
        challenge = ChallengeHighLevel.Arm(challenge);
    }

    public void EnterChallengeStartVolume()
    {
        challenge = ChallengeHighLevel.StartOnce(challenge);
    }

    [SagaAction("Restart")]
    public void Restart()
    {
        challenge = ChallengeHighLevel.Restart(challenge);
    }

    [SagaAction("Exit")]
    public void Exit()
    {
        // Product navigation/runtime interaction is intentionally deferred.
    }

    public void AddCoreProgress(int coreValue)
    {
        challenge = ChallengeHighLevel.AddProgress(challenge, coreValue);
    }
}
