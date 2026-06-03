namespace Game;

using System;
using System.Linq;
using Saga;
using Saga.Gameplay.HighLevel;

[SagaBehavior(Id = "behavior://csharp-blocks/advanced-opaque/game-rules", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public sealed class GameRulesAdvancedOpaque
{
    public void Run(Player player, Door door)
    {
        var methodNames = typeof(GameRulesAdvancedOpaque)
            .GetMethods()
            .Select(method => method.Name);

        if (methodNames.Any(name => name.Equals("Run", StringComparison.Ordinal)))
        {
            Door.Open(door);
        }
    }
}
