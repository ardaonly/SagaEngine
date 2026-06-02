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

internal sealed record SourceSpan(
    [property: JsonPropertyName("startLine")] int StartLine,
    [property: JsonPropertyName("startColumn")] int StartColumn,
    [property: JsonPropertyName("endLine")] int EndLine,
    [property: JsonPropertyName("endColumn")] int EndColumn,
    [property: JsonPropertyName("startByte")] int StartByte,
    [property: JsonPropertyName("endByte")] int EndByte);

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

internal sealed record SagaWeaverBehavior
{
    public string BehaviorId { get; init; } = "";
    public string ApiLevel { get; init; } = "Unsupported";
    public string ApiDomain { get; init; } = "Unsupported";
    public string Compatibility { get; init; } = "Unsupported";
    public string DeclaringType { get; init; } = "";
    public string MethodName { get; init; } = "";
    public string SourceFile { get; init; } = "";
    public string SourceHash { get; init; } = "";
    public SourceSpan? SourceSpan { get; init; }
    public IReadOnlyList<string> NamespaceUsages { get; init; } = Array.Empty<string>();
    public IReadOnlyList<SagaWeaverNode> Nodes { get; init; } = Array.Empty<SagaWeaverNode>();
}

internal sealed record SagaWeaverNode
{
    public string NodeId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string ApiLevel { get; init; } = "Unsupported";
    public string ApiDomain { get; init; } = "Unsupported";
    public string Capability { get; init; } = "ProjectionOnly";
    public string ProjectionCompatibility { get; init; } = "ReadOnly";
    public bool ReadOnly { get; init; } = true;
    public SourceSpan? SourceSpan { get; init; }
    public string? OpaqueReason { get; init; }
    public string? LiteralValue { get; init; }
}

internal sealed record SagaWeaverAnalysisReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "analyze";
    public string Status { get; init; } = "Passed";
    public string SourceHash { get; init; } = "";
    public IReadOnlyList<SagaWeaverBehavior> Behaviors { get; init; } = Array.Empty<SagaWeaverBehavior>();
    public DiagnosticSummary Summary { get; init; } = new();
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record RuntimeBindingReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "emit-bindings";
    public string Status { get; init; } = "Passed";
    public string SourceHash { get; init; } = "";
    public IReadOnlyList<RuntimeBindingEntry> Bindings { get; init; } = Array.Empty<RuntimeBindingEntry>();
    public DiagnosticSummary Summary { get; init; } = new();
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record RuntimeBindingEntry
{
    public string BehaviorId { get; init; } = "";
    public string ApiLevel { get; init; } = "";
    public string ApiDomain { get; init; } = "";
    public string DeclaringType { get; init; } = "";
    public string MethodName { get; init; } = "";
    public string SourceFile { get; init; } = "";
    public string SourceHash { get; init; } = "";
    public SourceSpan? SourceSpan { get; init; }
    public string Compatibility { get; init; } = "";
    public IReadOnlyList<RuntimeBindingNode> Nodes { get; init; } = Array.Empty<RuntimeBindingNode>();
    public IReadOnlyList<string> LibraryIds { get; init; } = Array.Empty<string>();
}

internal sealed record RuntimeBindingNode
{
    public string NodeId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string ApiDomain { get; init; } = "";
    public string ApiLevel { get; init; } = "";
    public string Capability { get; init; } = "";
    public string ProjectionCompatibility { get; init; } = "";
    public string CompatibilityClassification { get; init; } = "";
    public string RuntimeProof { get; init; } = "";
}

internal sealed record SourceMapReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "source-map";
    public string Status { get; init; } = "Passed";
    public string SourceHash { get; init; } = "";
    public IReadOnlyList<SourceMapFile> Files { get; init; } = Array.Empty<SourceMapFile>();
    public IReadOnlyList<SourceMapBehavior> Behaviors { get; init; } = Array.Empty<SourceMapBehavior>();
    public DiagnosticSummary Summary { get; init; } = new();
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record SourceMapFile
{
    public string SourceFile { get; init; } = "";
    public string SourceHash { get; init; } = "";
    public int ByteLength { get; init; }
}

internal sealed record SourceMapBehavior
{
    public string BehaviorId { get; init; } = "";
    public string ApiLevel { get; init; } = "";
    public string ApiDomain { get; init; } = "";
    public string SourceFile { get; init; } = "";
    public string SourceHash { get; init; } = "";
    public SourceSpan? SourceSpan { get; init; }
    public IReadOnlyList<SagaWeaverNode> Nodes { get; init; } = Array.Empty<SagaWeaverNode>();
}

