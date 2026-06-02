namespace SagaDocGuard;

internal static class ToolInfo
{
    public const string Name = "sagadocguard";
    public const string Version = "0.0.9-dev";
}

internal sealed record Diagnostic
{
    public string Severity { get; init; } = "Info";
    public string Code { get; init; } = "";
    public string Message { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record ClaimMatch
{
    public string Category { get; init; } = "";
    public string RuleId { get; init; } = "";
    public string Path { get; init; } = "";
    public int Line { get; init; }
    public string Text { get; init; } = "";
}

internal sealed record MissingEvidence
{
    public string EvidenceId { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record MissingRequiredNonClaim
{
    public string NonClaimId { get; init; } = "";
    public string ExpectedPattern { get; init; } = "";
}

internal sealed record DocGuardReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "scan";
    public string Status { get; init; } = "Passed";
    public IReadOnlyList<string> ScannedFiles { get; init; } = Array.Empty<string>();
    public IReadOnlyList<ClaimMatch> ForbiddenMatches { get; init; } = Array.Empty<ClaimMatch>();
    public IReadOnlyList<MissingRequiredNonClaim> MissingRequiredNonClaims { get; init; } = Array.Empty<MissingRequiredNonClaim>();
    public IReadOnlyList<MissingEvidence> MissingEvidence { get; init; } = Array.Empty<MissingEvidence>();
    public IReadOnlyList<ClaimMatch> ReviewedNonClaimMatches { get; init; } = Array.Empty<ClaimMatch>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record ClaimRule(string RuleId, string Category, string Pattern);
internal sealed record RequiredNonClaimRule(string NonClaimId, string Pattern);
internal sealed record EvidenceRule(string EvidenceId, string Path);
