namespace SagaEnterpriseGate;

internal static class ToolInfo
{
    public const string Name = "sagaenterprisegate";
    public const string Version = "0.0.9-dev";
}

internal sealed record Diagnostic
{
    public string Severity { get; init; } = "Info";
    public string Code { get; init; } = "";
    public string Message { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record RegressionCheck
{
    public string CheckId { get; init; } = "";
    public string Status { get; init; } = "MissingEvidence";
    public string EvidencePath { get; init; } = "";
    public string Summary { get; init; } = "";
}

internal sealed record RegressionSummary
{
    public int Passed { get; init; }
    public int Failed { get; init; }
    public int MissingEvidence { get; init; }
    public int Total { get; init; }
}

internal sealed record PolicyRegressionReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "policy-regression";
    public string Status { get; init; } = "Failed";
    public IReadOnlyList<RegressionCheck> Checks { get; init; } = Array.Empty<RegressionCheck>();
    public RegressionSummary Summary { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record EvidenceCategory
{
    public string Category { get; init; } = "";
    public string Status { get; init; } = "MissingEvidence";
    public string EvidencePath { get; init; } = "";
    public string Summary { get; init; } = "";
}

internal sealed record GateSummary
{
    public int Passed { get; init; }
    public int PartiallyProven { get; init; }
    public int Deferred { get; init; }
    public int MissingEvidence { get; init; }
    public int Blocked { get; init; }
    public int NotApplicable { get; init; }
    public int Total { get; init; }
}

internal sealed record EvidenceGateReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "evidence-gate";
    public string Status { get; init; } = "MissingEvidence";
    public IReadOnlyList<EvidenceCategory> Categories { get; init; } = Array.Empty<EvidenceCategory>();
    public GateSummary Summary { get; init; } = new();
    public IReadOnlyList<string> ResidualDebt { get; init; } = Array.Empty<string>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal sealed record AcceptanceStep
{
    public string StepId { get; init; } = "";
    public string Status { get; init; } = "MissingEvidence";
    public string EvidencePath { get; init; } = "";
    public string Summary { get; init; } = "";
}

internal sealed record AcceptanceSummary
{
    public int Passed { get; init; }
    public int PartiallyProven { get; init; }
    public int MissingEvidence { get; init; }
    public int Blocked { get; init; }
    public int Total { get; init; }
}

internal sealed record AcceptanceScenarioReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "acceptance-scenario";
    public string Status { get; init; } = "MissingEvidence";
    public IReadOnlyList<AcceptanceStep> Steps { get; init; } = Array.Empty<AcceptanceStep>();
    public AcceptanceSummary Summary { get; init; } = new();
    public IReadOnlyList<string> ResidualDebt { get; init; } = Array.Empty<string>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal sealed record ClosureCheckReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "closure-check";
    public string Outcome { get; init; } = "Blocked";
    public string AllowedClaim { get; init; } = "Enterprise-evolvable foundation established with focused local/report-only evidence.";
    public IReadOnlyList<EvidenceCategory> EvidenceMatrix { get; init; } = Array.Empty<EvidenceCategory>();
    public GateSummary Summary { get; init; } = new();
    public IReadOnlyList<string> ResidualDebt { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> BlockedClaims { get; init; } = Array.Empty<string>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}
