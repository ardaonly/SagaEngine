// SagaScriptCompiler.cs
// Emits validated SagaScript assemblies and script artifact manifests.

using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Text;

namespace SagaScript;

internal sealed record SagaScriptCompileRequest
{
    public required AnalysisResult Analysis { get; init; }
    public required IReadOnlyList<string> SourceFiles { get; init; }
    public required string ManifestOutputDirectory { get; init; }
    public required string ArtifactOutputDirectory { get; init; }
    public required string ProjectRoot { get; init; }
    public string AssemblyName { get; init; } = "SagaProjectScripts";
}

internal sealed record SagaScriptCompileResult
{
    public required ScriptArtifactManifest ScriptArtifactManifest { get; init; }
    public required GenericArtifactManifest GenericArtifactManifest { get; init; }
    public required DiagnosticReport DiagnosticReport { get; init; }
}

internal static class SagaScriptCompiler
{
    private const string AttributeShimPath = "__SagaScriptAttributes.g.cs";
    private const string RuntimeBridgeAssemblyName = "SagaScript.RuntimeBridge.dll";
    private const string RuntimeBridgeMissingCode = "Script.Toolchain.RuntimeBridgeMissing";

    public static SagaScriptCompileResult Compile(SagaScriptCompileRequest request)
    {
        Directory.CreateDirectory(request.ManifestOutputDirectory);
        Directory.CreateDirectory(request.ArtifactOutputDirectory);
        ClearAssemblyOutputs(request);

        var diagnostics = request.Analysis.Diagnostics.ToList();
        if (!request.Analysis.Summary.HasBlockingDiagnostics)
        {
            EmitAssembly(request, diagnostics);
        }

        var summary = SagaScriptAnalyzer.BuildSummary(diagnostics);
        var analysis = request.Analysis with
        {
            Diagnostics = diagnostics,
            Summary = summary
        };

        var scriptArtifacts = ManifestWriter.BuildScriptArtifactManifest(
            analysis,
            request.ProjectRoot,
            request.ArtifactOutputDirectory,
            request.AssemblyName);
        var genericArtifacts = ManifestWriter.BuildGenericScriptArtifactManifest(scriptArtifacts);
        var diagnosticsReport = ManifestWriter.BuildDiagnosticReport(analysis);

        ManifestWriter.WriteJson(
            Path.Combine(request.ManifestOutputDirectory, "script_bindings.json"),
            ManifestWriter.BuildBindingManifest(analysis));
        ManifestWriter.WriteJson(
            Path.Combine(request.ManifestOutputDirectory, "script_capabilities.json"),
            ManifestWriter.BuildCapabilityManifest(analysis));
        ManifestWriter.WriteJson(
            Path.Combine(request.ManifestOutputDirectory, "script_artifacts.json"),
            scriptArtifacts);
        ManifestWriter.WriteJson(
            Path.Combine(request.ManifestOutputDirectory, "artifact_manifest.scripts.json"),
            genericArtifacts);
        ManifestWriter.WriteJson(
            Path.Combine(request.ManifestOutputDirectory, "sagascript_diagnostics.json"),
            diagnosticsReport);

        return new SagaScriptCompileResult
        {
            ScriptArtifactManifest = scriptArtifacts,
            GenericArtifactManifest = genericArtifacts,
            DiagnosticReport = diagnosticsReport
        };
    }

    private static void EmitAssembly(SagaScriptCompileRequest request, List<SagaDiagnostic> diagnostics)
    {
        var runtimeBridgeAssembly = ResolveRuntimeBridgeAssembly();
        if (runtimeBridgeAssembly is null)
        {
            diagnostics.Add(new SagaDiagnostic
            {
                Severity = SagaDiagnosticSeverity.Error.ToString(),
                Code = RuntimeBridgeMissingCode,
                Message = "SagaScript compile requires SagaScript.RuntimeBridge.dll. Build the managed runtime bridge or set SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY."
            });
            return;
        }

        var syntaxTrees = request.SourceFiles
            .Select(file => CSharpSyntaxTree.ParseText(
                SourceText.From(File.ReadAllText(file), Encoding.UTF8),
                path: file))
            .Append(CSharpSyntaxTree.ParseText(
                SourceText.From(BuildAttributeShim(), Encoding.UTF8),
                path: AttributeShimPath))
            .ToArray();

        var assemblyPath = Path.Combine(request.ArtifactOutputDirectory, request.AssemblyName + ".scripts.dll");
        var symbolsPath = Path.Combine(request.ArtifactOutputDirectory, request.AssemblyName + ".scripts.pdb");
        var runtimeConfigPath = Path.Combine(request.ArtifactOutputDirectory, request.AssemblyName + ".scripts.runtimeconfig.json");

        var compilation = CSharpCompilation.Create(
            request.AssemblyName + ".Scripts",
            syntaxTrees,
            BuildMetadataReferences(runtimeBridgeAssembly),
            new CSharpCompilationOptions(
                OutputKind.DynamicallyLinkedLibrary,
                optimizationLevel: OptimizationLevel.Release,
                deterministic: true,
                nullableContextOptions: NullableContextOptions.Enable));

        using var assemblyStream = File.Create(assemblyPath);
        using var symbolsStream = File.Create(symbolsPath);
        var emit = compilation.Emit(assemblyStream, pdbStream: symbolsStream);
        if (!emit.Success)
        {
            assemblyStream.Close();
            symbolsStream.Close();
            TryDelete(assemblyPath);
            TryDelete(symbolsPath);
            TryDelete(runtimeConfigPath);
            diagnostics.AddRange(emit.Diagnostics
                .Where(diagnostic => diagnostic.Severity is DiagnosticSeverity.Error)
                .Select(ToSagaDiagnostic));
            return;
        }

        WriteRuntimeConfig(runtimeConfigPath);
        File.Copy(
            runtimeBridgeAssembly,
            Path.Combine(request.ArtifactOutputDirectory, RuntimeBridgeAssemblyName),
            overwrite: true);
    }

