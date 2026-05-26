using SagaEngine.Scripting;

namespace Game;

public sealed class PlayerScript : SagaScript
{
    public override void OnCreate()
    {
        Log("OnCreate");
    }

    public override void OnStart()
    {
        Log("OnStart");
    }

    public override void OnUpdate(float deltaTime)
    {
        Log("OnUpdate:" + deltaTime.ToString("R", System.Globalization.CultureInfo.InvariantCulture));
    }

    public override void OnDestroy()
    {
        Log("OnDestroy");
    }

    private static void Log(string line)
    {
        var path = Environment.GetEnvironmentVariable("SAGA_CSHARP_HOST_TEST_LOG");
        if (!string.IsNullOrWhiteSpace(path))
        {
            File.AppendAllText(path, line + Environment.NewLine);
        }
    }
}

public sealed class ThrowingScript : SagaScript
{
    public override void OnStart()
    {
        throw new InvalidOperationException("script boom");
    }
}

public sealed class NotASagaScript
{
}

public sealed class ContextProbeScript : SagaScript
{
    public override void OnCreate()
    {
        Context.Log.Info("Context:OnCreate:" + (Context is not null));
    }

    public override void OnStart()
    {
        Context.Log.Info("Context:OnStart:" + (Context is not null));
    }

    public override void OnUpdate(float deltaTime)
    {
        Context.Log.Info("Context:OnUpdate:" + (Context is not null) + ":" + Format(deltaTime));
    }

    public override void OnDestroy()
    {
        Context.Log.Info("Context:OnDestroy:" + (Context is not null));
    }

    private static string Format(float value)
    {
        return value.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
    }
}

public sealed class ContextLogScript : SagaScript
{
    public override void OnStart()
    {
        Context.Log.Info("ContextLog:OnStart");
    }
}

public sealed class WorldMutationScript : SagaScript
{
    private ScriptEntity? entity;

    public override void OnStart()
    {
        if (!Context.World.TryCreateEntity("spawned", out var created))
        {
            Context.Log.Error("WorldMutation:CreateFailed");
            return;
        }

        entity = created;
        if (!entity.TrySetPosition(1.0F, 2.0F, 3.0F))
        {
            Context.Log.Error("WorldMutation:SetFailed");
            return;
        }

        if (entity.TryGetPosition(out var position))
        {
            Context.Log.Info("WorldMutation:Start:" + Format(position.X));
        }
    }

    public override void OnUpdate(float deltaTime)
    {
        if (entity is null || !entity.TryGetPosition(out var position))
        {
            Context.Log.Error("WorldMutation:UpdateReadFailed");
            return;
        }

        entity.TrySetPosition(position.X + deltaTime, position.Y, position.Z);
        Context.Log.Info("WorldMutation:Update:" + Format(deltaTime));
    }

    private static string Format(float value)
    {
        return value.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
    }
}

public sealed class SelfMutationScript : SagaScript
{
    public override void OnStart()
    {
        if (Context.Self.TrySetPosition(4.0F, 5.0F, 6.0F) &&
            Context.Self.TryGetPosition(out var position))
        {
            Context.Log.Info("SelfMutation:Start:" + Format(position.X));
        }
    }

    public override void OnUpdate(float deltaTime)
    {
        if (!Context.Self.TryGetPosition(out var position))
        {
            Context.Log.Error("SelfMutation:ReadFailed");
            return;
        }

        Context.Self.TrySetPosition(position.X + deltaTime, position.Y, position.Z);
        Context.Log.Info("SelfMutation:Update:" + Format(deltaTime));
    }

    private static string Format(float value)
    {
        return value.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
    }
}

public sealed class MissingCapabilityScript : SagaScript
{
    public override void OnStart()
    {
        var created = Context.World.TryCreateEntity("denied", out _);
        Context.Log.Info("MissingCapability:CreateEntity:" + created);
    }
}

public sealed class InvalidSelfScript : SagaScript
{
    public override void OnStart()
    {
        var updated = Context.Self.TrySetPosition(1.0F, 2.0F, 3.0F);
        Context.Log.Info("InvalidSelf:SetPosition:" + updated);
    }
}

public sealed class UiNamedActionScript : SagaScript
{
    public void RecordUiAction(UiNamedActionContext action)
    {
        Context.Log.Info(
            "UiAction:" +
            action.ActionId + ":" +
            action.ScreenId + ":" +
            action.ElementId + ":" +
            action.EventType + ":" +
            action.Text);
    }

    public bool BoolUiAction(UiNamedActionContext action)
    {
        Context.Log.Info("UiBool:" + action.ActionId + ":" + action.Text);
        return true;
    }

    public bool RejectUiAction(UiNamedActionContext action)
    {
        Context.Log.Info("UiReject:" + action.ActionId);
        return false;
    }

    public void ThrowUiAction(UiNamedActionContext action)
    {
        throw new InvalidOperationException("ui action boom: " + action.ActionId);
    }

    public void InvalidUiAction()
    {
    }
}
