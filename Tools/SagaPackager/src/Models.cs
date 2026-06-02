namespace SagaPackager;

internal sealed class SagaProjectManifest
{
    public int SchemaVersion { get; init; }
    public string ProjectId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public ProjectPaths Paths { get; init; } = new();
    public List<ProjectAssetReference> Assets { get; init; } = [];
    public List<ProjectPathReference> LaunchProfiles { get; init; } = [];
    public List<ProjectPathReference> PackageProfiles { get; init; } = [];
}

internal sealed class ProjectPaths
{
    public string Diagnostics { get; init; } = "";
    public string GeneratedReports { get; init; } = "";
}

internal sealed class ProjectPathReference
{
    public string Id { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed class ProjectAssetReference
{
    public string Id { get; init; } = "";
    public string Path { get; init; } = "";
    public string Kind { get; init; } = "";
}

internal sealed class PackageProfilesDocument
{
    public int SchemaVersion { get; init; }
    public List<PackageProfile> Profiles { get; init; } = [];
}

internal sealed class PackageProfile
{
    public string Id { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string Role { get; init; } = "";
    public string OutputDirectory { get; init; } = "";
    public string DiagnosticsDirectory { get; init; } = "";
    public string ReportsDirectory { get; init; } = "";
    public string PackageManifestPath { get; init; } = "";
    public string AssetManifestPath { get; init; } = "";
    public string AssetIdentityManifestPath { get; init; } = "";
    public string ScriptMetadataPath { get; init; } = "";
    public string RuntimeMetadataPath { get; init; } = "";
    public string LaunchProfileId { get; init; } = "";
}

internal sealed class PackageDiagnostic
{
    public string Severity { get; init; } = "Error";
    public string Code { get; init; } = "";
    public string Category { get; init; } = "Package";
    public string Path { get; init; } = "";
    public string Message { get; init; } = "";
}

internal sealed class ProjectSummary
{
    public string ProjectId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string ProjectRoot { get; init; } = "";
    public string ManifestPath { get; init; } = "";
}

internal sealed class PackageProfileSummary
{
    public string Id { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string Role { get; init; } = "";
    public string OutputDirectory { get; init; } = "";
    public string PackageManifestPath { get; init; } = "";
    public string LaunchProfileId { get; init; } = "";
}

internal sealed class CheckedInput
{
    public string Kind { get; init; } = "";
    public string Path { get; init; } = "";
    public bool Exists { get; init; }
    public string Status { get; init; } = "";
}

internal sealed class StagedFile
{
    public string Path { get; init; } = "";
    public long SizeBytes { get; init; }
}

internal sealed class PackageReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagapack";
    public string Command { get; init; } = "";
    public string Status { get; init; } = "Failed";
    public ProjectSummary Project { get; init; } = new();
    public PackageProfileSummary PackageProfile { get; init; } = new();
    public List<CheckedInput> CheckedInputs { get; init; } = [];
    public string StagedPackagePath { get; init; } = "";
    public string PackageManifestPath { get; init; } = "";
    public string StageManifestPath { get; init; } = "";
    public List<StagedFile> StagedFiles { get; init; } = [];
    public List<PackageDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class AssetReferenceCheck
{
    public string Id { get; init; } = "";
    public string Kind { get; init; } = "";
    public string Path { get; init; } = "";
    public bool Exists { get; init; }
    public string PlaceholderStatus { get; init; } = "";
}

internal sealed class PackageAssetEvidence
{
    public string PackageReportPath { get; init; } = "";
    public string StageStatus { get; init; } = "NotSupplied";
    public string AssetManifestPath { get; init; } = "";
    public bool AssetManifestExists { get; init; }
    public string AssetIdentityManifestPath { get; init; } = "";
    public bool AssetIdentityManifestExists { get; init; }
}

internal sealed class AssetWorkflowReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagapack";
    public string Command { get; init; } = "asset-validate";
    public string Status { get; init; } = "Failed";
    public ProjectSummary Project { get; init; } = new();
    public List<AssetReferenceCheck> AssetReferences { get; init; } = [];
    public PackageAssetEvidence PackageEvidence { get; init; } = new();
    public List<PackageDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class PackageProfileMatrixRow
{
    public string ProfileId { get; init; } = "";
    public string Role { get; init; } = "";
    public string LaunchProfileId { get; init; } = "";
    public string ValidationStatus { get; init; } = "Failed";
    public string StageSupport { get; init; } = "Unsupported";
    public string PublishCheckEvidenceStatus { get; init; } = "EvidenceNotSupplied";
    public List<PackageDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class PackageProfileMatrixReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagapack";
    public string Command { get; init; } = "profile-matrix";
    public string Status { get; init; } = "Failed";
    public ProjectSummary Project { get; init; } = new();
    public List<PackageProfileMatrixRow> Profiles { get; init; } = [];
    public List<PackageDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class PackageSourceTruthAlignmentReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagapack";
    public string Command { get; init; } = "source-truth-alignment";
    public string Status { get; init; } = "Failed";
    public ProjectSummary Project { get; init; } = new();
    public List<PackageProfileSummary> PackageProfiles { get; init; } = [];
    public List<string> ReferencedLaunchProfileIds { get; init; } = [];
    public GateResult SourceTruthGate { get; init; } = new();
    public GateResult AssetReferenceGate { get; init; } = new();
    public List<PackageManifestCanonicalRejection> PackageGeneratedManifestCanonicalRejections { get; init; } = [];
    public List<PackageDiagnostic> Diagnostics { get; init; } = [];
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal sealed class PackageManifestCanonicalRejection
{
    public string ProfileId { get; init; } = "";
    public string PackageManifestPath { get; init; } = "";
    public string AssetManifestPath { get; init; } = "";
    public string Reason { get; init; } = "";
}

internal sealed class GateResult
{
    public string Name { get; init; } = "";
    public string Status { get; init; } = "Failed";
    public string EvidencePath { get; init; } = "";
    public string Message { get; init; } = "";
}

internal sealed class PublishReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagapack";
    public string Command { get; init; } = "publish-check";
    public string Status { get; init; } = "Failed";
    public ProjectSummary Project { get; init; } = new();
    public PackageProfileSummary PackageProfile { get; init; } = new();
    public List<CheckedInput> RequiredEvidence { get; init; } = [];
    public List<GateResult> Gates { get; init; } = [];
    public List<PackageDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class ProcessInfo
{
    public string Executable { get; init; } = "";
    public List<string> Arguments { get; init; } = [];
    public string WorkingDirectory { get; init; } = "";
    public int? ExitCode { get; init; }
    public bool TimedOut { get; init; }
    public long DurationMs { get; init; }
}

internal sealed class DeferredStage
{
    public string Name { get; init; } = "";
    public string Status { get; init; } = "Deferred";
    public string Reason { get; init; } = "";
}

internal sealed class PackageSmokeReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagapack";
    public string Command { get; init; } = "smoke";
    public string Status { get; init; } = "Failed";
    public PackageProfileSummary PackageProfile { get; init; } = new();
    public string StagedPackagePath { get; init; } = "";
    public ProcessInfo Process { get; init; } = new();
    public List<string> DiagnosticsReportPaths { get; init; } = [];
    public List<DeferredStage> DeferredStages { get; init; } = [];
    public List<PackageDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class ProjectResolution
{
    public string ProjectRoot { get; init; } = "";
    public string ManifestPath { get; init; } = "";
    public SagaProjectManifest? Project { get; init; }
    public PackageProfile? Profile { get; init; }
    public string PackageProfilesPath { get; init; } = "";
    public List<PackageDiagnostic> Diagnostics { get; init; } = [];
}
