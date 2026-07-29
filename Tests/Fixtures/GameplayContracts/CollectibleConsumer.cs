// SPDX-License-Identifier: MPL-2.0

using SagaEngine.Scripting.Gameplay;

namespace GameplayContractsFixture;

internal static class CollectibleConsumer
{
    public static CollectibleState Create() =>
        CollectiblesHighLevel.Create(new CollectibleDefinition(1.5, 1));
}
