namespace StarterArena.Scripts;

using SagaEngine.Scripting;

[SagaScriptId("script://starter-arena/game-rules")]
public sealed class GameRules : SagaScript
{
    public override void OnCreate()
    {
        Context.Log.Info("StarterArena.GameRules.OnCreate");
    }

    public override void OnStart()
    {
        Context.Log.Info("StarterArena.GameRules.OnStart");
    }

    public override void OnUpdate(float deltaTime)
    {
        Context.Log.Info("StarterArena.GameRules.OnUpdate");
        if (!Context.State.IsAvailable ||
            !Context.State.TryGetBool("starter.pickup.0.reachable", out var reachable) ||
            !Context.State.TryGetBool("starter.pickup.0.collected", out var collected) ||
            !reachable || collected)
        {
            return;
        }

        if (!Context.State.TrySetBool("starter.pickup.0.collected", true) ||
            !Context.State.TryAddInt32("starter.score", 10, out _) ||
            !Context.State.TrySetString("starter.player.state", "powered"))
        {
            Context.Log.Error("StarterArena.GameRules.GameplayMutationFailed");
            return;
        }
        Context.Log.Info("StarterArena.GameRules.GameplayMutationPassed");
    }

    public override void OnDestroy()
    {
        Context.Log.Info("StarterArena.GameRules.OnDestroy");
    }

    [BlockCallable]
    [BlockName("Add Pickup Score")]
    [BlockCategory("StarterArena")]
    [SharedPure]
    public int AddPickupScore(int currentScore, int pickupValue)
    {
        return currentScore + pickupValue;
    }
}