    private static void ClearAssemblyOutputs(SagaScriptCompileRequest request)
    {
        TryDelete(Path.Combine(request.ArtifactOutputDirectory, request.AssemblyName + ".scripts.dll"));
        TryDelete(Path.Combine(request.ArtifactOutputDirectory, request.AssemblyName + ".scripts.pdb"));
        TryDelete(Path.Combine(request.ArtifactOutputDirectory, request.AssemblyName + ".scripts.runtimeconfig.json"));
        TryDelete(Path.Combine(request.ArtifactOutputDirectory, RuntimeBridgeAssemblyName));
    }

    private static IReadOnlyList<MetadataReference> BuildMetadataReferences(
        string runtimeBridgeAssembly)
    {
        var trustedPlatformAssemblies =
            AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") as string;
        if (!string.IsNullOrWhiteSpace(trustedPlatformAssemblies))
        {
            return trustedPlatformAssemblies
                .Split(Path.PathSeparator)
                .Where(path => path.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
                .Append(runtimeBridgeAssembly)
                .Select(path => MetadataReference.CreateFromFile(path))
                .ToArray();
        }

        return new[]
        {
            MetadataReference.CreateFromFile(typeof(object).Assembly.Location),
            MetadataReference.CreateFromFile(typeof(Attribute).Assembly.Location),
            MetadataReference.CreateFromFile(typeof(Enumerable).Assembly.Location),
            MetadataReference.CreateFromFile(typeof(RuntimeInformation).Assembly.Location),
            MetadataReference.CreateFromFile(runtimeBridgeAssembly)
        };
    }

    private static string? ResolveRuntimeBridgeAssembly()
    {
        var candidates = new List<string>();
        var environmentPath = Environment.GetEnvironmentVariable("SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY");
        if (!string.IsNullOrWhiteSpace(environmentPath))
        {
            candidates.Add(environmentPath);
        }

        candidates.Add(Path.Combine(AppContext.BaseDirectory, RuntimeBridgeAssemblyName));
        candidates.Add(Path.Combine(
            Directory.GetCurrentDirectory(),
            "build",
            "RelWithDebInfo",
            "Managed",
            "SagaScript.RuntimeBridge",
            RuntimeBridgeAssemblyName));
        candidates.Add(Path.Combine(
            Directory.GetCurrentDirectory(),
            "Engine",
            "Managed",
            "SagaScript.RuntimeBridge",
            "bin",
            "Release",
            "net10.0",
            RuntimeBridgeAssemblyName));

        return candidates
            .Select(Path.GetFullPath)
            .FirstOrDefault(File.Exists);
    }

    private static SagaDiagnostic ToSagaDiagnostic(Diagnostic diagnostic)
    {
        var span = diagnostic.Location.GetLineSpan();
        SourceRange? range = diagnostic.Location.IsInSource
            ? new SourceRange(
                span.StartLinePosition.Line + 1,
                span.StartLinePosition.Character + 1,
                span.EndLinePosition.Line + 1,
                span.EndLinePosition.Character + 1)
            : null;

        return new SagaDiagnostic
        {
            Severity = SagaDiagnosticSeverity.Error.ToString(),
            Code = "Script.Compile.Roslyn",
            Message = diagnostic.ToString(),
            SourceFile = diagnostic.Location.IsInSource ? span.Path : null,
            SourceRange = range
        };
    }

    private static void WriteRuntimeConfig(string runtimeConfigPath)
    {
        var payload = new
        {
            runtimeOptions = new
            {
                tfm = ToolInfo.TargetFramework,
                framework = new
                {
                    name = "Microsoft.NETCore.App",
                    version = ToolInfo.RuntimeFrameworkVersion
                }
            }
        };

        var json = JsonSerializer.Serialize(payload, new JsonSerializerOptions
        {
            PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
            WriteIndented = true
        });
        File.WriteAllText(runtimeConfigPath, json + Environment.NewLine);
    }

    private static string BuildAttributeShim()
    {
        return """
using System;

[AttributeUsage(AttributeTargets.Class | AttributeTargets.Method, AllowMultiple = false)]
internal sealed class SagaScriptIdAttribute : Attribute
{
    public SagaScriptIdAttribute(string value) {}
}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
internal sealed class BlockCallableAttribute : Attribute {}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
internal sealed class BlockCategoryAttribute : Attribute
{
    public BlockCategoryAttribute(string value) {}
}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
internal sealed class BlockNameAttribute : Attribute
{
    public BlockNameAttribute(string value) {}
}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
internal sealed class ClientOnlyAttribute : Attribute {}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
internal sealed class ServerOnlyAttribute : Attribute {}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
internal sealed class SharedPureAttribute : Attribute {}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
internal sealed class WritesPersistentStateAttribute : Attribute {}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
internal sealed class ReplicatesAttribute : Attribute {}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
internal sealed class PredictionUnsafeAttribute : Attribute {}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = true)]
internal sealed class RequiresCapabilityAttribute : Attribute
{
    public RequiresCapabilityAttribute(string value) {}
}

[AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
internal sealed class GeneratedCodeOriginAttribute : Attribute
{
    public GeneratedCodeOriginAttribute(string sourceResourceId, string sourceHash, string generatedPath) {}
}
""";
    }

    private static void TryDelete(string path)
    {
        try
        {
            if (File.Exists(path))
            {
                File.Delete(path);
            }
        }
        catch (IOException)
        {
        }
    }
}
