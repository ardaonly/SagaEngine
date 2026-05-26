namespace SagaEngine.Scripting;

public abstract class SagaScript
{
    public ScriptContext Context { get; internal set; } = ScriptContext.CreateUnavailable();

    public virtual void OnCreate()
    {
    }

    public virtual void OnStart()
    {
    }

    public virtual void OnUpdate(float deltaTime)
    {
    }

    public virtual void OnDestroy()
    {
    }
}

public enum UiNamedActionEventType
{
    Click = 0,
    Submit = 1,
    TextChanged = 2,
    FocusGained = 3,
    FocusLost = 4,
}

public sealed class UiNamedActionContext
{
    public UiNamedActionContext(
        string actionId,
        string screenId,
        string elementId,
        UiNamedActionEventType eventType,
        string text)
    {
        ActionId = actionId;
        ScreenId = screenId;
        ElementId = elementId;
        EventType = eventType;
        Text = text;
    }

    public string ActionId { get; }
    public string ScreenId { get; }
    public string ElementId { get; }
    public UiNamedActionEventType EventType { get; }
    public string Text { get; }
}
