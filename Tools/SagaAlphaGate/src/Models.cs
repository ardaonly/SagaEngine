namespace SagaAlphaGate;

internal static class ToolInfo
{
    public const string Name = "sagaalphagate";
    public const string Version = "0.0.9-dev";
}

internal sealed record Diagnostic
{
    public string Severity { get; init; } = "Info";
    public string Code { get; init; } = "";
    public string Message { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record EvidenceCheck
{
    public string Id { get; init; } = "";
    public string Phase { get; init; } = "";
    public string Title { get; init; } = "";
    public string Path { get; init; } = "";
    public string Status { get; init; } = "Missing";
    public bool Required { get; init; } = true;
}

internal sealed record OpeningReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "opening-check";
    public string Status { get; init; } = "Blocked";
    public string TechnicalPreviewRoot { get; init; } = "";
    public IReadOnlyList<EvidenceCheck> Phase65Evidence { get; init; } = Array.Empty<EvidenceCheck>();
    public IReadOnlyList<string> AllowedAlphaClaims { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> BlockedAlphaClaims { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> ResidualTechnicalPreviewDebt { get; init; } = Array.Empty<string>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record AcceptanceScenarioStep
{
    public int Sequence { get; init; }
    public string Id { get; init; } = "";
    public string Description { get; init; } = "";
    public string Classification { get; init; } = "Missing";
    public string RequiredEvidence { get; init; } = "";
    public string RoadmapPhase { get; init; } = "";
}

internal sealed record AcceptancePlanReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "acceptance-plan";
    public string Status { get; init; } = "Passed";
    public IReadOnlyList<AcceptanceScenarioStep> ScenarioMatrix { get; init; } = Array.Empty<AcceptanceScenarioStep>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record BudgetEntry
{
    public string Id { get; init; } = "";
    public string Workflow { get; init; } = "";
    public int BudgetMs { get; init; }
    public int? MeasuredMs { get; init; }
    public string Status { get; init; } = "MeasurementMissing";
    public string EvidencePath { get; init; } = "";
}

internal sealed record BudgetReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "budget-report";
    public string Status { get; init; } = "PassedWithMissingMeasurements";
    public IReadOnlyList<BudgetEntry> Budgets { get; init; } = Array.Empty<BudgetEntry>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record ScriptRuntimeProofSummary
{
    public int RuntimeBackedWithEvidence { get; init; }
    public int RuntimeBackedMissingEvidence { get; init; }
    public int ProjectionOnly { get; init; }
    public int Deferred { get; init; }
}

internal sealed record ScriptEvidenceReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "script-evidence";
    public string Status { get; init; } = "Blocked";
    public string ScriptValidationPath { get; init; } = "";
    public string ScriptValidationStatus { get; init; } = "Missing";
    public ScriptRuntimeProofSummary RuntimeProofSummary { get; init; } = new();
    public IReadOnlyList<string> BlockedClaims { get; init; } = Array.Empty<string>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record GameplayExpansionBlocker
{
    public string Id { get; init; } = "";
    public string Scope { get; init; } = "";
    public string Status { get; init; } = "Deferred";
    public string RequiredSeam { get; init; } = "";
    public string Reason { get; init; } = "";
}

internal sealed record GameplayExpansionBlockerReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "gameplay-expansion-blockers";
    public string Status { get; init; } = "Deferred";
    public string RoadmapPhase { get; init; } = "Phase 86";
    public string RoadmapTitle { get; init; } = "MultiplayerSandbox Gameplay Expansion v1";
    public IReadOnlyList<GameplayExpansionBlocker> Blockers { get; init; } = Array.Empty<GameplayExpansionBlocker>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record WorkflowEvidenceReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "";
    public string Status { get; init; } = "Passed";
    public string ReportKind { get; init; } = "";
    public string PhaseRange { get; init; } = "";
    public string EvidenceScope { get; init; } = "";
    public IReadOnlyList<string> Documentation { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> FocusedTests { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> BlockedClaims { get; init; } = Array.Empty<string>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record AlphaEvidenceItem
{
    public string Id { get; init; } = "";
    public string Block { get; init; } = "";
    public string PhaseRange { get; init; } = "";
    public string Category { get; init; } = "";
    public string Title { get; init; } = "";
    public string Path { get; init; } = "";
    public bool Required { get; init; } = true;
    public string Status { get; init; } = "MissingEvidence";
    public string Confidence { get; init; } = "MissingEvidence";
    public string Classification { get; init; } = "Evidence";
    public string Message { get; init; } = "";
}

internal sealed record AlphaAcceptanceCategory
{
    public string Category { get; init; } = "";
    public string Status { get; init; } = "MissingEvidence";
    public IReadOnlyList<string> EvidenceIds { get; init; } = Array.Empty<string>();
}

internal sealed record SmallTeamAlphaAcceptanceReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "accept-alpha";
    public string Status { get; init; } = "Blocked";
    public string EvidenceRoot { get; init; } = "";
    public IReadOnlyList<AlphaAcceptanceCategory> Categories { get; init; } = Array.Empty<AlphaAcceptanceCategory>();
    public IReadOnlyList<AlphaEvidenceItem> Evidence { get; init; } = Array.Empty<AlphaEvidenceItem>();
    public IReadOnlyList<string> AllowedAlphaClaims { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> BlockedAlphaClaims { get; init; } = Array.Empty<string>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record SmallTeamAlphaEvidenceMatrixReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "evidence-matrix";
    public string Status { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public IReadOnlyList<AlphaEvidenceItem> Matrix { get; init; } = Array.Empty<AlphaEvidenceItem>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record SmallTeamAlphaClosureReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "close-alpha";
    public string Status { get; init; } = "RejectedOrBlocked";
    public string EvidenceRoot { get; init; } = "";
    public string Decision { get; init; } = "Small-Team Alpha rejected / blocked";
    public IReadOnlyList<AlphaEvidenceItem> ImplementedMatrix { get; init; } = Array.Empty<AlphaEvidenceItem>();
    public IReadOnlyList<AlphaEvidenceItem> EvidenceMatrix { get; init; } = Array.Empty<AlphaEvidenceItem>();
    public IReadOnlyList<string> KnownDebt { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> BlockedClaims { get; init; } = Array.Empty<string>();
    public string Hedef3OpeningRecommendation { get; init; } = "";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}