internal sealed record ProjectionReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "project-blocks";
    public string Status { get; init; } = "Passed";
    public string ApiLevel { get; init; } = "Unsupported";
    public string ApiDomain { get; init; } = "Unsupported";
    public string SourceHash { get; init; } = "";
    public IReadOnlyList<SagaWeaverBehavior> Behaviors { get; init; } = Array.Empty<SagaWeaverBehavior>();
    public DiagnosticSummary Summary { get; init; } = new();
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record NodeMetadataReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "project-blocks";
    public string Status { get; init; } = "Passed";
    public string SourceHash { get; init; } = "";
    public IReadOnlyList<NodeMetadataEntry> Nodes { get; init; } = Array.Empty<NodeMetadataEntry>();
    public DiagnosticSummary Summary { get; init; } = new();
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record NodeMetadataEntry
{
    public string NodeId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string ApiDomain { get; init; } = "";
    public string ApiLevel { get; init; } = "";
    public string Capability { get; init; } = "";
    public string ProjectionCompatibility { get; init; } = "";
    public string Level { get; init; } = "";
    public string Domain { get; init; } = "";
    public IReadOnlyList<string> Capabilities { get; init; } = Array.Empty<string>();
    public string SourceFile { get; init; } = "";
    public SourceSpan? SourceSpan { get; init; }
    public bool ReadOnly { get; init; } = true;
}

internal sealed record NodeLibraryReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "extract-nodes";
    public string Status { get; init; } = "Passed";
    public string SourceHash { get; init; } = "";
    public IReadOnlyList<SourceMapFile> SourceFiles { get; init; } = Array.Empty<SourceMapFile>();
    public IReadOnlyList<NodeLibraryEntry> Libraries { get; init; } = Array.Empty<NodeLibraryEntry>();
    public IReadOnlyList<NodeLibraryNodeEntry> Nodes { get; init; } = Array.Empty<NodeLibraryNodeEntry>();
    public DiagnosticSummary Summary { get; init; } = new();
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record NodeLibraryEntry
{
    public string LibraryId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string ApiDomain { get; init; } = "";
    public string ApiLevel { get; init; } = "";
    public string SourceFile { get; init; } = "";
    public SourceSpan? SourceSpan { get; init; }
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record NodeLibraryNodeEntry
{
    public string NodeId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string ApiDomain { get; init; } = "";
    public string ApiLevel { get; init; } = "";
    public string Capability { get; init; } = "";
    public string SourceFile { get; init; } = "";
    public SourceSpan? SourceSpan { get; init; }
    public string Compatibility { get; init; } = "";
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record CompatibilityConstruct
{
    public string ConstructId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string Classification { get; init; } = "";
    public string ApiDomain { get; init; } = "Unsupported";
    public string ApiLevel { get; init; } = "Unsupported";
    public string Capability { get; init; } = "Unsupported";
    public string SourceFile { get; init; } = "";
    public SourceSpan? SourceSpan { get; init; }
    public bool Editable { get; init; }
    public string PatchOperation { get; init; } = "";
    public string Reason { get; init; } = "";
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record CompatibilityProfileReport
{
    public int SchemaVersion { get; init; } = 2;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "compatibility-profile";
    public string Status { get; init; } = "Passed";
    public IReadOnlyList<SourceMapFile> SourceFiles { get; init; } = Array.Empty<SourceMapFile>();
    public IReadOnlyDictionary<string, string> SourceHashes { get; init; } = new Dictionary<string, string>();
    public IReadOnlyList<CompatibilityConstruct> Constructs { get; init; } = Array.Empty<CompatibilityConstruct>();
    public IReadOnlyDictionary<string, int> Summary { get; init; } = new Dictionary<string, int>();
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal sealed record CheckedScriptArtifact
{
    public string Kind { get; init; } = "";
    public string Path { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed record RuntimeProofSummary
{
    public int RuntimeBackedWithEvidence { get; init; }
    public int RuntimeBackedMissingEvidence { get; init; }
    public int ProjectionOnly { get; init; }
    public int Deferred { get; init; }
}

internal sealed record ScriptArtifactValidationReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "validate-artifacts";
    public string Status { get; init; } = "Passed";
    public string ArtifactRoot { get; init; } = "";
    public IReadOnlyList<CheckedScriptArtifact> CheckedArtifacts { get; init; } = Array.Empty<CheckedScriptArtifact>();
    public RuntimeProofSummary RuntimeProofSummary { get; init; } = new();
    public IReadOnlyDictionary<string, int> Summary { get; init; } = new Dictionary<string, int>();
    public IReadOnlyList<SagaDiagnostic> Diagnostics { get; init; } = Array.Empty<SagaDiagnostic>();
}

internal static class ToolInfo
{
    public const string Name = "sagascript";
    public const string Version = "0.0.8-dev";
    public const string TargetFramework = "net10.0";
    public const string RuntimeFrameworkVersion = "10.0.0";
}
