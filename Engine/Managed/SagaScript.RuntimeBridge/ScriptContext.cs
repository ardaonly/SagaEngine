// SPDX-License-Identifier: MPL-2.0
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
        Configuration = new ScriptConfigurationPort(host);
        Input = new ScriptInputPort(host);
        Runtime = new ScriptRuntimePort(host);
        World = new ScriptWorld(host);
        State = new ScriptStatePort(host);
        Self = new SelfFacade(new ScriptEntity(host, selfEntityHandle));
        Log = new ScriptLogger(host);
    }

    public ScriptWorld World { get; }
    public ScriptStatePort State { get; }
    public ScriptConfigurationPort Configuration { get; }
    public ScriptInputPort Input { get; }
    public ScriptRuntimePort Runtime { get; }
    public SelfFacade Self { get; }
    public ScriptLogger Log { get; }

    internal NativeScriptHost? Host { get; }

    internal static ScriptContext CreateUnavailable()
    {
        return new ScriptContext(null, 0);
    }
}

public enum LogicalInputState
{
    Pressed = 0,
    Held = 1,
    Released = 2,
}

/// <summary>Read-only, per-instance authored configuration.</summary>
public sealed class ScriptConfigurationPort
{
    internal ScriptConfigurationPort(NativeScriptHost? host) { Host = host; }

    public bool TryGetInt32(string key, out int value)
    {
        value = 0;
        return Host is not null && Host.TryGetConfigurationInt32(key, out value);
    }

    public bool TryGetDouble(string key, out double value)
    {
        value = 0;
        return Host is not null && Host.TryGetConfigurationDouble(key, out value);
    }

    public bool TryGetString(string key, out string value)
    {
        value = string.Empty;
        return Host is not null && Host.TryGetConfigurationString(key, out value);
    }

    private NativeScriptHost? Host { get; }
}

/// <summary>Logical input snapshot selected by the verified input artifact.</summary>
public sealed class ScriptInputPort
{
    internal ScriptInputPort(NativeScriptHost? host) { Host = host; }

    public bool IsPressed(string actionId) => Query(actionId, LogicalInputState.Pressed);
    public bool IsHeld(string actionId) => Query(actionId, LogicalInputState.Held);
    public bool IsReleased(string actionId) => Query(actionId, LogicalInputState.Released);

    public bool TryGetValue(string actionId, out double value)
    {
        value = 0;
        return Host is not null && Host.TryGetInputValue(actionId, out value);
    }

    private bool Query(string actionId, LogicalInputState state) =>
        Host is not null && Host.QueryInput(actionId, state);

    private NativeScriptHost? Host { get; }
}

/// <summary>Generic process-level runtime requests; gameplay policy remains managed.</summary>
public sealed class ScriptRuntimePort
{
    internal ScriptRuntimePort(NativeScriptHost? host) { Host = host; }
    public bool RequestExit() => Host is not null && Host.RequestExit();
    private NativeScriptHost? Host { get; }
}

public sealed class ScriptStatePort
{
    internal ScriptStatePort(NativeScriptHost? host) { Host = host; }

    public bool IsAvailable => Host is not null && Host.StatePortAvailable();

    public bool TryGetBool(string key, out bool value)
    {
        value = false;
        return Host is not null && Host.TryGetBool(key, out value);
    }

    public bool TryGetInt32(string key, out int value)
    {
        value = 0;
        return Host is not null && Host.TryGetInt32(key, out value);
    }

    public bool TryGetDouble(string key, out double value)
    {
        value = 0;
        return Host is not null && Host.TryGetDouble(key, out value);
    }

    public bool TryGetString(string key, out string value)
    {
        value = string.Empty;
        return Host is not null && Host.TryGetString(key, out value);
    }

    public bool TryAddInt32(string key, int delta, out int value)
    {
        value = 0;
        return Host is not null && Host.TryAddInt32(key, delta, out value);
    }

    public bool TrySetBool(string key, bool value) =>
        Host is not null && Host.TrySetBool(key, value);

    public bool TrySetInt32(string key, int value) =>
        Host is not null && Host.TrySetInt32(key, value);

    public bool TrySetDouble(string key, double value) =>
        Host is not null && Host.TrySetDouble(key, value);

    public bool TrySetString(string key, string value) =>
        Host is not null && Host.TrySetString(key, value);

    private NativeScriptHost? Host { get; }
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

    public bool TryFindEntity(string stableEntityId, out ScriptEntity entity)
    {
        entity = ScriptEntity.Invalid;
        if (Host is null || !Host.TryFindEntity(stableEntityId, out var handle))
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

    public bool TrySetActive(bool active) =>
        Host is not null && Host.TrySetEntityActive(Handle, active);

    public bool TryGetActive(out bool active)
    {
        active = false;
        return Host is not null && Host.TryGetEntityActive(Handle, out active);
    }

    internal static ScriptEntity Invalid => new(null, 0);

    private NativeScriptHost? Host { get; }
    private long Handle { get; }
}

/// <summary>Project-facing facade for the behavior's current entity.</summary>
public sealed class SelfFacade
{
    internal SelfFacade(ScriptEntity entity)
    {
        Entity = entity;
        Transform = new TransformFacade(entity);
    }

