using System.Text.Json.Serialization;

namespace SagaViewKit;

internal static class ToolInfo
{
    public const string Name = "sagaviewkit";
    public const string Version = "0.0.9-dev";
}

internal sealed record EvidenceArtifact
{
    public string Kind { get; init; } = "";
    public string Path { get; init; } = "";
    public bool Required { get; init; } = true;
}

internal sealed record ViewCapabilityManifest
{
    public int SchemaVersion { get; init; } = 1;
    public string ViewId { get; init; } = "";
    public string ViewKind { get; init; } = "";
    public IReadOnlyList<string> AllowedApiDomains { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> AllowedApiLevels { get; init; } = Array.Empty<string>();
    public bool CanShowOpaqueNodes { get; init; }
    public bool MustDiscloseOpaqueNodes { get; init; }
    public bool CanEditSupportedNodes { get; init; }
    public bool CanEditUnsupportedNodes { get; init; }
    public bool CanApplyPatch { get; init; }
    public bool CanMutateSource { get; init; }
    public bool CanRegenerateSource { get; init; }
    public bool CanShowSourceLinks { get; init; }
    public bool CanShowDiagnostics { get; init; }
    public bool CanShowCollaborationMetadata { get; init; }
    public IReadOnlyList<EvidenceArtifact> RequiredEvidenceArtifacts { get; init; } = Array.Empty<EvidenceArtifact>();
}

internal sealed record Diagnostic
{
    public string Severity { get; init; } = "Info";
    public string Code { get; init; } = "";
    public string Message { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record Violation
{
    public string Code { get; init; } = "";
    public string Message { get; init; } = "";
    public string ViewId { get; init; } = "";
    public string NodeId { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record ProfileSummary
{
    public string ViewId { get; init; } = "";
    public string ViewKind { get; init; } = "";
    public IReadOnlyList<string> AllowedApiDomains { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> AllowedApiLevels { get; init; } = Array.Empty<string>();
    public bool CanEditUnsupportedNodes { get; init; }
    public bool CanApplyPatch { get; init; }
    public bool CanMutateSource { get; init; }
    public bool CanRegenerateSource { get; init; }
}

internal sealed record ViewCapabilityReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "";
    public string Status { get; init; } = "Passed";
    public IReadOnlyList<ProfileSummary> ProfileSummaries { get; init; } = Array.Empty<ProfileSummary>();
    public IReadOnlyList<Violation> Violations { get; init; } = Array.Empty<Violation>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record SimpleViewHonestyReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "check-simple";
    public string Status { get; init; } = "Passed";
    public string CheckedProjection { get; init; } = "";
    public ProfileSummary ViewProfile { get; init; } = new();
    public IReadOnlyList<Violation> Violations { get; init; } = Array.Empty<Violation>();
    public int OpaqueNodeCount { get; init; }
    public int UnsupportedNodeCount { get; init; }
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record ViewSliceCompatibilityReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "slice-compatibility";
    public string Status { get; init; } = "Passed";
    public string View { get; init; } = "";
    public string SliceResolution { get; init; } = "";
    public string Decision { get; init; } = "Compatible";
    public int VisibleCount { get; init; }
    public int RestrictedCount { get; init; }
    public int HiddenCount { get; init; }
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record ProjectionNode(
    string NodeId,
    string Kind,
    string ApiLevel,
    string ApiDomain,
    string Capability,
    string ProjectionCompatibility,
    bool ReadOnly,
    string OpaqueReason,
    string BehaviorCompatibility);

internal sealed record MetadataNode(
    string NodeId,
    string Level,
    string Domain,
    string Capability,
    string ProjectionCompatibility,
    bool ReadOnly);

internal sealed record ValidationResult(
    bool Passed,
    IReadOnlyList<Violation> Violations,
    IReadOnlyList<Diagnostic> Diagnostics);
