using System.Text.Json.Serialization;

namespace SagaProjectKit;

internal sealed class SagaProjectDescriptor
{
    public int SchemaVersion { get; init; }
    public string ProjectId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public EngineCompatibility EngineCompatibility { get; init; } = new();
    public ProjectPaths Paths { get; init; } = new();
    public List<ProjectPathReference> Scenes { get; init; } = [];
    public List<ProjectPathReference> Assets { get; init; } = [];
    public List<ProjectPathReference> ScriptFolders { get; init; } = [];
    public List<ProjectPathReference> LaunchProfiles { get; init; } = [];
    public List<ProjectPathReference> PackageProfiles { get; init; } = [];
}

internal sealed class EngineCompatibility
{
    public string MinimumVersion { get; init; } = "";
    public string TargetVersion { get; init; } = "";
    public string Channel { get; init; } = "";
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
    public string Kind { get; init; } = "";
}

internal sealed class ProjectDiagnostic
{
    public string Severity { get; init; } = "Error";
    public string Code { get; init; } = "";
    public string Category { get; init; } = "Project";
    public string Path { get; init; } = "";
    public string Message { get; init; } = "";
}

internal sealed class ProjectSliceDefinition
{
    public int SchemaVersion { get; init; }
    public string SliceId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string Description { get; init; } = "";
    public string IntendedRole { get; init; } = "";
    public List<ProjectSliceResource> IncludedResources { get; init; } = [];
    public List<ProjectSliceResource> ExcludedResources { get; init; } = [];
    public List<ProjectSliceResource> VisibilityRules { get; init; } = [];
    public List<ProjectDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class ProjectSliceResource
{
    public string Kind { get; init; } = "";
    public string Id { get; init; } = "";
    public string Path { get; init; } = "";
    public string Visibility { get; init; } = "";
}

internal sealed class ProjectSliceSummary
{
    public string SliceId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string IntendedRole { get; init; } = "";
    public string SlicePath { get; init; } = "";
    public int SchemaVersion { get; init; }
}

internal sealed class SliceResolvedResource
{
    public string Kind { get; init; } = "";
    public string Id { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string Status { get; init; } = "";
    public string Visibility { get; init; } = "";
    public string Placeholder { get; init; } = "";
    public bool Exists { get; init; }
}

internal sealed class ProjectSummary
{
    public string ProjectId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public string ProjectRoot { get; init; } = "";
    public string ManifestPath { get; init; } = "";
    public int SchemaVersion { get; init; }
}

internal class ProjectReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagaproject";
    public string Command { get; init; } = "";
    public string Status { get; init; } = "Failed";
    public ProjectSummary Project { get; init; } = new();
    public List<ProjectDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed class ProjectResolutionReport : ProjectReport
{
    public List<ResolvedProjectPath> ResolvedPaths { get; init; } = [];
}

internal sealed class ProjectSliceResolutionReport : ProjectReport
{
    public ProjectSliceSummary Slice { get; init; } = new();
    public List<SliceResolvedResource> VisibleArtifacts { get; init; } = [];
    public List<SliceResolvedResource> RestrictedArtifacts { get; init; } = [];
    public List<SliceResolvedResource> MissingArtifacts { get; init; } = [];
    public List<SliceResolvedResource> ExcludedArtifacts { get; init; } = [];
    public List<SliceResolvedResource> ResourceVisibility { get; init; } = [];
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal sealed class RestrictedProjectResolutionReport : ProjectReport
{
    public ProjectSliceSummary Slice { get; init; } = new();
    public List<SliceResolvedResource> ResolvedResources { get; init; } = [];
    public List<SliceResolvedResource> RestrictedResources { get; init; } = [];
    public List<SliceResolvedResource> ResourceVisibility { get; init; } = [];
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal sealed class SourceTruthInventoryReport : ProjectReport
{
    public SourceTruthSummary Summary { get; init; } = new();
    public List<SceneSourceTruthItem> Scenes { get; init; } = [];
    public List<EntitySourceTruthItem> Entities { get; init; } = [];
    public List<AssetReferenceTruthItem> AssetReferences { get; init; } = [];
    public List<GeneratedArtifactTruthItem> GeneratedArtifacts { get; init; } = [];
    public List<ReadBoundaryTruthItem> ReadBoundaries { get; init; } = [];
    public List<string> DiagnosticRules { get; init; } = [];
    public List<string> ResidualDebt { get; init; } = [];
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal sealed class SourceTruthGateReport : ProjectReport
{
    public SourceTruthSummary Summary { get; init; } = new();
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<string> ResidualDebt { get; init; } = [];
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal sealed class AssetWorkflowOpeningReport : ProjectReport
{
    public string Outcome { get; init; } = "PartiallyProven";
    public List<string> OpenedResponsibilities { get; init; } = [];
    public List<string> ReservedFollowUps { get; init; } = [];
    public string InventoryStatus { get; init; } = "";
    public string GateStatus { get; init; } = "";
    public List<string> Docs { get; init; } = [];
    public List<string> Reports { get; init; } = [];
    public List<string> ResidualDebt { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal abstract class ReportOnlyProjectReport : ProjectReport
{
    public bool LocalOnly { get; init; } = true;
    public string NetworkExposure { get; init; } = "None";
    public bool MutatesSource { get; init; }
    public string Enforcement { get; init; } = "ReportOnly";
}

internal sealed class AssetSourceManifestInventoryReport : ReportOnlyProjectReport
{
    public List<AssetRootInventoryItem> AssetRoots { get; init; } = [];
    public List<AssetSourceManifestItem> AssetSourceManifests { get; init; } = [];
    public List<AssetOwnerInventoryItem> AssetOwners { get; init; } = [];
    public List<AssetReferenceTruthItem> AssetReferences { get; init; } = [];
    public List<MissingAssetOwnerItem> MissingOwners { get; init; } = [];
    public List<GeneratedAssetArtifactItem> GeneratedArtifacts { get; init; } = [];
}

internal sealed class AssetReferenceGateReport : ReportOnlyProjectReport
{
    public List<ResolvedAssetReferenceItem> ResolvedReferences { get; init; } = [];
    public List<UnresolvedAssetReferenceItem> UnresolvedReferences { get; init; } = [];
    public List<MissingAssetOwnerItem> MissingOwners { get; init; } = [];
    public List<RejectedGeneratedOwnerItem> RejectedGeneratedOwners { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
}

internal sealed class GeneratedArtifactFreshnessReport : ReportOnlyProjectReport
{
    public List<GeneratedFreshnessArtifactItem> GeneratedArtifacts { get; init; } = [];
    public List<GeneratedFreshnessSourceInputItem> SourceInputs { get; init; } = [];
    public List<GeneratedFreshnessStatusItem> FreshnessStatus { get; init; } = [];
}

internal sealed class SceneEntityValidationReport : ReportOnlyProjectReport
{
    public List<SceneSchemaValidationItem> Scenes { get; init; } = [];
    public List<EntitySchemaValidationItem> Entities { get; init; } = [];
    public List<AssetReferenceTruthItem> AssetReferences { get; init; } = [];
    public List<GeneratedArtifactTruthItem> GeneratedArtifacts { get; init; } = [];
    public List<SourceTruthGateCheck> SchemaChecks { get; init; } = [];
}

internal sealed class ComponentMetadataOwnershipReport : ReportOnlyProjectReport
{
    public List<ComponentMetadataOwnershipItem> Components { get; init; } = [];
    public List<GeneratedEvidenceOwnershipItem> GeneratedEvidence { get; init; } = [];
    public List<GeneratedFreshnessStatusItem> FreshnessDebt { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
}

internal sealed class EditorInspectionModelReport : ReportOnlyProjectReport
{
    public bool EditorMayMutate { get; init; }
    public List<string> ProhibitedActions { get; init; } = [];
    public List<EditorInspectionSceneItem> Scenes { get; init; } = [];
    public List<EditorInspectionEntityItem> Entities { get; init; } = [];
    public List<ComponentMetadataOwnershipItem> Components { get; init; } = [];
    public List<AssetReferenceTruthItem> AssetReferences { get; init; } = [];
    public List<GeneratedFreshnessStatusItem> FreshnessStatus { get; init; } = [];
}

internal sealed class EditorSourceTruthReadReport : ReportOnlyProjectReport
{
    public bool EditorMayMutate { get; init; }
    public List<EditorSourceTruthReadItem> ReadSources { get; init; } = [];
    public List<GeneratedEvidenceOwnershipItem> RejectedGeneratedCanonicalClaims { get; init; } = [];
    public List<string> ProhibitedActions { get; init; } = [];
}

internal sealed class RuntimeReadModelAuditReport : ReportOnlyProjectReport
{
    public List<RuntimeReadInputItem> AcceptedInputs { get; init; } = [];
    public List<RuntimeReadInputItem> NonCanonicalGeneratedProjections { get; init; } = [];
    public List<RuntimeForbiddenSourceTruthItem> ForbiddenRuntimeSourceTruth { get; init; } = [];
    public List<GeneratedFreshnessStatusItem> FreshnessStatus { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
}

internal sealed class RuntimeReadinessGateReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string PlanningScope { get; init; } = "Planning allowed only; Runtime not implemented.";
    public string RuntimeImplementationClaim { get; init; } = "Runtime not implemented.";
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
}

internal sealed class SourceTruthScenarioReport : ReportOnlyProjectReport
{
    public string Outcome { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public List<ScenarioEvidenceItem> RequiredEvidence { get; init; } = [];
    public List<string> ExplicitDeferrals { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class ClientEvaluationPrerequisiteAuditReport : ReportOnlyProjectReport
{
    public string Outcome { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public string ClientEvaluationStatus { get; init; } = "Deferred";
    public List<ScenarioEvidenceItem> PriorEvidence { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<FutureSeamItem> RequiredFutureSeams { get; init; } = [];
    public List<string> RecommendedNextSlices { get; init; } = [];
    public List<string> ResidualDebt { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class ClientHostBoundaryPlanReport : ReportOnlyProjectReport
{
    public string Outcome { get; init; } = "MissingEvidence";
    public string FutureSeamName { get; init; } = "ClientEvaluationRuntimeReadSeam";
    public string ClientEvaluationStatus { get; init; } = "Deferred";
    public string PrerequisiteStatus { get; init; } = "MissingEvidence";
    public List<FutureBoundaryInputItem> FutureInputs { get; init; } = [];
    public List<string> ForbiddenDirectOwnership { get; init; } = [];
    public List<string> DeferredConcerns { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class AssetImportCookDeferralReport : ReportOnlyProjectReport
{
    public string Outcome { get; init; } = "Deferred";
    public string AssetManifestInventoryStatus { get; init; } = "MissingEvidence";
    public List<AssetPipelineDeferralItem> AssetClassifications { get; init; } = [];
    public List<AssetPipelineDeferralItem> DeferredPipelines { get; init; } = [];
    public List<string> FuturePrerequisites { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class BroadTestHealthPreflightReport : ReportOnlyProjectReport
{
    public string Outcome { get; init; } = "PartiallyProven";
    public string EvidenceRoot { get; init; } = "";
    public List<TestHealthEvidenceItem> FocusedEvidence { get; init; } = [];
    public List<TestHealthEvidenceItem> UnclaimedEvidence { get; init; } = [];
    public List<TestHealthEvidenceItem> DeferredEvidence { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class AssetWorkflowEvidenceMatrixReport : ReportOnlyProjectReport
{
    public string Outcome { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public List<EvidenceMatrixItem> Evidence { get; init; } = [];
    public List<string> ResidualDebt { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class AssetWorkflowClosureReport : ReportOnlyProjectReport
{
    public string Outcome { get; init; } = "Blocked";
    public string EvidenceRoot { get; init; } = "";
    public bool LimitationsDocPresent { get; init; }
    public List<EvidenceMatrixItem> RequiredEvidence { get; init; } = [];
    public List<string> ResidualDebt { get; init; } = [];
    public List<string> NextTargetRecommendations { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class RuntimeReadinessV2Report : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public string PlanningScope { get; init; } = "Runtime read seam implementation planning only.";
    public string RuntimeImplementationClaim { get; init; } = "NotImplemented";
    public string ClientHostImplementationClaim { get; init; } = "NotImplemented";
    public string ClientEvaluationStatus { get; init; } = "Deferred";
    public List<RuntimeReadinessEvidenceItem> RequiredEvidence { get; init; } = [];
    public List<RuntimeReadAdapterAuditItem> AdapterAudit { get; init; } = [];
    public List<RuntimeReadInputItem> FutureReadableInputs { get; init; } = [];
    public List<RuntimeForbiddenSourceTruthItem> ForbiddenRuntimeSourceTruth { get; init; } = [];
    public List<RuntimeReadFixtureContractItem> FixtureContract { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<string> MissingAdapterSeams { get; init; } = [];
    public List<string> ResidualDebt { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class ClientHostEvaluationOwnershipBoundaryReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string RuntimeReadinessV2Status { get; init; } = "MissingEvidence";
    public string ClientHostImplementationClaim { get; init; } = "NotImplemented";
    public string ClientEvaluationStatus { get; init; } = "Deferred";
    public List<FutureBoundaryInputItem> AllowedFutureInputs { get; init; } = [];
    public List<string> ForbiddenDirectOwnership { get; init; } = [];
    public List<string> DeferredSystems { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class ClientEvaluationLaunchProfileContractReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string RuntimeReadinessV2Status { get; init; } = "MissingEvidence";
    public string ClientEvaluationStatus { get; init; } = "Deferred";
    public string PlannedProfileId { get; init; } = "client-evaluation-local-no-network";
    public string PlannedProfileStatus { get; init; } = "DeferredProfileNotImplemented";
    public List<LaunchProfileContractItem> CurrentLaunchProfiles { get; init; } = [];
    public List<LaunchProfileContractItem> CurrentPackageProfiles { get; init; } = [];
    public List<SourceTruthGateCheck> RequiredFutureEvidence { get; init; } = [];
    public List<string> CannotLaunchYet { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class ClientEvaluationNoNetworkPlanReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "PartiallyProven";
    public string ClientEvaluationStatus { get; init; } = "Deferred";
    public string NoNetworkModeStatus { get; init; } = "PlanOnly";
    public List<SourceTruthGateCheck> NoNetworkDefinition { get; init; } = [];
    public List<RuntimeReadInputItem> AllowedLaterSimulatedInputs { get; init; } = [];
    public List<RuntimeForbiddenSourceTruthItem> ForbiddenLaterInputs { get; init; } = [];
    public List<string> BlockersBeforeImplementation { get; init; } = [];
    public List<string> NotStartedSystems { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class ClientEvaluationDiagnosticsShellReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public string RuntimeReadinessV2Status { get; init; } = "MissingEvidence";
    public string ClientHostBoundaryStatus { get; init; } = "MissingEvidence";
    public string LaunchProfileContractStatus { get; init; } = "MissingEvidence";
    public string NoNetworkPlanStatus { get; init; } = "MissingEvidence";
    public List<ReportEvidenceStatusItem> RequiredInputs { get; init; } = [];
    public List<string> DeferredSystems { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
    public List<string> ImplementationBlockers { get; init; } = [];
}

internal sealed class ClientEvaluationBlockerMatrixReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public string ClientEvaluationImplementationClaim { get; init; } = "NotImplemented";
    public string ClientHostImplementationClaim { get; init; } = "NotImplemented";
    public string RuntimeImplementationClaim { get; init; } = "NotImplemented";
    public List<ReportEvidenceStatusItem> RequiredReports { get; init; } = [];
    public List<ClientEvaluationBlockerMatrixRow> Blockers { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class MinimalRuntimeReadSeamPlanReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public string RuntimeImplementationClaim { get; init; } = "NotImplemented";
    public string ClientHostImplementationClaim { get; init; } = "NotImplemented";
    public string ClientEvaluationImplementationClaim { get; init; } = "NotImplemented";
    public string FutureSeamName { get; init; } = "RuntimeReadSeamV1";
    public List<ReportEvidenceStatusItem> RequiredReports { get; init; } = [];
    public List<RuntimeReadInputItem> FirstRuntimeConsumedSourceTruth { get; init; } = [];
    public List<RuntimeReadInputItem> EvidenceOnlyGeneratedProjections { get; init; } = [];
    public List<RuntimeReadAdapterAuditItem> FutureAdapterBoundary { get; init; } = [];
    public List<string> FutureTouchCandidates { get; init; } = [];
    public List<string> ForbiddenTouchTargets { get; init; } = [];
    public List<string> FutureProofTests { get; init; } = [];
    public List<string> BlockersBeforeImplementation { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class MinimalClientHostEvaluationShellPlanReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public string ClientHostImplementationClaim { get; init; } = "NotImplemented";
    public string RuntimeImplementationClaim { get; init; } = "NotImplemented";
    public string ClientEvaluationImplementationClaim { get; init; } = "NotImplemented";
    public string FutureSeamName { get; init; } = "ClientHostEvaluationShellV1";
    public List<ReportEvidenceStatusItem> RequiredReports { get; init; } = [];
    public List<EvaluationOwnershipItem> ClientHostFutureOwnership { get; init; } = [];
    public List<EvaluationOwnershipItem> RuntimeFutureOwnership { get; init; } = [];
    public List<EvaluationOwnershipItem> LaunchProfileFutureOwnership { get; init; } = [];
    public List<string> OutsideClientHost { get; init; } = [];
    public List<string> StartupDiagnosticsWithoutGameplay { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class PackageLaunchEvaluationAlignmentPlanReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public string ClientEvaluationImplementationClaim { get; init; } = "NotImplemented";
    public string RequiredLaunchProfileId { get; init; } = "client-evaluation-local-no-network";
    public string RequiredLaunchProfileStatus { get; init; } = "DeferredProfileNotImplemented";
    public string RequiredPackageProfileId { get; init; } = "project-readiness-client-evaluation-local";
    public string RequiredPackageProfileStatus { get; init; } = "DeferredProfileNotImplemented";
    public List<ReportEvidenceStatusItem> RequiredReports { get; init; } = [];
    public List<LaunchProfileContractItem> CurrentLaunchProfiles { get; init; } = [];
    public List<LaunchProfileContractItem> CurrentPackageProfiles { get; init; } = [];
    public List<SourceTruthGateCheck> RequiredReferencedEvidence { get; init; } = [];
    public List<string> DeferredWork { get; init; } = [];
    public List<string> NotRuntimeProof { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class EditorlessEvaluationWorkflowPlanReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public string ClientEvaluationImplementationClaim { get; init; } = "NotImplemented";
    public string PlayableEditorClaim { get; init; } = "NotClaimed";
    public string FutureTrigger { get; init; } = "Future CLI/report workflow only.";
    public List<ReportEvidenceStatusItem> RequiredReports { get; init; } = [];
    public List<RuntimeReadInputItem> ConsumedInputs { get; init; } = [];
    public List<string> FutureDiagnostics { get; init; } = [];
    public List<string> ImpossibleWithoutEditorUi { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class EvaluationEvidenceGateReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public string RuntimeImplementationClaim { get; init; } = "NotImplemented";
    public string ClientHostImplementationClaim { get; init; } = "NotImplemented";
    public string ClientEvaluationImplementationClaim { get; init; } = "NotImplemented";
    public List<ReportEvidenceStatusItem> RequiredReports { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<string> DeferredImplementationWork { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class AssetImportCookPrerequisiteReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvidenceRoot { get; init; } = "";
    public string AssetImportImplementationClaim { get; init; } = "NotImplemented";
    public string AssetCookImplementationClaim { get; init; } = "NotImplemented";
    public List<ReportEvidenceStatusItem> RequiredEvidence { get; init; } = [];
    public List<PrerequisiteEvidenceItem> AssetSourceTruth { get; init; } = [];
    public List<ResolvedAssetReferenceItem> AcceptedFutureEvaluationReferences { get; init; } = [];
    public List<PrerequisiteEvidenceItem> EvidenceOnlyArtifacts { get; init; } = [];
    public List<PipelinePrerequisiteItem> MissingPipelines { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class RuntimeAssetConsumptionPrerequisiteReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string SourceTruthRoot { get; init; } = "";
    public string EvaluationRoot { get; init; } = "";
    public string RuntimeAssetConsumptionSeamClaim { get; init; } = "NotImplemented";
    public string RuntimeImplementationClaim { get; init; } = "NotImplemented";
    public List<ReportEvidenceStatusItem> RequiredEvidence { get; init; } = [];
    public List<RuntimeReadInputItem> FutureRuntimeEvaluationAssetNeeds { get; init; } = [];
    public List<ResolvedAssetReferenceItem> AcceptedAssetReferences { get; init; } = [];
    public List<RuntimeForbiddenSourceTruthItem> ForbiddenRuntimeAssetInputs { get; init; } = [];
    public List<RuntimeReadAdapterAuditItem> MissingRuntimeAssetConsumptionSeams { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class RuntimeProjectionFreshnessGateReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string SourceTruthRoot { get; init; } = "";
    public string RuntimeImplementationClaim { get; init; } = "NotImplemented";
    public string RuntimeGameplayClaim { get; init; } = "NotImplemented";
    public List<ReportEvidenceStatusItem> RequiredEvidence { get; init; } = [];
    public List<RuntimeReadInputItem> RuntimeFacingProjectionReadiness { get; init; } = [];
    public List<GeneratedFreshnessSourceInputItem> StaleSourceInputs { get; init; } = [];
    public List<GeneratedFreshnessArtifactItem> StaleGeneratedProjections { get; init; } = [];
    public List<GeneratedEvidenceOwnershipItem> NonCanonicalGeneratedProjections { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class ClientEvaluationRegressionFixturePlanReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvaluationRoot { get; init; } = "";
    public List<ReportEvidenceStatusItem> RequiredReports { get; init; } = [];
    public List<ClientEvaluationRegressionFixtureItem> Fixtures { get; init; } = [];
    public List<string> ForbiddenExecution { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class EvaluationFocusedTestHealthGateReport : ReportOnlyProjectReport
{
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public string EvaluationRoot { get; init; } = "";
    public string RawFullCTestClaim { get; init; } = "Unclaimed";
    public string HeavyStressClaim { get; init; } = "Unclaimed";
    public string RealTransportProofClaim { get; init; } = "Unclaimed";
    public List<ReportEvidenceStatusItem> RequiredReports { get; init; } = [];
    public List<TestHealthEvidenceItem> FocusedEvidence { get; init; } = [];
    public List<TestHealthEvidenceItem> OptionalEvidence { get; init; } = [];
    public List<TestHealthEvidenceItem> UnclaimedEvidence { get; init; } = [];
    public List<SourceTruthGateCheck> Checks { get; init; } = [];
    public List<string> NonClaims { get; init; } = [];
}

internal sealed class PrerequisiteEvidenceItem
{
    public string ItemId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string Path { get; init; } = "";
    public string Classification { get; init; } = "";
    public bool Canonical { get; init; }
    public string Status { get; init; } = "";
}

internal sealed class PipelinePrerequisiteItem
{
    public string PipelineId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string ImplementationClaim { get; init; } = "NotImplemented";
    public string Status { get; init; } = "NotImplemented";
}

internal sealed class ClientEvaluationRegressionFixtureItem
{
    public string FixtureId { get; init; } = "";
    public List<string> RequiredEvidence { get; init; } = [];
    public string ExpectedFutureAssertion { get; init; } = "";
    public string CurrentStatus { get; init; } = "PlanOnly";
    public string ExecutionStatus { get; init; } = "NotExecuted";
}

internal sealed class RuntimeReadinessEvidenceItem
{
    public string EvidenceId { get; init; } = "";
    public string EvidenceScope { get; init; } = "";
    public string Command { get; init; } = "";
    public string Path { get; init; } = "";
    public string SourceStatus { get; init; } = "Missing";
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public bool Required { get; init; } = true;
}

internal sealed class RuntimeReadAdapterAuditItem
{
    public string AdapterId { get; init; } = "";
    public string SourceEvidenceId { get; init; } = "";
    public string ExpectedInput { get; init; } = "";
    public string Status { get; init; } = "MissingAdapterSeam";
}

internal sealed class RuntimeReadFixtureContractItem
{
    public string FixtureId { get; init; } = "";
    public string RequiredInput { get; init; } = "";
    public string ExpectedBehavior { get; init; } = "";
    public string Status { get; init; } = "ContractOnly";
}

internal sealed class LaunchProfileContractItem
{
    public string ProfileId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string Path { get; init; } = "";
    public string Classification { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed class ReportEvidenceStatusItem
{
    public string EvidenceId { get; init; } = "";
    public string Command { get; init; } = "";
    public string Path { get; init; } = "";
    public string SourceStatus { get; init; } = "Missing";
    public string ReadinessStatus { get; init; } = "MissingEvidence";
    public bool Required { get; init; } = true;
}

internal sealed class ClientEvaluationBlockerMatrixRow
{
    public string BlockerId { get; init; } = "";
    public string Area { get; init; } = "";
    public string EvidenceStatus { get; init; } = "";
    public string ImplementationStatus { get; init; } = "";
    public string MatrixStatus { get; init; } = "Blocked";
    public string Summary { get; init; } = "";
}

internal sealed class EvaluationOwnershipItem
{
    public string Owner { get; init; } = "";
    public string Responsibility { get; init; } = "";
    public string Boundary { get; init; } = "";
    public string Status { get; init; } = "FuturePlan";
}

internal sealed class ComponentMetadataOwnershipItem
{
    public string SceneId { get; init; } = "";
    public string EntityId { get; init; } = "";
    public string ComponentType { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string Classification { get; init; } = "SceneEntitySourceTruth";
    public string Status { get; init; } = "Accepted";
}

internal sealed class GeneratedEvidenceOwnershipItem
{
    public string SceneId { get; init; } = "";
    public string ArtifactId { get; init; } = "";
    public string Path { get; init; } = "";
    public string ArtifactKind { get; init; } = "";
    public string Classification { get; init; } = "EvidenceOnly";
    public bool Canonical { get; init; }
    public string Status { get; init; } = "EvidenceOnly";
}

internal sealed class EditorInspectionSceneItem
{
    public string SceneId { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string SourceKind { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed class EditorInspectionEntityItem
{
    public string SceneId { get; init; } = "";
    public string EntityId { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string SourceKind { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed class EditorSourceTruthReadItem
{
    public string SceneId { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public bool Exists { get; init; }
    public string Status { get; init; } = "";
}

internal sealed class RuntimeReadInputItem
{
    public string InputId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string Path { get; init; } = "";
    public string Classification { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed class RuntimeForbiddenSourceTruthItem
{
    public string InputId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string Path { get; init; } = "";
    public string Reason { get; init; } = "";
}

internal sealed class ScenarioEvidenceItem
{
    public string EvidenceId { get; init; } = "";
    public string Command { get; init; } = "";
    public string Path { get; init; } = "";
    public string Status { get; init; } = "Missing";
}

internal sealed class FutureSeamItem
{
    public string SeamName { get; init; } = "";
    public string Scope { get; init; } = "";
    public string Status { get; init; } = "PlanningOnly";
}

internal sealed class FutureBoundaryInputItem
{
    public string InputId { get; init; } = "";
    public string Boundary { get; init; } = "";
    public string Source { get; init; } = "";
    public string Status { get; init; } = "FutureInput";
}

internal sealed class AssetPipelineDeferralItem
{
    public string ItemId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string Source { get; init; } = "";
    public string Classification { get; init; } = "";
    public string Status { get; init; } = "Deferred";
}

internal sealed class TestHealthEvidenceItem
{
    public string EvidenceId { get; init; } = "";
    public string Path { get; init; } = "";
    public string Status { get; init; } = "";
    public string Classification { get; init; } = "";
}

internal sealed class EvidenceMatrixItem
{
    public string EvidenceId { get; init; } = "";
    public string EvidenceScope { get; init; } = "";
    public string Path { get; init; } = "";
    public string SourceStatus { get; init; } = "Missing";
    public string MatrixStatus { get; init; } = "MissingEvidence";
    public string Classification { get; init; } = "";
}

internal sealed class SourceTruthSummary
{
    public int Scenes { get; init; }
    public int Entities { get; init; }
    public int AssetReferences { get; init; }
    public int GeneratedArtifacts { get; init; }
    public int SourceTruthFiles { get; init; }
    public int NonCanonicalGeneratedArtifacts { get; init; }
    public int MissingSourceTruth { get; init; }
    public int StaleGeneratedEvidence { get; init; }
    public int DeferredReadBoundaries { get; init; }
}

internal sealed class SceneSourceTruthItem
{
    public string SceneId { get; init; } = "";
    public string ProjectReferenceId { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string Classification { get; init; } = "SourceTruth";
    public string Status { get; init; } = "Passed";
}

internal sealed class EntitySourceTruthItem
{
    public string SceneId { get; init; } = "";
    public string EntityId { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string Classification { get; init; } = "SourceTruth";
    public string Status { get; init; } = "Passed";
}

internal sealed class AssetReferenceTruthItem
{
    public string SceneId { get; init; } = "";
    public string EntityId { get; init; } = "";
    public string AssetId { get; init; } = "";
    public string Owner { get; init; } = "";
    public string Path { get; init; } = "";
    public string Classification { get; init; } = "AssetReference";
    public string Status { get; init; } = "Passed";
}

internal sealed class GeneratedArtifactTruthItem
{
    public string SceneId { get; init; } = "";
    public string ArtifactId { get; init; } = "";
    public string Path { get; init; } = "";
    public string Classification { get; init; } = "GeneratedArtifact";
    public bool Canonical { get; init; }
    public string ExpectedSourceHash { get; init; } = "";
    public string ActualSourceHash { get; init; } = "";
    public string Status { get; init; } = "Passed";
}

internal sealed class ReadBoundaryTruthItem
{
    public string Boundary { get; init; } = "";
    public string Status { get; init; } = "";
    public string Enforcement { get; init; } = "ReportOnly";
    public string Summary { get; init; } = "";
}

internal sealed class SourceTruthGateCheck
{
    public string CheckId { get; init; } = "";
    public string Status { get; init; } = "";
    public string Summary { get; init; } = "";
}

internal sealed class AssetRootInventoryItem
{
    public string AssetId { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string Kind { get; init; } = "";
    public bool Exists { get; init; }
    public string Classification { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed class AssetSourceManifestItem
{
    public string ManifestId { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string ManifestKind { get; init; } = "";
    public string Classification { get; init; } = "";
    public bool Canonical { get; init; }
    public string Status { get; init; } = "";
}

internal sealed class AssetOwnerInventoryItem
{
    public string AssetId { get; init; } = "";
    public string Owner { get; init; } = "";
    public string Path { get; init; } = "";
    public string SourceManifest { get; init; } = "";
    public string Classification { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed class MissingAssetOwnerItem
{
    public string SceneId { get; init; } = "";
    public string EntityId { get; init; } = "";
    public string AssetId { get; init; } = "";
    public string Path { get; init; } = "";
    public string Reason { get; init; } = "";
}

internal sealed class GeneratedAssetArtifactItem
{
    public string ArtifactId { get; init; } = "";
    public string Path { get; init; } = "";
    public string Classification { get; init; } = "";
    public bool Canonical { get; init; }
    public string Status { get; init; } = "";
}

internal sealed class ResolvedAssetReferenceItem
{
    public string SceneId { get; init; } = "";
    public string EntityId { get; init; } = "";
    public string AssetId { get; init; } = "";
    public string Owner { get; init; } = "";
    public string Path { get; init; } = "";
    public string Resolution { get; init; } = "";
}

internal sealed class UnresolvedAssetReferenceItem
{
    public string SceneId { get; init; } = "";
    public string EntityId { get; init; } = "";
    public string AssetId { get; init; } = "";
    public string Owner { get; init; } = "";
    public string Path { get; init; } = "";
    public string Reason { get; init; } = "";
}

internal sealed class RejectedGeneratedOwnerItem
{
    public string SceneId { get; init; } = "";
    public string EntityId { get; init; } = "";
    public string AssetId { get; init; } = "";
    public string Owner { get; init; } = "";
    public string Reason { get; init; } = "";
}

internal sealed class GeneratedFreshnessArtifactItem
{
    public string SceneId { get; init; } = "";
    public string ArtifactId { get; init; } = "";
    public string Path { get; init; } = "";
    public string ExpectedSourceHash { get; init; } = "";
    public string ActualSourceHash { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed class GeneratedFreshnessSourceInputItem
{
    public string SceneId { get; init; } = "";
    public string SourceHash { get; init; } = "";
    public string RelativePath { get; init; } = "";
}

internal sealed class GeneratedFreshnessStatusItem
{
    public string ArtifactId { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed class SceneSchemaValidationItem
{
    public string SceneId { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string SchemaVersion { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed class EntitySchemaValidationItem
{
    public string SceneId { get; init; } = "";
    public string EntityId { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string SchemaVersion { get; init; } = "";
    public string Status { get; init; } = "";
}

internal sealed class ResolvedProjectPath
{
    public string Category { get; init; } = "";
    public string Id { get; init; } = "";
    public string Kind { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string AbsolutePath { get; init; } = "";
    public bool Exists { get; init; }
}

internal sealed class ManifestLoadResult
{
    public string ProjectRoot { get; init; } = "";
    public string ManifestPath { get; init; } = "";
    public SagaProjectDescriptor? Descriptor { get; init; }
    public List<ProjectDiagnostic> Diagnostics { get; init; } = [];
    public List<ResolvedProjectPath> ResolvedPaths { get; init; } = [];

    [JsonIgnore]
    public bool Passed => Diagnostics.All(d => d.Severity != "Error");
}
