namespace Game;

using Saga;
using Saga.Gameplay.HighLevel;

[SagaBehavior(Id = "behavior://csharp-blocks/unsupported/game-rules", Level = SagaApiLevel.Unsupported, Domain = SagaApiDomain.Gameplay)]
public sealed class GameRulesUnsupported
{
    public void Run(Player player, Door door)
    {
        while (Inventory.Has(player, "starter_key"))
        {
            Door.Open(door);
            break;
        }
    }
}
