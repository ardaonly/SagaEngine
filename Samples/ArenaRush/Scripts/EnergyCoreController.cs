// SPDX-License-Identifier: MIT

using SagaEngine.Scripting;
using SagaEngine.Scripting.Gameplay;

namespace ArenaRush;

public sealed class EnergyCoreController : SagaBehavior
{
    private static readonly CollectibleDefinition Definition = new(2, 5);
    private CollectibleState collectible = CollectiblesHighLevel.Create(Definition);

    [SagaAction("Interact")]
    public void Collect()
    {
        collectible = CollectiblesHighLevel.ScheduleRespawn(
            CollectiblesHighLevel.Deactivate(
                CollectiblesHighLevel.Collect(collectible)));
    }
}
