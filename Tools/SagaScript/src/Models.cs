// Models.cs
// Data contracts used by the standalone SagaScript CLI.

using System.Text.Json.Serialization;

namespace SagaScript;

internal enum SagaDiagnosticSeverity
{
    Info,
    Warning,
    Error,
    Security
}

internal sealed record SourceRange(
    [property: JsonPropertyName("startLine")] int StartLine,
    [property: JsonPropertyName("startColumn")] int StartColumn,
    [property: JsonPropertyName("endLine")] int EndLine,
    [property: JsonPropertyName("endColumn")] int EndColumn);

internal sealed record SagaDiagnostic
{
    public string Severity { get; init; } = SagaDiagnosticSeverity.Info.ToString();
    public string Code { get; init; } = "";
    public string Message { get; init; } = "";
    public string? ScriptId { get; init; }
    public string? SourceFile { get; init; }
    public SourceRange? SourceRange { get; init; }
    public string? BindingId { get; init; }
    public string? Capability { get; init; }
}

internal sealed record DiagnosticSummary
{
    public int InfoCount { get; init; }
    public int WarningCount { get; init; }
    public int ErrorCount { get; init; }
    public int SecurityCount { get; init; }
    public bool HasBlockingDiagnostics { get; init; }
}

internal sealed record DiagnosticReport
{
    public int SchemaVersion { get; init; } = 1;
    public string ToolName { get; init; } = ToolInfo.Name;
    public string ToolchainVersion { get; init; } = ToolInfo.Version;
    public IReadOnlyList<string> Inputs { get; init; } = Array.Empty<string>();
    public DiagnosticSummary Summary { get; init; } = new();
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record ScriptParameter
{
    public string Name { get; init; } = "";
    public string Type { get; init; } = "";
    public bool Supported { get; init; }
}

internal sealed record ScriptReturn
{
    public string Type { get; init; } = "void";
    public bool Supported { get; init; }
}

internal sealed record GeneratedCodeOrigin
{
    public string? SourceResourceId { get; init; }
    public string? SourceHash { get; init; }
    public string? GeneratedPath { get; init; }
}

internal sealed record ScriptBinding
{
    public string BindingId { get; init; } = "";
    public string ScriptId { get; init; } = "";
    public string DeclaringType { get; init; } = "";
    public string MethodName { get; init; } = "";
    public string? BlockName { get; init; }
    public string? BlockCategory { get; init; }
    public IReadOnlyList<ScriptParameter> Parameters { get; init; } = Array.Empty<ScriptParameter>();
    public ScriptReturn ReturnType { get; init; } = new();
    public string Authority { get; init; } = "";
    public IReadOnlyList<string> SideEffects { get; init; } = Array.Empty<string>();
    public bool PredictionUnsafe { get; init; }
    public IReadOnlyList<string> RequestedCapabilities { get; init; } = Array.Empty<string>();
    public string SourceFile { get; init; } = "";
    public SourceRange? SourceRange { get; init; }
    public string SourceHash { get; init; } = "";
    public GeneratedCodeOrigin? GeneratedOrigin { get; init; }
}

internal sealed record ScriptBindingManifest
{
    public int SchemaVersion { get; init; } = 1;
    public string ToolName { get; init; } = ToolInfo.Name;
    public string ToolchainVersion { get; init; } = ToolInfo.Version;
    public string SourceHash { get; init; } = "";
    public IReadOnlyList<ScriptBinding> Bindings { get; init; } = Array.Empty<ScriptBinding>();
    public DiagnosticSummary DiagnosticSummary { get; init; } = new();
}

internal sealed record CapabilityEntry
{
    public string ScriptId { get; init; } = "";
    public string Authority { get; init; } = "";
    public string PackageDestinationIntent { get; init; } = "";
    public IReadOnlyList<string> RequestedCapabilities { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> GrantedCapabilities { get; init; } = Array.Empty<string>();
}

internal sealed record ScriptCapabilityManifest
{
    public int SchemaVersion { get; init; } = 1;
    public string ToolName { get; init; } = ToolInfo.Name;
    public string ToolchainVersion { get; init; } = ToolInfo.Version;
    public string SourceHash { get; init; } = "";
    public IReadOnlyList<CapabilityEntry> Scripts { get; init; } = Array.Empty<CapabilityEntry>();
    public DiagnosticSummary DiagnosticSummary { get; init; } = new();
}

internal sealed record ScriptArtifactEntry
{
    public string ArtifactId { get; init; } = "";
    public string ScriptId { get; init; } = "";
    public string AssemblyPath { get; init; } = "";
    public string RuntimeConfigPath { get; init; } = "";
    public string? SymbolsPath { get; init; }
    public string Hash { get; init; } = "";
    public string Authority { get; init; } = "";
    public string PackageDestinationIntent { get; init; } = "";
    public IReadOnlyList<string> RequestedCapabilities { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> GrantedCapabilities { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> BindingIds { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> SourceFiles { get; init; } = Array.Empty<string>();
}

internal sealed record ScriptArtifactManifest
{
    public int SchemaVersion { get; init; } = 1;
    public string ToolName { get; init; } = ToolInfo.Name;
    public string ToolchainVersion { get; init; } = ToolInfo.Version;
    public string TargetFramework { get; init; } = ToolInfo.TargetFramework;
    public string SourceHash { get; init; } = "";
    public IReadOnlyList<ScriptArtifactEntry> Artifacts { get; init; } = Array.Empty<ScriptArtifactEntry>();
    public DiagnosticSummary DiagnosticSummary { get; init; } = new();
}

internal sealed record ArtifactManifestEntry
{
    public string Id { get; init; } = "";
    public string Kind { get; init; } = "script";
    public string Path { get; init; } = "";
    public string? Hash { get; init; }
}

internal sealed record GenericArtifactManifest
{
    public int SchemaVersion { get; init; } = 1;
    public IReadOnlyList<ArtifactManifestEntry> Artifacts { get; init; } = Array.Empty<ArtifactManifestEntry>();
}

internal sealed record InspectionReport
{
    public int SchemaVersion { get; init; } = 1;
    public string ToolName { get; init; } = ToolInfo.Name;
    public string ToolchainVersion { get; init; } = ToolInfo.Version;
    public IReadOnlyList<ScriptBinding> Bindings { get; init; } = Array.Empty<ScriptBinding>();
    public DiagnosticSummary DiagnosticSummary { get; init; } = new();
}

internal sealed record AnalysisResult
{
    public IReadOnlyList<string> Inputs { get; init; } = Array.Empty<string>();
    public string SourceHash { get; init; } = "";
    public IReadOnlyList<ScriptBinding> Bindings { get; init; } = Array.Empty<ScriptBinding>();
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
    public DiagnosticSummary Summary { get; init; } = new();
}

internal static class ToolInfo
{
    public const string Name = "sagascript";
    public const string Version = "0.0.8-dev";
    public const string TargetFramework = "net10.0";
    public const string RuntimeFrameworkVersion = "10.0.0";
}
