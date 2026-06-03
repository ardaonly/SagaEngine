namespace Game;

using System.Linq;
using Saga;
using Saga.Gameplay.HighLevel;

[SagaBehavior(Id = "behavior://csharp-blocks/partial/game-rules", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public sealed class GameRulesPartial
{
    public void Run(Player player, Door door)
    {
        if (Inventory.Has(player, "starter_key"))
        {
            Door.Open(door);
        }

        var remainingValues = new[] { 1, 2, 3 }.Where(value => value > 1);
    }
}
