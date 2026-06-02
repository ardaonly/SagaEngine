namespace MultiplayerSandbox.Scripts;

using Saga;
using Saga.Gameplay.LowLevel;

[SagaBehavior(
    Id = "behavior://multiplayer-sandbox/door-state",
    Level = SagaApiLevel.Low,
    Domain = SagaApiDomain.Gameplay)]
public sealed class DoorStateLogic
{
    public void ApplyDoorUnlock(EntityId door, InventorySlot slot)
    {
        if (GameplayOps.QueryInventorySlot(slot, "key"))
        {
            GameplayOps.SetDoorState(door, "open");
            GameplayOps.ApplyItemDelta(slot, "key", -1);
            GameplayOps.EmitGameplayEvent("door.opened");
        }
    }
}
