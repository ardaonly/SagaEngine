using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text;

namespace SagaEngine.Scripting.RuntimeBridge;

public static unsafe class NativeBridge
{
    private static readonly object Sync = new();
    private static readonly Dictionary<long, ScriptPackage> Packages = new();
    private static readonly Dictionary<long, SagaScript> Instances = new();
    private static readonly List<string> ProbeDirectories = new();
    private static long NextPackageHandle = 1;
    private static long NextInstanceHandle = 1;

    static NativeBridge()
    {
        AssemblyLoadContext.Default.Resolving += ResolveFromProbeDirectories;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int LoadAssembly(byte* assemblyPath, byte* errorBuffer, int errorBufferLength, long* packageHandle)
    {
        if (packageHandle is null)
        {
            WriteError(errorBuffer, errorBufferLength, "Package handle output pointer is null.");
            return BridgeStatus.InvalidArgument;
        }

        *packageHandle = 0;
        var path = ReadUtf8(assemblyPath);
        if (string.IsNullOrWhiteSpace(path))
        {
            WriteError(errorBuffer, errorBufferLength, "Script assembly path is empty.");
            return BridgeStatus.InvalidArgument;
        }

        try
        {
            var fullPath = Path.GetFullPath(path);
            var directory = Path.GetDirectoryName(fullPath);
            if (!string.IsNullOrWhiteSpace(directory))
            {
                lock (Sync)
                {
                    if (!ProbeDirectories.Contains(directory, StringComparer.Ordinal))
                    {
                        ProbeDirectories.Add(directory);
                    }
                }
            }

            var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(fullPath);
            lock (Sync)
            {
                var handle = NextPackageHandle++;
                Packages.Add(handle, new ScriptPackage(assembly));
                *packageHandle = handle;
            }

            return BridgeStatus.Ok;
        }
        catch (Exception ex)
        {
            WriteError(errorBuffer, errorBufferLength, ex.GetType().Name + ": " + ex.Message);
            return BridgeStatus.AssemblyLoadFailed;
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int CreateInstance(
        long packageHandle,
        byte* className,
        long contextHandle,
        long selfEntityHandle,
        NativeCallbackTable* callbacks,
        byte* errorBuffer,
        int errorBufferLength,
        long* instanceHandle)
    {
        if (instanceHandle is null)
        {
            WriteError(errorBuffer, errorBufferLength, "Instance handle output pointer is null.");
            return BridgeStatus.InvalidArgument;
        }

        *instanceHandle = 0;
        var managedClassName = ReadUtf8(className);
        if (string.IsNullOrWhiteSpace(managedClassName))
        {
            WriteError(errorBuffer, errorBufferLength, "Script class name is empty.");
            return BridgeStatus.InvalidArgument;
        }

        ScriptPackage package;
        lock (Sync)
        {
            if (!Packages.TryGetValue(packageHandle, out package!))
            {
                WriteError(errorBuffer, errorBufferLength, "Script package handle is invalid.");
                return BridgeStatus.InvalidArgument;
            }
        }

        try
        {
            if (callbacks is not null && callbacks->Version != 2)
            {
                WriteError(errorBuffer, errorBufferLength, "Native callback table version mismatch.");
                return BridgeStatus.InvalidArgument;
            }
            var type = package.Assembly.GetType(managedClassName, throwOnError: false, ignoreCase: false);
            if (type is null)
            {
                var availableTypes = string.Join(
                    ", ",
                    package.Assembly.GetTypes().Select(t => t.FullName).Order(StringComparer.Ordinal));
                WriteError(errorBuffer, errorBufferLength, "Script class was not found. Available types: " + availableTypes);
                return BridgeStatus.ClassMissing;
            }

            if (!typeof(SagaScript).IsAssignableFrom(type))
            {
                WriteError(errorBuffer, errorBufferLength, "Script class must derive from SagaScript.");
                return BridgeStatus.ClassInvalid;
            }

            if (type.IsAbstract)
            {
                WriteError(errorBuffer, errorBufferLength, "Script class must be concrete.");
                return BridgeStatus.ClassInvalid;
            }

            var instance = (SagaScript?)Activator.CreateInstance(type);
            if (instance is null)
            {
                WriteError(errorBuffer, errorBufferLength, "Script constructor returned null.");
                return BridgeStatus.InstanceCreationFailed;
            }

            instance.Context = callbacks is null
                ? ScriptContext.CreateUnavailable()
                : new ScriptContext(
                    new NativeScriptHost(contextHandle, *callbacks),
                    selfEntityHandle);

            lock (Sync)
            {
                var handle = NextInstanceHandle++;
                Instances.Add(handle, instance);
                *instanceHandle = handle;
            }

            return BridgeStatus.Ok;
        }
        catch (TargetInvocationException ex)
        {
            var inner = ex.InnerException ?? ex;
            WriteError(errorBuffer, errorBufferLength, inner.GetType().Name + ": " + inner.Message);
            return BridgeStatus.InstanceCreationFailed;
        }
        catch (Exception ex)
        {
            WriteError(errorBuffer, errorBufferLength, ex.GetType().Name + ": " + ex.Message);
            return BridgeStatus.InstanceCreationFailed;
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int InvokeLifecycle(long instanceHandle, int lifecycleMethod, float deltaTime, byte* errorBuffer, int errorBufferLength)
    {
        SagaScript instance;
        lock (Sync)
        {
            if (!Instances.TryGetValue(instanceHandle, out instance!))
            {
                WriteError(errorBuffer, errorBufferLength, "Script instance handle is invalid.");
                return BridgeStatus.InvalidArgument;
            }
        }

        try
        {
            switch (lifecycleMethod)
            {
            case 0:
                instance.OnCreate();
                break;
            case 1:
                instance.OnStart();
                break;
            case 2:
                instance.OnUpdate(deltaTime);
                break;
            case 3:
                instance.OnDestroy();
                break;
            default:
                WriteError(errorBuffer, errorBufferLength, "Lifecycle method token is invalid.");
                return BridgeStatus.InvalidArgument;
            }

            return BridgeStatus.Ok;
        }
        catch (Exception ex)
        {
            WriteError(errorBuffer, errorBufferLength, ex.GetType().Name + ": " + ex.Message);
            return BridgeStatus.ManagedException;
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int InvokeUiNamedAction(
        long instanceHandle,
        byte* methodName,
        byte* actionId,
        byte* screenId,
        byte* elementId,
        int eventType,
        byte* text,
        byte* errorBuffer,
        int errorBufferLength)
    {
        SagaScript instance;
        lock (Sync)
        {
            if (!Instances.TryGetValue(instanceHandle, out instance!))
            {
                WriteError(errorBuffer, errorBufferLength, "Script instance handle is invalid.");
                return BridgeStatus.InvalidArgument;
            }
        }

        var managedMethodName = ReadUtf8(methodName);
        if (string.IsNullOrWhiteSpace(managedMethodName))
        {
            WriteError(errorBuffer, errorBufferLength, "UI named action method name is empty.");
            return BridgeStatus.InvalidArgument;
        }

        var context = new UiNamedActionContext(
            ReadUtf8(actionId),
            ReadUtf8(screenId),
            ReadUtf8(elementId),
            (UiNamedActionEventType)eventType,
            ReadUtf8(text));

        try
        {
            var methods = instance.GetType()
                .GetMethods(BindingFlags.Instance | BindingFlags.Public)
                .Where(method => string.Equals(method.Name, managedMethodName, StringComparison.Ordinal))
                .ToArray();
            if (methods.Length == 0)
            {
                WriteError(errorBuffer, errorBufferLength, "UI named action method was not found.");
                return BridgeStatus.UiNamedActionMethodMissing;
            }

            var validMethods = methods.Where(IsValidUiNamedActionMethod).ToArray();
            if (validMethods.Length != 1)
            {
                WriteError(
                    errorBuffer,
                    errorBufferLength,
                    "UI named action method must be public instance void/bool Method(UiNamedActionContext).");
                return BridgeStatus.UiNamedActionInvalidSignature;
            }

            var returnValue = validMethods[0].Invoke(instance, new object[] { context });
            if (validMethods[0].ReturnType == typeof(bool) &&
                returnValue is bool handled &&
                !handled)
            {
                WriteError(errorBuffer, errorBufferLength, "UI named action method returned false.");
                return BridgeStatus.UiNamedActionReturnedFalse;
            }

            return BridgeStatus.Ok;
        }
        catch (TargetInvocationException ex)
        {
            var inner = ex.InnerException ?? ex;
            WriteError(errorBuffer, errorBufferLength, inner.GetType().Name + ": " + inner.Message);
            return BridgeStatus.ManagedException;
        }
        catch (Exception ex)
        {
            WriteError(errorBuffer, errorBufferLength, ex.GetType().Name + ": " + ex.Message);
            return BridgeStatus.ManagedException;
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int InvokeInt32BinaryMethod(
        long instanceHandle,
        byte* methodName,
        int left,
        int right,
        int* result,
        byte* errorBuffer,
        int errorBufferLength)
    {
        if (result is null)
        {
            WriteError(errorBuffer, errorBufferLength, "Result output pointer is null.");
            return BridgeStatus.InvalidArgument;
        }

        *result = 0;

        SagaScript instance;
        lock (Sync)
        {
            if (!Instances.TryGetValue(instanceHandle, out instance!))
            {
                WriteError(errorBuffer, errorBufferLength, "Script instance handle is invalid.");
                return BridgeStatus.InvalidArgument;
            }
        }

        var managedMethodName = ReadUtf8(methodName);
        if (string.IsNullOrWhiteSpace(managedMethodName))
        {
            WriteError(errorBuffer, errorBufferLength, "Int32 binary method name is empty.");
            return BridgeStatus.InvalidArgument;
        }

        try
        {
            var methods = instance.GetType()
                .GetMethods(BindingFlags.Instance | BindingFlags.Public)
                .Where(method => string.Equals(method.Name, managedMethodName, StringComparison.Ordinal))
                .ToArray();
            if (methods.Length == 0)
            {
                WriteError(errorBuffer, errorBufferLength, "Int32 binary method was not found.");
                return BridgeStatus.Int32BinaryMethodMissing;
            }

            var validMethods = methods.Where(IsValidInt32BinaryMethod).ToArray();
            if (validMethods.Length != 1)
            {
                WriteError(
                    errorBuffer,
                    errorBufferLength,
                    "Int32 binary method must be public instance int Method(int, int).");
                return BridgeStatus.Int32BinaryInvalidSignature;
            }

            var returnValue = validMethods[0].Invoke(instance, new object[] { left, right });
            if (returnValue is not int intValue)
            {
                WriteError(
                    errorBuffer,
                    errorBufferLength,
                    "Int32 binary method returned a non-int value.");
                return BridgeStatus.Int32BinaryInvalidSignature;
            }

            *result = intValue;
            return BridgeStatus.Ok;
        }
        catch (TargetInvocationException ex)
        {
            var inner = ex.InnerException ?? ex;
            WriteError(errorBuffer, errorBufferLength, inner.GetType().Name + ": " + inner.Message);
            return BridgeStatus.ManagedException;
        }
        catch (Exception ex)
        {
            WriteError(errorBuffer, errorBufferLength, ex.GetType().Name + ": " + ex.Message);
            return BridgeStatus.ManagedException;
        }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    public static int ReleasePackage(long packageHandle)
    {
        lock (Sync)
        {
            Packages.Remove(packageHandle);
        }

        return BridgeStatus.Ok;
    }

    private static bool IsValidUiNamedActionMethod(MethodInfo method)
    {
        var parameters = method.GetParameters();
        return parameters.Length == 1 &&
            parameters[0].ParameterType == typeof(UiNamedActionContext) &&
            (method.ReturnType == typeof(void) || method.ReturnType == typeof(bool));
    }

    private static bool IsValidInt32BinaryMethod(MethodInfo method)
    {
        var parameters = method.GetParameters();
        return parameters.Length == 2 &&
            parameters[0].ParameterType == typeof(int) &&
            parameters[1].ParameterType == typeof(int) &&
            method.ReturnType == typeof(int);
    }

    private static string ReadUtf8(byte* value)
    {
        if (value is null)
        {
            return string.Empty;
        }

        var length = 0;
        while (value[length] != 0)
        {
            ++length;
        }

        return Encoding.UTF8.GetString(value, length);
    }

    private static void WriteError(byte* buffer, int bufferLength, string message)
    {
        if (buffer is null || bufferLength <= 0)
        {
            return;
        }

        var bytes = Encoding.UTF8.GetBytes(message);
        var count = Math.Min(bytes.Length, bufferLength - 1);
        for (var index = 0; index < count; ++index)
        {
            buffer[index] = bytes[index];
        }

        buffer[count] = 0;
    }

    private static Assembly? ResolveFromProbeDirectories(
        AssemblyLoadContext context,
        AssemblyName assemblyName)
    {
        var loadedAssembly = AppDomain.CurrentDomain.GetAssemblies()
            .FirstOrDefault(assembly =>
                string.Equals(
                    assembly.GetName().Name,
                    assemblyName.Name,
                    StringComparison.Ordinal));
        if (loadedAssembly is not null)
        {
            return loadedAssembly;
        }

        List<string> directories;
        lock (Sync)
        {
            directories = ProbeDirectories.ToList();
        }

        foreach (var directory in directories)
        {
            var candidate = Path.Combine(directory, assemblyName.Name + ".dll");
            if (File.Exists(candidate))
            {
                return context.LoadFromAssemblyPath(candidate);
            }
        }

        return null;
    }

    private sealed record ScriptPackage(Assembly Assembly);
}

internal static class BridgeStatus
{
    public const int Ok = 0;
    public const int InvalidArgument = 1;
    public const int AssemblyLoadFailed = 2;
    public const int ClassMissing = 3;
    public const int ClassInvalid = 4;
    public const int InstanceCreationFailed = 5;
    public const int LifecycleMethodMissing = 6;
    public const int LifecycleFailed = 7;
    public const int ManagedException = 8;
    public const int UiNamedActionMethodMissing = 9;
    public const int UiNamedActionInvalidSignature = 10;
    public const int UiNamedActionReturnedFalse = 11;
    public const int Int32BinaryMethodMissing = 12;
    public const int Int32BinaryInvalidSignature = 13;
}
