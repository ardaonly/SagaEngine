namespace SagaDocGuard;

internal static class EvidenceChecker
{
    public static IReadOnlyList<EvidenceRule> RequiredEvidence { get; } =
    [
        new EvidenceRule("diagnostics-boundary", "docs/architecture/DIAGNOSTICS_MIGRATION_AUDIT.md"),
        new EvidenceRule("project-spine", "docs/internal/product-history/SAGA_PROJECT_MODEL_INVENTORY_MILESTONE14.md"),
        new EvidenceRule("playable-vertical-slice", "docs/internal/product-history/MULTIPLAYER_SANDBOX_PLAYABLE_VERTICAL_SLICE_MILESTONE20_25.md"),
        new EvidenceRule("diagnostics-visibility", "docs/internal/product-history/DIAGNOSTICS_VISIBILITY_PRODUCT_LAYER_MILESTONE26_28.md"),
        new EvidenceRule("packaging-publish-proof", "docs/internal/product-history/PACKAGING_PUBLISH_PROOF_MILESTONE29_33.md"),
        new EvidenceRule("sagaweaver-mvp", "docs/internal/product-history/SAGAWEAVER_SAGASCRIPT_MVP_MILESTONE34_43.md"),
        new EvidenceRule("editor-authoring-spine", "docs/architecture/EDITOR.md"),
        new EvidenceRule("collaboration-metadata", "docs/architecture/SAGA_COLLABORATIVE_WORKSPACE_ARCHITECTURE.md"),
        new EvidenceRule("view-capability-honesty", "docs/architecture/SAGA_PRODUCT_SHELL_WORKFLOW_CONTRACT.md"),
        new EvidenceRule("technical-preview-quickstart", "docs/internal/product-history/SAGAENGINE_0_1_TECHNICAL_PREVIEW_QUICKSTART.md"),
        new EvidenceRule("technical-preview-claims", "docs/internal/product-history/SAGA_RELEASE_VOCABULARY_AND_CLAIM_LEVELS.md"),
        new EvidenceRule("technical-preview-closure", "docs/internal/product-history/SAGAENGINE_0_1_TECHNICAL_PREVIEW_CLOSURE_MILESTONE60_65.md"),
        new EvidenceRule("small-team-alpha-boundary", "docs/architecture/SAGA_PRODUCT_SHELL_WORKFLOW_CONTRACT.md"),
        new EvidenceRule("small-team-alpha-acceptance-plan", "docs/testing/SAGA_SMALL_TEAM_ALPHA_ACCEPTANCE_TEST_PLAN.md"),
        new EvidenceRule("small-team-alpha-budgets", "docs/architecture/SAGA_PERFORMANCE_BUDGETS.md"),
        new EvidenceRule("gameplay-node-library-v1-policy", "docs/architecture/SAGA_GAMEPLAY_NODE_LIBRARY_V1.md"),
        new EvidenceRule("source-patch-apply-policy", "docs/architecture/SOURCE_PATCH_APPLY_POLICY.md"),
        new EvidenceRule("editor-ui-implementation-strategy", "docs/architecture/EDITOR_UI_IMPLEMENTATION_STRATEGY.md"),
        new EvidenceRule("client-preview-runtime-ui-strategy", "docs/architecture/CLIENT_PREVIEW_AND_RUNTIME_UI_STRATEGY.md"),
        new EvidenceRule("generated-files-policy", "docs/architecture/SAGA_GENERATED_FILES_POLICY.md"),
        new EvidenceRule("artifact-contracts-v1", "docs/architecture/SAGA_ARTIFACT_CONTRACTS_V1.md"),
        new EvidenceRule("small-team-alpha-package-boundary", "docs/architecture/PUBLISH.md"),
        new EvidenceRule("small-team-alpha-collaboration-boundary", "docs/architecture/SAGA_COLLABORATIVE_WORKSPACE_ARCHITECTURE.md"),
        new EvidenceRule("small-team-alpha-quickstart", "docs/internal/product-history/SAGAENGINE_SMALL_TEAM_ALPHA_QUICKSTART.md"),
        new EvidenceRule("small-team-alpha-multiplayer-tutorial", "docs/internal/product-history/MULTIPLAYER_SANDBOX_SMALL_TEAM_ALPHA_TUTORIAL.md"),
        new EvidenceRule("small-team-alpha-authoring-guide", "docs/internal/product-history/SMALL_TEAM_ALPHA_CSHARP_BLOCKS_AUTHORING_GUIDE.md"),
        new EvidenceRule("small-team-alpha-collaboration-guide", "docs/internal/product-history/SMALL_TEAM_ALPHA_COLLABORATION_GUIDE.md"),
        new EvidenceRule("small-team-alpha-known-limitations", "docs/internal/product-history/SMALL_TEAM_ALPHA_KNOWN_LIMITATIONS.md"),
        new EvidenceRule("small-team-alpha-troubleshooting", "docs/internal/product-history/SMALL_TEAM_ALPHA_TROUBLESHOOTING.md"),
        new EvidenceRule("small-team-alpha-status", "docs/architecture/CURRENT_STATUS.md"),
        new EvidenceRule("enterprise-boundary", "docs/architecture/SAGA_ENTERPRISE_THREAT_MODEL_V0.md"),
        new EvidenceRule("enterprise-threat-model-v0", "docs/architecture/SAGA_ENTERPRISE_THREAT_MODEL_V0.md"),
        new EvidenceRule("enterprise-claim-levels", "docs/dev/SAGA_ENTERPRISE_CLAIM_LEVELS.md"),
        new EvidenceRule("policy-domain-model-v0", "docs/architecture/POLICY_DOMAIN_MODEL_V0.md"),
        new EvidenceRule("project-role-profiles-v1", "docs/architecture/PROJECT_ROLE_PROFILES_V1.md"),
        new EvidenceRule("dangerous-operation-policy-gate-v1", "docs/architecture/DANGEROUS_OPERATION_POLICY_GATE_V1.md"),
        new EvidenceRule("policy-diagnostics-integration", "docs/architecture/CLAIM_AND_EVIDENCE_POLICY.md"),
        new EvidenceRule("project-slice-schema-v0", "docs/architecture/PROJECT_SLICE_SCHEMA_V0.md"),
        new EvidenceRule("project-slice-resolver-v1", "docs/architecture/PROJECT_SLICE_RESOLVER_V1.md"),
        new EvidenceRule("source-visibility-levels-v1", "docs/architecture/SOURCE_VISIBILITY_LEVELS_V1.md"),
        new EvidenceRule("restricted-project-resolution", "docs/architecture/PROJECT_SLICE_RESOLVER_V1.md"),
        new EvidenceRule("viewkit-policy-slice-compatibility", "docs/architecture/PROJECT_SLICE_SCHEMA_V0.md"),
        new EvidenceRule("workspacehub-server-architecture", "docs/architecture/SAGA_WORKSPACEHUB_SERVER_ARCHITECTURE.md"),
        new EvidenceRule("immutable-audit-log-v1", "docs/architecture/IMMUTABLE_AUDIT_LOG_V1.md"),
        new EvidenceRule("review-approval-workflow-v2", "docs/architecture/REVIEW_APPROVAL_WORKFLOW_V2.md"),
        new EvidenceRule("publish-policy-integration", "docs/architecture/PUBLISH_POLICY_INTEGRATION.md"),
        new EvidenceRule("restricted-artifact-export-gate", "docs/architecture/RESTRICTED_ARTIFACT_EXPORT_GATE.md"),
        new EvidenceRule("policy-regression-suite", "docs/architecture/POLICY_DOMAIN_MODEL_V0.md"),
        new EvidenceRule("restricted-presence-redaction", "docs/architecture/SAGA_COLLABORATION_PRESENCE_LOCK_BOUNDARY.md"),
        new EvidenceRule("policy-aware-editor-actions", "docs/architecture/DANGEROUS_OPERATION_POLICY_GATE_V1.md"),
        new EvidenceRule("project-slice-aware-team-room", "docs/architecture/SAGA_WORKSPACEHUB_SERVER_ARCHITECTURE.md"),
        new EvidenceRule("admin-lead-governance-panel-v0", "docs/architecture/AUTHORING_AUTHORITY_MODEL.md"),
        new EvidenceRule("enterprise-security-limitations", "docs/architecture/ENTERPRISE_SECURITY_LIMITATIONS.md"),
        new EvidenceRule("enterprise-evolvable-foundation-status", "docs/architecture/CURRENT_STATUS.md"),
    ];

    public static IReadOnlyList<MissingEvidence> Check(string evidenceRoot)
    {
        if (string.IsNullOrWhiteSpace(evidenceRoot))
        {
            return Array.Empty<MissingEvidence>();
        }

        return RequiredEvidence
            .Where(rule => !File.Exists(Path.Combine(evidenceRoot, rule.Path)))
            .Select(rule => new MissingEvidence
            {
                EvidenceId = rule.EvidenceId,
                Path = rule.Path,
            })
            .OrderBy(item => item.EvidenceId, StringComparer.Ordinal)
            .ToArray();
    }
}
