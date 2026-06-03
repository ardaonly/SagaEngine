namespace Game;

using Saga;
using Saga.Gameplay.HighLevel;

[SagaBehavior(Id = "behavior://csharp-blocks/projectable/game-rules", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public sealed class GameRulesProjectable
{
    public void Run(Player player, Door door)
    {
        if (Inventory.Has(player, "starter_key") && true)
        {
            Door.Open(door);
        }
    }
}
