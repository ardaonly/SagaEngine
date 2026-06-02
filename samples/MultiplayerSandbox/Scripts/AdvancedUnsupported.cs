namespace MultiplayerSandbox.Scripts;

using System.Linq;
using Saga;
using Saga.Gameplay.HighLevel;

[SagaBehavior(
    Id = "behavior://multiplayer-sandbox/advanced-unsupported",
    Level = SagaApiLevel.High,
    Domain = SagaApiDomain.Gameplay)]
public sealed class AdvancedUnsupported
{
    public void Run(Player player)
    {
        var visible = new[] { "alpha", "beta" }
            .Where(value => value.Length > 0)
            .Select(value => value.ToUpperInvariant());
    }
}
