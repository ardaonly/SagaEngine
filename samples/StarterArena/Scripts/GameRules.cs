namespace StarterArena.Scripts;

using SagaEngine.Scripting;

[SagaScriptId("script://starter-arena/game-rules")]
public sealed class GameRules : SagaScript
{
    [BlockCallable]
    [BlockName("Add Pickup Score")]
    [BlockCategory("StarterArena")]
    [SharedPure]
    public int AddPickupScore(int currentScore, int pickupValue)
    {
        return currentScore + pickupValue;
    }
}
