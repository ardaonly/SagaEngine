using Saga;

namespace Saga.BuiltinGameplayNodes;

[SagaLibrary(Id = "library://saga/gameplay/high", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public static class GameplayHighNodes
{
    [SagaNode(Id = "Gameplay.High.Inventory.Has", DisplayName = "Inventory Has", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
    public static bool InventoryHas(string entityId, string itemId) => false;

    [SagaNode(Id = "Gameplay.High.Door.Open", DisplayName = "Door Open", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
    public static void DoorOpen(string doorId) {}

    [SagaNode(Id = "Gameplay.High.Score.Add", DisplayName = "Score Add", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
    public static void ScoreAdd(string playerId, int amount) {}

    [SagaNode(Id = "Gameplay.High.Audio.PlayEvent", DisplayName = "Audio Play Event", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
    public static void AudioPlayEvent(string eventId) {}

    [SagaNode(Id = "Gameplay.High.Entity.SetTag", DisplayName = "Entity Set Tag", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
    public static void EntitySetTag(string entityId, string tag) {}
}

[SagaLibrary(Id = "library://saga/gameplay/low", Level = SagaApiLevel.Low, Domain = SagaApiDomain.Gameplay)]
public static class GameplayLowNodes
{
    [SagaNode(Id = "Gameplay.Low.Trigger.OnEnter", DisplayName = "Trigger On Enter", Level = SagaApiLevel.Low, Domain = SagaApiDomain.Gameplay)]
    public static void TriggerOnEnter(long triggerId, long entityId) {}

    [SagaNode(Id = "Gameplay.Low.Spawn.Entity", DisplayName = "Spawn Entity", Level = SagaApiLevel.Low, Domain = SagaApiDomain.Gameplay)]
    public static long SpawnEntity(string prefabId) => 0;

    [SagaNode(Id = "Gameplay.Low.Timer.Delay", DisplayName = "Timer Delay", Level = SagaApiLevel.Low, Domain = SagaApiDomain.Gameplay)]
    public static void TimerDelay(float seconds) {}

    [SagaNode(Id = "Gameplay.Low.Entity.DestroyOrDeactivate", DisplayName = "Destroy Or Deactivate Entity", Level = SagaApiLevel.Low, Domain = SagaApiDomain.Gameplay)]
    public static void DestroyOrDeactivate(long entityId) {}
}
