// SPDX-License-Identifier: MPL-2.0
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

/// <summary>High-level project behavior base; runtime loading remains owned by SagaScript.</summary>
public abstract class SagaBehavior : SagaScript
{
}

/// <summary>
/// Declares the stable logical identity of a project behavior module. The value
/// is authored source metadata and must not be derived from the CLR type name or
/// source path.
/// </summary>
[AttributeUsage(AttributeTargets.Class, AllowMultiple = false, Inherited = false)]
public sealed class SagaBehaviorModuleAttribute : Attribute
{
    public SagaBehaviorModuleAttribute(string moduleId)
    {
        ModuleId = moduleId;
    }

    public string ModuleId { get; }
}

/// <summary>Declares a logical project action handled by a behavior method.</summary>
[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
public sealed class SagaActionAttribute : Attribute
{
    public SagaActionAttribute(string actionId)
    {
        ActionId = actionId;
    }

    public string ActionId { get; }
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