    public bool IsValid => Entity.IsValid;
    public TransformFacade Transform { get; }
    public bool TrySetPosition(float x, float y, float z) => Entity.TrySetPosition(x, y, z);
    public bool TryGetPosition(out ScriptVector3 position) => Entity.TryGetPosition(out position);

    private ScriptEntity Entity { get; }
}

/// <summary>Value used by the high-level transform facade.</summary>
public readonly record struct Vector3Value(float X, float Y, float Z);

/// <summary>Reads and writes the current entity position through the existing host callbacks.</summary>
public sealed class TransformFacade
{
    internal TransformFacade(ScriptEntity entity)
    {
        Entity = entity;
    }

    public Vector3Value Position
    {
        get
        {
            return Entity.TryGetPosition(out var value)
                ? new Vector3Value(value.X, value.Y, value.Z)
                : default;
        }
        set
        {
            Entity.TrySetPosition(value.X, value.Y, value.Z);
        }
    }

    private ScriptEntity Entity { get; }
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
    public delegate* unmanaged[Cdecl]<long, byte*, int*, int> GetStateBool;
    public delegate* unmanaged[Cdecl]<long, byte*, int, int*, int> AddStateInt32;
    public delegate* unmanaged[Cdecl]<long, byte*, int, int> SetStateBool;
    public delegate* unmanaged[Cdecl]<long, byte*, byte*, int> SetStateString;
    public delegate* unmanaged[Cdecl]<long, int> StatePortAvailable;
    public delegate* unmanaged[Cdecl]<long, byte*, int*, int> GetConfigurationInt32;
    public delegate* unmanaged[Cdecl]<long, byte*, double*, int> GetConfigurationDouble;
    public delegate* unmanaged[Cdecl]<long, byte*, byte*, int, int> GetConfigurationString;
    public delegate* unmanaged[Cdecl]<long, byte*, int, int*, int> QueryInput;
    public delegate* unmanaged[Cdecl]<long, byte*, double*, int> GetInputValue;
    public delegate* unmanaged[Cdecl]<long, byte*, long*, int> FindEntity;
    public delegate* unmanaged[Cdecl]<long, long, int, int> SetEntityActive;
    public delegate* unmanaged[Cdecl]<long, long, int*, int> GetEntityActive;
    public delegate* unmanaged[Cdecl]<long, byte*, int*, int> GetStateInt32;
    public delegate* unmanaged[Cdecl]<long, byte*, double*, int> GetStateDouble;
    public delegate* unmanaged[Cdecl]<long, byte*, byte*, int, int> GetStateString;
    public delegate* unmanaged[Cdecl]<long, byte*, int, int> SetStateInt32;
    public delegate* unmanaged[Cdecl]<long, byte*, double, int> SetStateDouble;
    public delegate* unmanaged[Cdecl]<long, int> RequestExit;
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

