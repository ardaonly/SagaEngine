namespace Game;

using Saga;
using SagaEngine.Scripting;

[SagaBehavior(Id = "behavior://two-way/string-literal", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
[SagaScriptId("script://two-way/string-literal")]
public sealed class TwoWayRules : SagaScript
{
    [BlockCallable]
    [BlockName("Required Key")]
    [BlockCategory("Two Way")]
    [SharedPure]
    public string RequiredKey()
    {
        // preserve this comment
        return "starter_key";
    }
}
