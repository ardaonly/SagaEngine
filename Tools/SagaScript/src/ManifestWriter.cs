// ManifestWriter.cs
// Writes SagaScript diagnostics and manifest JSON outputs.

using System.Text.Json;
using System.Security.Cryptography;

namespace SagaScript;

internal static class ManifestWriter
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true
    };

    public static void WriteJson<T>(string path, T payload)
    {
        var fullPath = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(fullPath) ?? ".");
        File.WriteAllText(fullPath, JsonSerializer.Serialize(payload, JsonOptions) + Environment.NewLine);
    }

    public static void PrintJson<T>(T payload)
    {
        Console.WriteLine(JsonSerializer.Serialize(payload, JsonOptions));
    }

    public static DiagnosticReport BuildDiagnosticReport(AnalysisResult result)
    {
        return new DiagnosticReport
        {
            Inputs = result.Inputs,
            Summary = result.Summary,
            Diagnostics = result.Diagnostics
        };
    }

    public static DiagnosticReport BuildMissingDotNetSdkReport()
    {
        return new DiagnosticReport
        {
            Summary = new DiagnosticSummary
            {
                ErrorCount = 1,
                HasBlockingDiagnostics = true
            },
            Diagnostics = new[]
            {
                new SagaDiagnostic
                {
                    Severity = SagaDiagnosticSeverity.Error.ToString(),
                    Code = "Script.Toolchain.DotNetSdkMissing",
                    Message = "SagaScript compile requires a .NET 10 SDK for target framework net10.0. Install a .NET 10 SDK and rerun; SagaScript will not downgrade target frameworks automatically."
                }
            }
        };
    }

    public static ScriptBindingManifest BuildBindingManifest(AnalysisResult result)
    {
        return new ScriptBindingManifest
        {
            SourceHash = result.SourceHash,
            Bindings = result.Bindings,
            DiagnosticSummary = result.Summary
        };
    }

    public static ScriptCapabilityManifest BuildCapabilityManifest(AnalysisResult result)
    {
        var entries = result.Bindings
            .GroupBy(binding => binding.ScriptId, StringComparer.Ordinal)
            .Select(group =>
            {
                var first = group.First();
                return new CapabilityEntry
                {
                    ScriptId = group.Key,
                    Authority = first.Authority,
                    PackageDestinationIntent = first.Authority == "ServerOnly" ? "server" :
                        first.Authority == "ClientOnly" ? "client" : "shared",
                    RequestedCapabilities = group.SelectMany(binding => binding.RequestedCapabilities)
                        .Distinct(StringComparer.Ordinal)
                        .Order(StringComparer.Ordinal)
                        .ToArray(),
                    GrantedCapabilities = Array.Empty<string>()
                };
            })
            .OrderBy(entry => entry.ScriptId, StringComparer.Ordinal)
            .ToArray();

        return new ScriptCapabilityManifest
        {
            SourceHash = result.SourceHash,
            Scripts = entries,
            DiagnosticSummary = result.Summary
        };
    }

    public static ScriptArtifactManifest BuildScriptArtifactManifest(
        AnalysisResult result,
        string projectRoot,
        string artifactOutputDirectory,
        string assemblyName)
    {
        var assemblyPath = Path.Combine(artifactOutputDirectory, assemblyName + ".scripts.dll");
        var runtimeConfigPath = Path.Combine(artifactOutputDirectory, assemblyName + ".scripts.runtimeconfig.json");
        var symbolsPath = Path.Combine(artifactOutputDirectory, assemblyName + ".scripts.pdb");
        if (!File.Exists(assemblyPath) || !File.Exists(runtimeConfigPath))
        {
            return new ScriptArtifactManifest
            {
                SourceHash = result.SourceHash,
                DiagnosticSummary = result.Summary
            };
        }

        var assemblyHash = ComputeFileHash(assemblyPath);
        var entries = result.Bindings
            .GroupBy(binding => binding.ScriptId, StringComparer.Ordinal)
            .Select(group =>
            {
                var first = group.First();
                return new ScriptArtifactEntry
                {
                    ArtifactId = BuildArtifactId(group.Key),
                    ScriptId = group.Key,
                    AssemblyPath = MakeRelative(projectRoot, assemblyPath),
                    RuntimeConfigPath = MakeRelative(projectRoot, runtimeConfigPath),
                    SymbolsPath = File.Exists(symbolsPath) ? MakeRelative(projectRoot, symbolsPath) : null,
                    Hash = "sha256:" + assemblyHash,
                    Authority = first.Authority,
                    PackageDestinationIntent = PackageDestinationForAuthority(first.Authority),
                    RequestedCapabilities = group.SelectMany(binding => binding.RequestedCapabilities)
                        .Distinct(StringComparer.Ordinal)
                        .Order(StringComparer.Ordinal)
                        .ToArray(),
                    GrantedCapabilities = Array.Empty<string>(),
                    BindingIds = group.Select(binding => binding.BindingId)
                        .Distinct(StringComparer.Ordinal)
                        .Order(StringComparer.Ordinal)
                        .ToArray(),
                    SourceFiles = group.Select(binding => MakeRelative(projectRoot, binding.SourceFile))
                        .Distinct(StringComparer.Ordinal)
                        .Order(StringComparer.Ordinal)
                        .ToArray()
                };
            })
            .OrderBy(entry => entry.ScriptId, StringComparer.Ordinal)
            .ToArray();

        return new ScriptArtifactManifest
        {
            SourceHash = result.SourceHash,
            Artifacts = entries,
            DiagnosticSummary = result.Summary
        };
    }

    public static GenericArtifactManifest BuildGenericScriptArtifactManifest(
        ScriptArtifactManifest manifest)
    {
        return new GenericArtifactManifest
        {
            Artifacts = manifest.Artifacts
                .Select(artifact => new ArtifactManifestEntry
                {
                    Id = artifact.ArtifactId,
                    Kind = "script",
                    Path = artifact.AssemblyPath,
                    Hash = artifact.Hash
                })
                .ToArray()
        };
    }

    private static string BuildArtifactId(string scriptId)
    {
        var suffix = scriptId.StartsWith("script://", StringComparison.Ordinal)
            ? scriptId["script://".Length..]
            : scriptId;
        suffix = string.Join(
            '/',
            suffix.Split('/', StringSplitOptions.RemoveEmptyEntries)
                .Select(part => SanitizeArtifactToken(part)));
        return "artifact://scripts/" + suffix;
    }

    private static string SanitizeArtifactToken(string token)
    {
        var sanitized = new string(token
            .Select(ch => char.IsLetterOrDigit(ch) || ch is '_' or '-' or '.'
                ? ch
                : '_')
            .ToArray());
        return string.IsNullOrWhiteSpace(sanitized) ? "unnamed" : sanitized;
    }

    private static string PackageDestinationForAuthority(string authority)
    {
        return authority == "ServerOnly" ? "server" :
            authority == "ClientOnly" ? "client" : "shared";
    }

    private static string MakeRelative(string projectRoot, string path)
    {
        if (string.IsNullOrWhiteSpace(projectRoot))
        {
            return Path.GetFullPath(path);
        }

        try
        {
            return Path.GetRelativePath(Path.GetFullPath(projectRoot), Path.GetFullPath(path))
                .Replace(Path.DirectorySeparatorChar, '/');
        }
        catch (ArgumentException)
        {
            return Path.GetFullPath(path);
        }
    }

    private static string ComputeFileHash(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
    }
}
