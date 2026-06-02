namespace SagaPreviewGate;

internal static class ToolInfo
{
    public const string Name = "sagapreviewgate";
    public const string Version = "0.0.9-dev";
}

internal sealed record Diagnostic
{
    public string Severity { get; init; } = "Info";
    public string Code { get; init; } = "";
    public string Message { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record PreviewCheck
{
    public string Id { get; init; } = "";
    public string Kind { get; init; } = "";
    public string Path { get; init; } = "";
    public string Status { get; init; } = "Missing";
    public bool Required { get; init; } = true;
}

internal sealed record ProcessRecord
{
    public string Executable { get; init; } = "";
    public IReadOnlyList<string> Arguments { get; init; } = Array.Empty<string>();
    public string WorkingDirectory { get; init; } = "";
    public int ExitCode { get; init; }
    public bool TimedOut { get; init; }
}

internal sealed record AcceptanceStep
{
    public int Sequence { get; init; }
    public string Id { get; init; } = "";
    public string Tool { get; init; } = "";
    public string Command { get; init; } = "";
    public string Status { get; init; } = "Failed";
    public string ReportPath { get; init; } = "";
    public ProcessRecord? Process { get; init; }
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record QuickstartReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "quickstart-check";
    public string Status { get; init; } = "Failed";
    public string RepoRoot { get; init; } = "";
    public string ProjectPath { get; init; } = "";
    public string BinDir { get; init; } = "";
    public IReadOnlyList<PreviewCheck> Checks { get; init; } = Array.Empty<PreviewCheck>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record AcceptanceReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "accept";
    public string Status { get; init; } = "Failed";
    public string RepoRoot { get; init; } = "";
    public string ProjectPath { get; init; } = "";
    public IReadOnlyList<AcceptanceStep> Steps { get; init; } = Array.Empty<AcceptanceStep>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record BuildMatrixEntry
{
    public string Platform { get; init; } = "";
    public string Configuration { get; init; } = "";
    public string EvidenceKind { get; init; } = "";
    public string Status { get; init; } = "Unavailable";
    public string EvidencePath { get; init; } = "";
    public string Notes { get; init; } = "";
}

internal sealed record BuildMatrixReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "build-matrix";
    public string Status { get; init; } = "Failed";
    public IReadOnlyList<BuildMatrixEntry> Entries { get; init; } = Array.Empty<BuildMatrixEntry>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record PackageFile
{
    public string SourcePath { get; init; } = "";
    public string PackagePath { get; init; } = "";
    public string Status { get; init; } = "Missing";
}

internal sealed record TechnicalPreviewPackageReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "package";
    public string Status { get; init; } = "Failed";
    public string PackageRoot { get; init; } = "";
    public IReadOnlyList<PackageFile> Files { get; init; } = Array.Empty<PackageFile>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record ClosureMatrixRow
{
    public string Id { get; init; } = "";
    public string Title { get; init; } = "";
    public string Status { get; init; } = "Missing";
    public string EvidencePath { get; init; } = "";
    public string ClaimLevel { get; init; } = "No claim";
}

internal sealed record ClosureReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "close";
    public string Status { get; init; } = "Blocked";
    public IReadOnlyList<ClosureMatrixRow> ImplementedMatrix { get; init; } = Array.Empty<ClosureMatrixRow>();
    public IReadOnlyList<ClosureMatrixRow> EvidenceMatrix { get; init; } = Array.Empty<ClosureMatrixRow>();
    public IReadOnlyList<string> ForbiddenClaimsPreserved { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> KnownDebt { get; init; } = Array.Empty<string>();
    public string Hedef2OpeningRecommendation { get; init; } = "";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}
