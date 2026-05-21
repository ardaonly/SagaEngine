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
