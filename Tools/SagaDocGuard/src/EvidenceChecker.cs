namespace SagaDocGuard;

internal static class EvidenceChecker
{
    public static IReadOnlyList<EvidenceRule> RequiredEvidence { get; } =
    [
        new EvidenceRule("diagnostics-closure", "docs/architecture/POST_DIAGNOSTICS_PRODUCT_OPENING_CHECKPOINT.md"),
        new EvidenceRule("project-spine", "docs/product/SAGA_PROJECT_MODEL_INVENTORY_PHASE14.md"),
        new EvidenceRule("playable-vertical-slice", "docs/product/MULTIPLAYER_SANDBOX_PLAYABLE_VERTICAL_SLICE_PHASE20_25.md"),
        new EvidenceRule("diagnostics-visibility", "docs/product/DIAGNOSTICS_VISIBILITY_PRODUCT_LAYER_PHASE26_28.md"),
        new EvidenceRule("packaging-publish-proof", "docs/product/PACKAGING_PUBLISH_PROOF_PHASE29_33.md"),
        new EvidenceRule("sagaweaver-mvp", "docs/product/SAGAWEAVER_SAGASCRIPT_MVP_PHASE34_43.md"),
        new EvidenceRule("editor-authoring-spine", "docs/architecture/EDITOR_AUTHORING_SPINE_PHASE44_49.md"),
        new EvidenceRule("collaboration-metadata", "docs/architecture/COLLABORATION_MODEL_BLOCK_H_PHASE50_56.md"),
        new EvidenceRule("view-capability-honesty", "docs/architecture/VIEW_CAPABILITY_PRODUCT_HONESTY_PHASE57_59.md"),
        new EvidenceRule("technical-preview-quickstart", "docs/product/SAGAENGINE_0_1_TECHNICAL_PREVIEW_QUICKSTART.md"),
        new EvidenceRule("technical-preview-claims", "docs/product/SAGA_RELEASE_VOCABULARY_AND_CLAIM_LEVELS.md"),
        new EvidenceRule("technical-preview-closure", "docs/product/SAGAENGINE_0_1_TECHNICAL_PREVIEW_CLOSURE_PHASE60_65.md"),
        new EvidenceRule("small-team-alpha-opening", "docs/architecture/SMALL_TEAM_ALPHA_OPENING_CHECKPOINT.md"),
        new EvidenceRule("small-team-alpha-acceptance-plan", "docs/testing/SAGA_SMALL_TEAM_ALPHA_ACCEPTANCE_TEST_PLAN.md"),
        new EvidenceRule("small-team-alpha-budgets", "docs/architecture/SAGA_PERFORMANCE_BUDGETS.md"),
        new EvidenceRule("gameplay-node-library-v1-policy", "docs/architecture/SAGA_GAMEPLAY_NODE_LIBRARY_V1.md"),
        new EvidenceRule("source-patch-apply-policy", "docs/architecture/SOURCE_PATCH_APPLY_POLICY.md"),
        new EvidenceRule("editor-ui-implementation-strategy", "docs/architecture/EDITOR_UI_IMPLEMENTATION_STRATEGY.md"),
        new EvidenceRule("client-preview-runtime-ui-strategy", "docs/architecture/CLIENT_PREVIEW_AND_RUNTIME_UI_STRATEGY.md"),
        new EvidenceRule("generated-files-policy", "docs/architecture/SAGA_GENERATED_FILES_POLICY.md"),
        new EvidenceRule("artifact-contracts-v1", "docs/architecture/SAGA_ARTIFACT_CONTRACTS_V1.md"),
        new EvidenceRule("small-team-alpha-block-d", "docs/architecture/SMALL_TEAM_ALPHA_BLOCK_D_EVIDENCE.md"),
        new EvidenceRule("small-team-alpha-block-e", "docs/architecture/SMALL_TEAM_ALPHA_BLOCK_E_EVIDENCE.md"),
        new EvidenceRule("small-team-alpha-quickstart", "docs/product/SAGAENGINE_SMALL_TEAM_ALPHA_QUICKSTART.md"),
        new EvidenceRule("small-team-alpha-multiplayer-tutorial", "docs/product/MULTIPLAYER_SANDBOX_SMALL_TEAM_ALPHA_TUTORIAL.md"),
        new EvidenceRule("small-team-alpha-authoring-guide", "docs/product/SMALL_TEAM_ALPHA_CSHARP_BLOCKS_AUTHORING_GUIDE.md"),
        new EvidenceRule("small-team-alpha-collaboration-guide", "docs/product/SMALL_TEAM_ALPHA_COLLABORATION_GUIDE.md"),
        new EvidenceRule("small-team-alpha-known-limitations", "docs/product/SMALL_TEAM_ALPHA_KNOWN_LIMITATIONS.md"),
        new EvidenceRule("small-team-alpha-troubleshooting", "docs/product/SMALL_TEAM_ALPHA_TROUBLESHOOTING.md"),
        new EvidenceRule("small-team-alpha-closure", "docs/architecture/SMALL_TEAM_ALPHA_CLOSURE_CHECKPOINT.md"),
        new EvidenceRule("enterprise-opening-checkpoint", "docs/architecture/ENTERPRISE_EVOLVABLE_OPENING_CHECKPOINT.md"),
        new EvidenceRule("enterprise-threat-model-v0", "docs/architecture/SAGA_ENTERPRISE_THREAT_MODEL_V0.md"),
        new EvidenceRule("enterprise-claim-levels", "docs/dev/SAGA_ENTERPRISE_CLAIM_LEVELS.md"),
        new EvidenceRule("policy-domain-model-v0", "docs/architecture/POLICY_DOMAIN_MODEL_V0.md"),
        new EvidenceRule("project-role-profiles-v1", "docs/architecture/PROJECT_ROLE_PROFILES_V1.md"),
        new EvidenceRule("dangerous-operation-policy-gate-v1", "docs/architecture/DANGEROUS_OPERATION_POLICY_GATE_V1.md"),
        new EvidenceRule("policy-diagnostics-integration-phase103", "docs/architecture/POLICY_DIAGNOSTICS_INTEGRATION_PHASE103.md"),
        new EvidenceRule("project-slice-schema-v0", "docs/architecture/PROJECT_SLICE_SCHEMA_V0.md"),
        new EvidenceRule("project-slice-resolver-v1", "docs/architecture/PROJECT_SLICE_RESOLVER_V1.md"),
        new EvidenceRule("source-visibility-levels-v1", "docs/architecture/SOURCE_VISIBILITY_LEVELS_V1.md"),
        new EvidenceRule("restricted-project-resolution-phase107", "docs/architecture/RESTRICTED_PROJECT_RESOLUTION_PHASE107.md"),
        new EvidenceRule("viewkit-policy-slice-compatibility-phase108", "docs/architecture/VIEWKIT_POLICY_SLICE_COMPATIBILITY_PHASE108.md"),
        new EvidenceRule("workspacehub-server-architecture", "docs/architecture/SAGA_WORKSPACEHUB_SERVER_ARCHITECTURE.md"),
        new EvidenceRule("immutable-audit-log-v1", "docs/architecture/IMMUTABLE_AUDIT_LOG_V1.md"),
        new EvidenceRule("review-approval-workflow-v2", "docs/architecture/REVIEW_APPROVAL_WORKFLOW_V2.md"),
        new EvidenceRule("publish-policy-integration", "docs/architecture/PUBLISH_POLICY_INTEGRATION.md"),
        new EvidenceRule("restricted-artifact-export-gate", "docs/architecture/RESTRICTED_ARTIFACT_EXPORT_GATE.md"),
        new EvidenceRule("policy-regression-suite-phase117", "docs/architecture/POLICY_REGRESSION_SUITE_PHASE117.md"),
        new EvidenceRule("restricted-presence-redaction-phase118", "docs/architecture/RESTRICTED_PRESENCE_REDACTION_PHASE118.md"),
        new EvidenceRule("policy-aware-editor-actions-phase119", "docs/architecture/POLICY_AWARE_EDITOR_ACTIONS_PHASE119.md"),
        new EvidenceRule("project-slice-aware-team-room-phase120", "docs/architecture/PROJECT_SLICE_AWARE_TEAM_ROOM_PHASE120.md"),
        new EvidenceRule("admin-lead-governance-panel-v0-phase121", "docs/architecture/ADMIN_LEAD_GOVERNANCE_PANEL_V0_PHASE121.md"),
        new EvidenceRule("enterprise-security-limitations", "docs/architecture/ENTERPRISE_SECURITY_LIMITATIONS.md"),
        new EvidenceRule("enterprise-evolvable-foundation-closure", "docs/architecture/ENTERPRISE_EVOLVABLE_FOUNDATION_CLOSURE_CHECKPOINT.md"),
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
