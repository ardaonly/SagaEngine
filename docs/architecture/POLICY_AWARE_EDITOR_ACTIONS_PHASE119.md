# Policy-Aware Editor Actions Phase 119

Phase 119 extends `Tools/SagaWorkspaceHub` with backend-neutral action
descriptors:

```sh
Tools/SagaWorkspaceHub/sagaworkspacehub policy-actions --policy-report <report> [--review-approval-report <report>] [--restricted-export-report <report>] --out <policy_aware_editor_actions_report.json>
```

The report emits fixed descriptors for `ApplyPatch`, `RollbackPatch`,
`ChangePackageProfile`, `ExportArtifact`, `ApproveReview`, `OverrideLock`,
`DeleteBehaviorSource`, and `DeleteScene`.

Each descriptor includes `actionId`, `status`, `reason`,
`sourceMutationOwner`, and `diagnostics`. Patch actions are disabled for editor
use and remain SagaScript-owned. Delete actions are blocked/report-only.
Export and review descriptors follow supplied policy/review/export evidence.

This phase does not add Editor UI, Qt UI, source mutation, patch apply,
patch rollback, or live permission enforcement.
