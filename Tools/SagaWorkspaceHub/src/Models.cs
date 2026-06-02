namespace SagaWorkspaceHub;

internal static class ToolInfo
{
    public const string Name = "sagaworkspacehub";
    public const string Version = "0.0.9-dev";
}

internal sealed record Diagnostic
{
    public string Severity { get; init; } = "Info";
    public string Code { get; init; } = "";
    public string Message { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record ProjectSummary
{
    public string ProjectId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string ProjectRoot { get; init; } = "";
    public string ManifestPath { get; init; } = "";
    public int SchemaVersion { get; init; }
}

internal sealed record SliceSummary
{
    public string SliceId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string IntendedRole { get; init; } = "";
    public string SlicePath { get; init; } = "";
    public int SchemaVersion { get; init; }
}

internal sealed record PolicySummary
{
    public string ReportPath { get; init; } = "";
    public string SourceDecision { get; init; } = "";
    public string AdapterDecision { get; init; } = "MissingPolicyEvidence";
    public string OperationDisposition { get; init; } = "ReportOnlyMissingEvidence";
    public bool OperationRejectedBeforeMutation { get; init; } = true;
    public string Action { get; init; } = "";
    public string Resource { get; init; } = "";
    public string Role { get; init; } = "";
    public string Subject { get; init; } = "";
    public string ScopeProjectId { get; init; } = "";
    public string ScopeWorkspaceId { get; init; } = "";
}

internal sealed record CollaborationSummary
{
    public string Mode { get; init; } = "LocalOffline";
    public string Session { get; init; } = "NotStarted";
    public string Presence { get; init; } = "NotEvaluated";
    public string Locks { get; init; } = "NotEvaluated";
    public string Reviews { get; init; } = "NotEvaluated";
}

internal sealed record ReportInputs
{
    public string Project { get; init; } = "";
    public string SliceResolution { get; init; } = "";
    public string PolicyReport { get; init; } = "";
    public string ViewCompatibility { get; init; } = "";
    public string ReviewReport { get; init; } = "";
    public string AuditReport { get; init; } = "";
    public string Request { get; init; } = "";
    public string PresenceReport { get; init; } = "";
    public string PresenceRedaction { get; init; } = "";
    public string TeamRoomStatus { get; init; } = "";
    public string EvidenceRoot { get; init; } = "";
}

internal sealed record ViewSummary
{
    public string ReportPath { get; init; } = "";
    public string View { get; init; } = "";
    public string Decision { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed record ResourceView
{
    public string Kind { get; init; } = "";
    public string Id { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string Status { get; init; } = "";
    public string Visibility { get; init; } = "";
    public string Placeholder { get; init; } = "";
}

internal sealed record ResourceCounts
{
    public int Visible { get; init; }
    public int Restricted { get; init; }
    public int SummaryOnly { get; init; }
    public int OpaqueRestricted { get; init; }
    public int Hidden { get; init; }
}

internal sealed record SliceViewResources
{
    public IReadOnlyList<ResourceView> VisibleResources { get; init; } = Array.Empty<ResourceView>();
    public IReadOnlyList<ResourceView> RestrictedPlaceholders { get; init; } = Array.Empty<ResourceView>();
}

internal sealed record WorkspaceSummaryReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "summarize";
    public string Status { get; init; } = "Passed";
    public ProjectSummary Project { get; init; } = new();
    public SliceSummary Slice { get; init; } = new();
    public PolicySummary Policy { get; init; } = new();
    public CollaborationSummary Collaboration { get; init; } = new();
    public ReportInputs Reports { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record PolicyAdapterReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "policy-adapter";
    public string Status { get; init; } = "MissingEvidence";
    public PolicySummary Policy { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record SliceScopedViewReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "slice-view";
    public string Status { get; init; } = "Passed";
    public ProjectSummary Project { get; init; } = new();
    public SliceSummary Slice { get; init; } = new();
    public PolicySummary Policy { get; init; } = new();
    public ViewSummary View { get; init; } = new();
    public SliceViewResources Resources { get; init; } = new();
    public ResourceCounts Counts { get; init; } = new();
    public ReportInputs Reports { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record AuditEventRecord
{
    public string EventId { get; init; } = "";
    public string EventType { get; init; } = "";
    public string ActorRef { get; init; } = "";
    public string TargetRef { get; init; } = "";
    public int Sequence { get; init; }
    public string Timestamp { get; init; } = "";
    public string PreviousRecordHash { get; init; } = "";
    public string RecordHash { get; init; } = "";
    public IReadOnlyList<string> EvidenceRefs { get; init; } = Array.Empty<string>();
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record AuditSummary
{
    public int EventCount { get; init; }
    public Dictionary<string, int> CountsByEventType { get; init; } = new(StringComparer.Ordinal);
    public string HashChainStatus { get; init; } = "NotSupplied";
}

internal sealed record AuditLogReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "audit-log";
    public string Status { get; init; } = "Passed";
    public IReadOnlyList<AuditEventRecord> Events { get; init; } = Array.Empty<AuditEventRecord>();
    public AuditSummary Summary { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record ReviewMetadata
{
    public string ReviewId { get; init; } = "";
    public string Requester { get; init; } = "";
    public IReadOnlyList<string> Reviewers { get; init; } = Array.Empty<string>();
    public string TargetRef { get; init; } = "";
    public string ExpectedHash { get; init; } = "";
    public string ActualHash { get; init; } = "";
    public IReadOnlyList<string> SourceReportPaths { get; init; } = Array.Empty<string>();
}

internal sealed record AuditEvidenceSummary
{
    public string ReportPath { get; init; } = "";
    public string Status { get; init; } = "NotSupplied";
    public int EventCount { get; init; }
    public Dictionary<string, int> CountsByEventType { get; init; } = new(StringComparer.Ordinal);
}

internal sealed record ReviewApprovalReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "review-approval";
    public string Status { get; init; } = "Blocked";
    public ReviewMetadata Review { get; init; } = new();
    public PolicySummary Policy { get; init; } = new();
    public string Decision { get; init; } = "MissingEvidence";
    public AuditEvidenceSummary Audit { get; init; } = new();
    public ReportInputs Reports { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record ExportRequestMetadata
{
    public string ArtifactId { get; init; } = "";
    public string Path { get; init; } = "";
    public string Visibility { get; init; } = "";
    public string Role { get; init; } = "";
    public string OutputTarget { get; init; } = "";
}

internal sealed record RestrictedExportItem
{
    public string Kind { get; init; } = "";
    public string Id { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string Visibility { get; init; } = "";
    public string Disposition { get; init; } = "";
    public string Placeholder { get; init; } = "";
}

internal sealed record RestrictedExportCounts
{
    public int AllowedExports { get; init; }
    public int BlockedExports { get; init; }
    public int RestrictedArtifacts { get; init; }
}

internal sealed record RestrictedExportReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "restricted-export";
    public string Status { get; init; } = "Blocked";
    public ExportRequestMetadata Request { get; init; } = new();
    public PolicySummary Policy { get; init; } = new();
    public IReadOnlyList<RestrictedExportItem> AllowedExports { get; init; } = Array.Empty<RestrictedExportItem>();
    public IReadOnlyList<RestrictedExportItem> BlockedExports { get; init; } = Array.Empty<RestrictedExportItem>();
    public IReadOnlyList<RestrictedExportItem> RestrictedArtifacts { get; init; } = Array.Empty<RestrictedExportItem>();
    public RestrictedExportCounts Counts { get; init; } = new();
    public ReportInputs Reports { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record PresenceActorView
{
    public string ActorId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string Role { get; init; } = "";
    public string ResourceRef { get; init; } = "";
    public string Placeholder { get; init; } = "";
}

internal sealed record PresenceResourceView
{
    public string ResourceId { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string Visibility { get; init; } = "";
    public string Placeholder { get; init; } = "";
}

internal sealed record PresenceRedactionCounts
{
    public int VisibleActors { get; init; }
    public int RedactedActors { get; init; }
    public int VisibleResources { get; init; }
    public int RedactedResources { get; init; }
}

internal sealed record PresenceRedactionReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "presence-redaction";
    public string Status { get; init; } = "Passed";
    public PolicySummary Policy { get; init; } = new();
    public IReadOnlyList<PresenceActorView> VisibleActors { get; init; } = Array.Empty<PresenceActorView>();
    public IReadOnlyList<PresenceActorView> RedactedActors { get; init; } = Array.Empty<PresenceActorView>();
    public IReadOnlyList<PresenceResourceView> VisibleResources { get; init; } = Array.Empty<PresenceResourceView>();
    public IReadOnlyList<PresenceResourceView> RedactedResources { get; init; } = Array.Empty<PresenceResourceView>();
    public PresenceRedactionCounts Counts { get; init; } = new();
    public ReportInputs Reports { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record PolicyActionDescriptor
{
    public string ActionId { get; init; } = "";
    public string Status { get; init; } = "Disabled";
    public string Reason { get; init; } = "";
    public string SourceMutationOwner { get; init; } = "None";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record PolicyActionsReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "policy-actions";
    public string Status { get; init; } = "Passed";
    public PolicySummary Policy { get; init; } = new();
    public IReadOnlyList<PolicyActionDescriptor> Actions { get; init; } = Array.Empty<PolicyActionDescriptor>();
    public ReportInputs Reports { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record TeamRoomItem
{
    public string Id { get; init; } = "";
    public string AuthorRef { get; init; } = "";
    public string TargetRef { get; init; } = "";
    public string Summary { get; init; } = "";
}

internal sealed record TeamRoomCounts
{
    public int VisibleActors { get; init; }
    public int RedactedActors { get; init; }
    public int VisibleResources { get; init; }
    public int RedactedResources { get; init; }
    public int VisibleReviews { get; init; }
    public int RestrictedReviews { get; init; }
    public int VisibleComments { get; init; }
    public int RestrictedComments { get; init; }
    public int HiddenResources { get; init; }
}

internal sealed record TeamRoomReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "team-room";
    public string Status { get; init; } = "Passed";
    public PolicySummary Policy { get; init; } = new();
    public IReadOnlyList<PresenceActorView> VisibleActors { get; init; } = Array.Empty<PresenceActorView>();
    public IReadOnlyList<PresenceResourceView> VisibleResources { get; init; } = Array.Empty<PresenceResourceView>();
    public IReadOnlyList<TeamRoomItem> VisibleReviews { get; init; } = Array.Empty<TeamRoomItem>();
    public IReadOnlyList<TeamRoomItem> VisibleComments { get; init; } = Array.Empty<TeamRoomItem>();
    public TeamRoomCounts Counts { get; init; } = new();
    public ReportInputs Reports { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}

internal sealed record EvidenceSummary
{
    public string Kind { get; init; } = "";
    public string Path { get; init; } = "";
    public string Status { get; init; } = "";
    public string Decision { get; init; } = "";
    public string Summary { get; init; } = "";
}

internal sealed record GovernancePanelCounts
{
    public int PolicyDecisions { get; init; }
    public int DangerousOperations { get; init; }
    public int ReviewApprovals { get; init; }
    public int PublishGates { get; init; }
    public int AuditLogs { get; init; }
    public int RestrictedExports { get; init; }
    public int RedactionReports { get; init; }
    public int TeamRoomReports { get; init; }
    public int MissingEvidence { get; init; }
    public int ResidualDebt { get; init; }
}

internal sealed record GovernancePanelReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string Command { get; init; } = "governance-panel";
    public string Status { get; init; } = "Passed";
    public string Model { get; init; } = "LocalGovernanceDashboardModel";
    public string NonClaim { get; init; } = "Not an admin console, auth UI, RBAC UI, security management UI, or enforcement surface.";
    public IReadOnlyList<EvidenceSummary> PolicyDecisions { get; init; } = Array.Empty<EvidenceSummary>();
    public IReadOnlyList<EvidenceSummary> DangerousOperationQueue { get; init; } = Array.Empty<EvidenceSummary>();
    public IReadOnlyList<EvidenceSummary> ReviewApprovals { get; init; } = Array.Empty<EvidenceSummary>();
    public IReadOnlyList<EvidenceSummary> PublishGates { get; init; } = Array.Empty<EvidenceSummary>();
    public IReadOnlyList<EvidenceSummary> AuditLogSummaries { get; init; } = Array.Empty<EvidenceSummary>();
    public IReadOnlyList<EvidenceSummary> RestrictedExports { get; init; } = Array.Empty<EvidenceSummary>();
    public IReadOnlyList<EvidenceSummary> RedactionReports { get; init; } = Array.Empty<EvidenceSummary>();
    public IReadOnlyList<EvidenceSummary> TeamRoomSummaries { get; init; } = Array.Empty<EvidenceSummary>();
    public IReadOnlyList<EvidenceSummary> ResidualDebt { get; init; } = Array.Empty<EvidenceSummary>();
    public IReadOnlyList<EvidenceSummary> MissingEvidence { get; init; } = Array.Empty<EvidenceSummary>();
    public GovernancePanelCounts Counts { get; init; } = new();
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
    public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = Array.Empty<Diagnostic>();
}
