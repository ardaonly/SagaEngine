namespace MultiplayerSandbox.Scripts;

using Saga;
using Saga.Gameplay.HighLevel;

[SagaBehavior(
    Id = "behavior://multiplayer-sandbox/door-interact",
    Level = SagaApiLevel.High,
    Domain = SagaApiDomain.Gameplay)]
public sealed class DoorLogic
{
    public void OnInteract(Player player, Door door)
    {
        if (Inventory.Has(player, "key"))
        {
            Door.Open(door);
            Quest.Complete(player, "open_first_door");
        }
    }
}
