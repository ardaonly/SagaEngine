// SPDX-License-Identifier: MIT

using SagaEngine.Scripting;

namespace StarterArena;

[SagaBehaviorModule("behavior://starter-arena/door-controller")]
public sealed class DoorController : SagaBehavior
{
    private const float ClosedHeight = 1.0f;
    private const float OpenHeight = 3.0f;
    private const float Speed = 2.5f;
    private const string TargetOpenState = "door.target-open";
    private float _targetHeight;

    public override void OnStart()
    {
        _targetHeight = ClosedHeight;
        Context.State.TrySetBool(TargetOpenState, false);
        var transform = Context.Self.Transform;
        transform.Position = transform.Position with { Y = ClosedHeight };
    }

    [SagaAction("Interact")]
    public void OnInteract()
    {
        _targetHeight = _targetHeight == ClosedHeight ? OpenHeight : ClosedHeight;
        Context.State.TrySetBool(TargetOpenState, _targetHeight == OpenHeight);
    }

    public override void OnUpdate(float deltaTime)
    {
        var transform = Context.Self.Transform;
        var current = transform.Position;
        var distance = _targetHeight - current.Y;
        var step = Math.Clamp(distance, -Speed * deltaTime, Speed * deltaTime);
        transform.Position = current with { Y = current.Y + step };
    }
}