    public bool TryFindEntity(string stableEntityId, out long entityHandle)
    {
        entityHandle = 0;
        if (Callbacks.FindEntity == null) return false;
        var buffer = ToNullTerminatedUtf8(stableEntityId);
        var handle = 0L;
        fixed (byte* pointer = buffer)
        {
            var status = Callbacks.FindEntity(ContextHandle, pointer, &handle);
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

    public bool TryGetBool(string key, out bool value)
    {
        value = false;
        if (Callbacks.GetStateBool == null) return false;
        var buffer = ToNullTerminatedUtf8(key);
        var raw = 0;
        fixed (byte* pointer = buffer)
        {
            var status = Callbacks.GetStateBool(ContextHandle, pointer, &raw);
            value = raw != 0;
            return status == NativeCallbackStatus.Ok;
        }
    }

    public bool TryGetInt32(string key, out int value) =>
        TryGetInt32(Callbacks.GetStateInt32, key, out value);

    public bool TryGetDouble(string key, out double value) =>
        TryGetDouble(Callbacks.GetStateDouble, key, out value);

    public bool TryGetString(string key, out string value) =>
        TryGetString(Callbacks.GetStateString, key, out value);

    public bool StatePortAvailable() =>
        Callbacks.StatePortAvailable != null && Callbacks.StatePortAvailable(ContextHandle) != 0;

    public bool TryAddInt32(string key, int delta, out int value)
    {
        value = 0;
        if (Callbacks.AddStateInt32 == null) return false;
        var buffer = ToNullTerminatedUtf8(key);
        var updated = 0;
        fixed (byte* pointer = buffer)
        {
            var status = Callbacks.AddStateInt32(ContextHandle, pointer, delta, &updated);
            value = updated;
            return status == NativeCallbackStatus.Ok;
        }
    }

    public bool TrySetBool(string key, bool value)
    {
        if (Callbacks.SetStateBool == null) return false;
        var buffer = ToNullTerminatedUtf8(key);
        fixed (byte* pointer = buffer)
        {
            return Callbacks.SetStateBool(ContextHandle, pointer, value ? 1 : 0) ==
                NativeCallbackStatus.Ok;
        }
    }

    public bool TrySetInt32(string key, int value)
    {
        if (Callbacks.SetStateInt32 == null) return false;
        var buffer = ToNullTerminatedUtf8(key);
        fixed (byte* pointer = buffer)
        {
            return Callbacks.SetStateInt32(ContextHandle, pointer, value) ==
                NativeCallbackStatus.Ok;
        }
    }

    public bool TrySetDouble(string key, double value)
    {
        if (Callbacks.SetStateDouble == null) return false;
        var buffer = ToNullTerminatedUtf8(key);
        fixed (byte* pointer = buffer)
        {
            return Callbacks.SetStateDouble(ContextHandle, pointer, value) ==
                NativeCallbackStatus.Ok;
        }
    }

    public bool TrySetString(string key, string value)
    {
        if (Callbacks.SetStateString == null) return false;
        var keyBuffer = ToNullTerminatedUtf8(key);
        var valueBuffer = ToNullTerminatedUtf8(value);
        fixed (byte* keyPointer = keyBuffer)
        fixed (byte* valuePointer = valueBuffer)
        {
            return Callbacks.SetStateString(ContextHandle, keyPointer, valuePointer) ==
                NativeCallbackStatus.Ok;
        }
    }

    public bool TryGetConfigurationInt32(string key, out int value) =>
        TryGetInt32(Callbacks.GetConfigurationInt32, key, out value);

    public bool TryGetConfigurationDouble(string key, out double value) =>
        TryGetDouble(Callbacks.GetConfigurationDouble, key, out value);

    public bool TryGetConfigurationString(string key, out string value) =>
        TryGetString(Callbacks.GetConfigurationString, key, out value);

    public bool QueryInput(string actionId, LogicalInputState state)
    {
        if (Callbacks.QueryInput == null) return false;
        var buffer = ToNullTerminatedUtf8(actionId);
        var result = 0;
        fixed (byte* pointer = buffer)
        {
            return Callbacks.QueryInput(
                ContextHandle, pointer, (int)state, &result) == NativeCallbackStatus.Ok &&
                result != 0;
        }
    }

    public bool TryGetInputValue(string actionId, out double value) =>
        TryGetDouble(Callbacks.GetInputValue, actionId, out value);

    public bool TrySetEntityActive(long entityHandle, bool active) =>
        Callbacks.SetEntityActive != null &&
        Callbacks.SetEntityActive(ContextHandle, entityHandle, active ? 1 : 0) ==
            NativeCallbackStatus.Ok;

    public bool TryGetEntityActive(long entityHandle, out bool active)
    {
        active = false;
        if (Callbacks.GetEntityActive == null) return false;
        var result = 0;
        var status = Callbacks.GetEntityActive(ContextHandle, entityHandle, &result);
        active = result != 0;
        return status == NativeCallbackStatus.Ok;
    }

    public bool RequestExit() =>
        Callbacks.RequestExit != null &&
        Callbacks.RequestExit(ContextHandle) == NativeCallbackStatus.Ok;

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

    private bool TryGetInt32(
        delegate* unmanaged[Cdecl]<long, byte*, int*, int> callback,
        string key,
        out int value)
    {
        value = 0;
        if (callback == null) return false;
        var buffer = ToNullTerminatedUtf8(key);
        var result = 0;
        fixed (byte* pointer = buffer)
        {
            var status = callback(ContextHandle, pointer, &result);
            value = result;
            return status == NativeCallbackStatus.Ok;
        }
    }

    private bool TryGetDouble(
        delegate* unmanaged[Cdecl]<long, byte*, double*, int> callback,
        string key,
        out double value)
    {
        value = 0;
        if (callback == null) return false;
        var buffer = ToNullTerminatedUtf8(key);
        var result = 0.0;
        fixed (byte* pointer = buffer)
        {
            var status = callback(ContextHandle, pointer, &result);
            value = result;
            return status == NativeCallbackStatus.Ok;
        }
    }

    private bool TryGetString(
        delegate* unmanaged[Cdecl]<long, byte*, byte*, int, int> callback,
        string key,
        out string value)
    {
        value = string.Empty;
        if (callback == null) return false;
        var keyBuffer = ToNullTerminatedUtf8(key);
        var valueBuffer = new byte[4096];
        fixed (byte* keyPointer = keyBuffer)
        fixed (byte* valuePointer = valueBuffer)
        {
            if (callback(ContextHandle, keyPointer, valuePointer, valueBuffer.Length) !=
                NativeCallbackStatus.Ok) return false;
        }
        var length = Array.IndexOf(valueBuffer, (byte)0);
        if (length < 0) return false;
        value = Encoding.UTF8.GetString(valueBuffer, 0, length);
        return true;
    }

    private long ContextHandle { get; }
    private NativeCallbackTable Callbacks { get; }
}
