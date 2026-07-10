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
