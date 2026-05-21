using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace SagaEngine.Scripting;

public readonly struct ScriptVector3
{
    public ScriptVector3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public float X { get; }
    public float Y { get; }
    public float Z { get; }
}

public sealed class ScriptContext
{
    internal ScriptContext(NativeScriptHost? host, long selfEntityHandle)
    {
        Host = host;
        World = new ScriptWorld(host);
        Self = new ScriptEntity(host, selfEntityHandle);
        Log = new ScriptLogger(host);
    }

    public ScriptWorld World { get; }
    public ScriptEntity Self { get; }
    public ScriptLogger Log { get; }

    internal NativeScriptHost? Host { get; }

    internal static ScriptContext CreateUnavailable()
    {
        return new ScriptContext(null, 0);
    }
}

public sealed class ScriptWorld
{
    internal ScriptWorld(NativeScriptHost? host)
    {
        Host = host;
    }

    public bool TryCreateEntity(string name, out ScriptEntity entity)
    {
        entity = ScriptEntity.Invalid;
        if (Host is null)
        {
            return false;
        }

        if (!Host.TryCreateEntity(name, out var handle))
        {
            return false;
        }

        entity = new ScriptEntity(Host, handle);
        return true;
    }

    private NativeScriptHost? Host { get; }
}

public sealed class ScriptEntity
{
    internal ScriptEntity(NativeScriptHost? host, long handle)
    {
        Host = host;
        Handle = handle;
    }

    public bool IsValid => Host is not null && Handle != 0;

    public bool TrySetPosition(float x, float y, float z)
    {
        return Host is not null && Host.TrySetPosition(Handle, x, y, z);
    }

    public bool TryGetPosition(out ScriptVector3 position)
    {
        position = default;
        if (Host is null)
        {
            return false;
        }

        return Host.TryGetPosition(Handle, out position);
    }

    internal static ScriptEntity Invalid => new(null, 0);

    private NativeScriptHost? Host { get; }
    private long Handle { get; }
}

public sealed class ScriptLogger
{
    internal ScriptLogger(NativeScriptHost? host)
    {
        Host = host;
    }

    public void Info(string message)
    {
        Host?.Log(ScriptLogLevel.Info, message);
    }

    public void Warn(string message)
    {
        Host?.Log(ScriptLogLevel.Warn, message);
    }

    public void Error(string message)
    {
        Host?.Log(ScriptLogLevel.Error, message);
    }

    private NativeScriptHost? Host { get; }
}

internal enum ScriptLogLevel
{
    Info = 0,
    Warn = 1,
    Error = 2,
}

internal static class NativeCallbackStatus
{
    public const int Ok = 0;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct NativeCallbackTable
{
    public int Version;
    public delegate* unmanaged[Cdecl]<long, int, byte*, int> Log;
    public delegate* unmanaged[Cdecl]<long, byte*, long*, int> CreateEntity;
    public delegate* unmanaged[Cdecl]<long, long, float, float, float, int> SetPosition;
    public delegate* unmanaged[Cdecl]<long, long, float*, float*, float*, int> GetPosition;
}

internal unsafe sealed class NativeScriptHost
{
    public NativeScriptHost(long contextHandle, NativeCallbackTable callbacks)
    {
        ContextHandle = contextHandle;
        Callbacks = callbacks;
    }

    public bool TryCreateEntity(string name, out long entityHandle)
    {
        entityHandle = 0;
        if (Callbacks.CreateEntity == null)
        {
            return false;
        }

        var handle = 0L;
        var buffer = ToNullTerminatedUtf8(name);
        fixed (byte* namePointer = buffer)
        {
            var status = Callbacks.CreateEntity(ContextHandle, namePointer, &handle);
            entityHandle = handle;
            return status == NativeCallbackStatus.Ok && entityHandle != 0;
        }
    }

    public bool TrySetPosition(long entityHandle, float x, float y, float z)
    {
        return Callbacks.SetPosition != null &&
            Callbacks.SetPosition(ContextHandle, entityHandle, x, y, z) ==
                NativeCallbackStatus.Ok;
    }

    public bool TryGetPosition(long entityHandle, out ScriptVector3 position)
    {
        position = default;
        if (Callbacks.GetPosition == null)
        {
            return false;
        }

        var x = 0.0F;
        var y = 0.0F;
        var z = 0.0F;
        var status = Callbacks.GetPosition(ContextHandle, entityHandle, &x, &y, &z);
        if (status != NativeCallbackStatus.Ok)
        {
            return false;
        }

        position = new ScriptVector3(x, y, z);
        return true;
    }

    public void Log(ScriptLogLevel level, string message)
    {
        if (Callbacks.Log == null)
        {
            return;
        }

        var buffer = ToNullTerminatedUtf8(message);
        fixed (byte* messagePointer = buffer)
        {
            _ = Callbacks.Log(ContextHandle, (int)level, messagePointer);
        }
    }

    private static byte[] ToNullTerminatedUtf8(string? value)
    {
        var bytes = Encoding.UTF8.GetBytes(value ?? string.Empty);
        var buffer = new byte[bytes.Length + 1];
        bytes.CopyTo(buffer, 0);
        return buffer;
    }

    private long ContextHandle { get; }
    private NativeCallbackTable Callbacks { get; }
}
