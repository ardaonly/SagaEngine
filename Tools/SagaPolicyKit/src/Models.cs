namespace SagaPolicyKit;

internal static class ToolInfo
{
    public const string Name = "sagapolicy";
    public const string Version = "0.0.9-dev";
}

internal sealed record Diagnostic
{
    public string Severity { get; init; } = "Info";
    public string Code { get; init; } = "";
    public string Message { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record ScopeInfo
{
    public string ProjectId { get; init; } = "";
    public string WorkspaceId { get; init; } = "";
}

internal sealed record EvidenceReference
{
    public string EvidenceId { get; init; } = "";
    public string Path { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed record PolicyEvaluationReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "evaluate";
    public string Status { get; init; } = "Passed";
    public string Decision { get; init; } = "NotApplicable";
    public string Subject { get; init; } = "";
    public string Resource { get; init; } = "";
    public string Action { get; init; } = "";
    public string Role { get; init; } = "";
    public ScopeInfo Scope { get; init; } = new();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
    public IReadOnlyList<EvidenceReference> Evidence { get; init; } = Array.Empty<EvidenceReference>();
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal sealed record PolicyRole
{
    public string Id { get; init; } = "";
    public IReadOnlyList<string> Allow { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> Deny { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> Warn { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> RequiresReview { get; init; } = Array.Empty<string>();
}

internal sealed record PolicyResource
{
    public string Id { get; init; } = "";
    public string Kind { get; init; } = "";
}

internal sealed record RequiredEvidenceRule
{
    public string Action { get; init; } = "";
    public string Resource { get; init; } = "";
    public string EvidenceId { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record PolicyDefinition
{
    public IReadOnlyList<PolicyRole> Roles { get; init; } = Array.Empty<PolicyRole>();
    public IReadOnlyList<PolicyResource> Resources { get; init; } = Array.Empty<PolicyResource>();
    public IReadOnlyList<string> DangerousOperations { get; init; } = Array.Empty<string>();
    public IReadOnlyList<RequiredEvidenceRule> RequiredEvidence { get; init; } = Array.Empty<RequiredEvidenceRule>();
}

internal sealed record PolicyRequest
{
    public string Subject { get; init; } = "";
    public string Actor { get; init; } = "";
    public string Role { get; init; } = "";
    public string Action { get; init; } = "";
    public string Resource { get; init; } = "";
    public ScopeInfo Scope { get; init; } = new();
    public IReadOnlyList<EvidenceReference> Evidence { get; init; } = Array.Empty<EvidenceReference>();
}
