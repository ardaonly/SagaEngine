namespace SagaLaunchLab;

internal sealed class SagaProjectManifest
{
    public int SchemaVersion { get; init; }
    public string ProjectId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public List<ProjectPathReference> LaunchProfiles { get; init; } = [];
}

internal sealed class ProjectPathReference
{
    public string Id { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed class LaunchProfilesDocument
{
    public int SchemaVersion { get; init; }
    public List<LaunchProfile> Profiles { get; init; } = [];
}

internal sealed class LaunchProfile
{
    public string Id { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string Role { get; init; } = "";
    public string Mode { get; init; } = "";
    public string Executable { get; init; } = "";
    public string ReportPath { get; init; } = "";
    public string DiagnosticsPath { get; init; } = "";
    public List<string> Arguments { get; init; } = [];
}

internal sealed class LaunchDiagnostic
{
    public string Severity { get; init; } = "Error";
    public string Code { get; init; } = "";
    public string Category { get; init; } = "Launch";
    public string Path { get; init; } = "";
    public string Message { get; init; } = "";
}

internal sealed class ProjectReportSummary
{
    public string ProjectId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string ProjectRoot { get; init; } = "";
    public string ManifestPath { get; init; } = "";
}

internal sealed class LaunchProfileSummary
{
    public string Id { get; init; } = "";
    public string Role { get; init; } = "";
    public string Mode { get; init; } = "";
}

internal sealed class LaunchProcessInfo
{
    public string Executable { get; init; } = "";
    public List<string> Arguments { get; init; } = [];
    public string WorkingDirectory { get; init; } = "";
    public int? ExitCode { get; init; }
    public bool TimedOut { get; init; }
    public long DurationMs { get; init; }
}

internal sealed class LaunchArtifacts
{
    public string HeadlessReportPath { get; init; } = "";
    public string DiagnosticsDirectory { get; init; } = "";
    public string StdoutPath { get; init; } = "";
    public string StderrPath { get; init; } = "";
    public string ProjectValidationReportPath { get; init; } = "";
    public string ProjectResolutionReportPath { get; init; } = "";
}

internal sealed class LaunchPreviewReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagalaunch";
    public string Command { get; init; } = "";
    public string Status { get; init; } = "Failed";
    public ProjectReportSummary Project { get; init; } = new();
    public LaunchProfileSummary LaunchProfile { get; init; } = new();
    public LaunchProcessInfo Process { get; init; } = new();
    public LaunchArtifacts Artifacts { get; init; } = new();
    public List<LaunchDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class AcceptanceStage
{
    public string Name { get; init; } = "";
    public string Status { get; init; } = "";
    public string ReportPath { get; init; } = "";
    public string Reason { get; init; } = "";
}

internal sealed class AcceptanceReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagalaunch";
    public string Command { get; init; } = "accept";
    public string Status { get; init; } = "Failed";
    public ProjectReportSummary Project { get; init; } = new();
    public AcceptanceStage ProjectValidation { get; init; } = new();
    public AcceptanceStage ProjectResolution { get; init; } = new();
    public AcceptanceStage ServerPreview { get; init; } = new();
    public List<AcceptanceStage> DeferredStages { get; init; } = [];
    public List<LaunchDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class LaunchProfileMatrixRow
{
    public string ProfileId { get; init; } = "";
    public string Role { get; init; } = "";
    public string Mode { get; init; } = "";
    public string Status { get; init; } = "Failed";
    public string ExecutablePath { get; init; } = "";
    public string Reason { get; init; } = "";
    public List<LaunchDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class LaunchProfileMatrixReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagalaunch";
    public string Command { get; init; } = "profile-matrix";
    public string Status { get; init; } = "Failed";
    public ProjectReportSummary Project { get; init; } = new();
    public List<LaunchProfileMatrixRow> Profiles { get; init; } = [];
    public List<LaunchDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class LaunchSourceTruthAlignmentReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagalaunch";
    public string Command { get; init; } = "source-truth-alignment";
    public string Status { get; init; } = "Failed";
    public ProjectReportSummary Project { get; init; } = new();
    public List<LaunchSourceTruthProfileItem> LaunchProfiles { get; init; } = [];
    public LaunchAlignmentEvidence SourceTruthGate { get; init; } = new();
    public LaunchAlignmentEvidence RuntimeReadiness { get; init; } = new();
    public List<AcceptanceStage> DeferredStages { get; init; } = [];
    public List<LaunchDiagnostic> Diagnostics { get; init; } = [];
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal sealed class LaunchSourceTruthProfileItem
{
    public string ProfileId { get; init; } = "";
    public string Role { get; init; } = "";
    public string Mode { get; init; } = "";
    public string SourceTruthCompatibility { get; init; } = "";
    public string EvidenceStatus { get; init; } = "";
}

internal sealed class LaunchAlignmentEvidence
{
    public string Name { get; init; } = "";
    public string Status { get; init; } = "Missing";
    public string EvidencePath { get; init; } = "";
}

internal sealed class LaunchResolution
{
    public string ProjectRoot { get; init; } = "";
    public string ManifestPath { get; init; } = "";
    public SagaProjectManifest? Project { get; init; }
    public LaunchProfile? Profile { get; init; }
    public List<LaunchDiagnostic> Diagnostics { get; init; } = [];
}
