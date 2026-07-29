// SPDX-License-Identifier: MPL-2.0

using SagaEngine.Scripting.Gameplay;

namespace GameplayContractsFixture;

internal static class ChallengeConsumer
{
    public static ChallengeState Create() =>
        ChallengeHighLevel.Create(new ChallengeDefinition(30, 5));
}
