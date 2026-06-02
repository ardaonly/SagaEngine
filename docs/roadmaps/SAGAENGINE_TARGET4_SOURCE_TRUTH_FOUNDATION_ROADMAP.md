# SagaEngine Target 4 / Source Truth Foundation Source Truth Roadmap

## Status

Phase 126-133 opening scope. Target 3 / Enterprise-Evolvable Foundation closed as `PartiallyProven`, so this
roadmap starts from preserved residual debt rather than claiming a productized
milestone.

## Phase 126-133 Opening Block

- Phase 126 freezes the Target 4 / Source Truth Foundation claim boundary around source truth only.
- Phase 127 defines Scene and Entity source-controlled truth.
- Phase 128 defines generated artifacts as evidence, not canonical source, and
  assigns asset reference ownership to project and scene/entity source files.
- Phase 129 documents Runtime and Editor read boundaries without implementing
  Runtime gameplay, Server gameplay, ClientHost, Editor UI, or Qt UI work.
- Phase 130 adds deterministic local scene/entity and generated artifact
  fixtures.
- Phase 131 emits a report-only source truth inventory.
- Phase 132 emits a report-only source truth gate.
- Phase 133 emits a Target 4 / Source Truth Foundation opening acceptance report.

## Phase 134-140 First Follow-Up Batch

Phase 134-140 is the active first follow-up batch. It remains local and
report-only.

- Phase 134 inventories project asset roots and placeholder asset root debt.
- Phase 135 accepts source-controlled `assets.source.json` or
  `asset_manifest.json` asset metadata and classifies generated/package asset
  manifests as non-canonical evidence.
- Phase 136 classifies generated artifact freshness as `Fresh`, `Stale`,
  `MissingSource`, `MissingGenerated`, `UnknownFreshness`, or `Invalid`.
- Phase 137 defines Scene schema v1 validation.
- Phase 138 validates malformed scene JSON, missing scene ids, and generated
  canonical claims.
- Phase 139 defines Entity schema v1 validation.
- Phase 140 validates duplicate entity ids, missing entity ids, and missing
  asset reference owners.

## Phase 141-148 Second Follow-Up Batch

Phase 141-148 is the active second follow-up batch. It remains local and
report-only.

- Phase 141 classifies component metadata ownership for `Transform` and
  `BehaviorReference`, diagnoses unknown component types, and keeps generated
  bindings/projections as evidence only.
- Phase 142 emits a backend-neutral editor inspection model with
  `editorMayMutate=false`.
- Phase 143 re-reads declared scene source files and rejects missing scene
  source or generated canonical claims.
- Phase 144 audits future-readable Runtime inputs without implementing Runtime.
- Phase 145 emits Runtime readiness as planning-only evidence.
- Phase 146 aligns package profiles to source-truth gates without staging
  packages or rewriting package formats.
- Phase 147 aligns launch profiles to source truth without launching processes
  and preserves Client Preview deferral.
- Phase 148 summarizes required source-truth scenario evidence.

## Phase 149-155 Closure Batch

Phase 149-155 closes Target 4 / Source Truth Foundation as a report-only source-truth foundation batch.

- Phase 149 audits Client Preview prerequisites and keeps
  `clientPreviewStatus=Deferred`.
- Phase 150 names the future `ClientPreviewRuntimeReadSeam` as planning-only
  boundary work.
- Phase 151 records asset import and cook as deferred from the asset source
  manifest inventory.
- Phase 152 records focused evidence health while leaving raw full CTest
  unclaimed and heavy/stress/soak/bot/transport proof deferred.
- Phase 153 emits the Target 4 / Source Truth Foundation evidence matrix and prevents missing required
  source-truth evidence from becoming `Passed`.
- Phase 154 freezes the Target 4 / Source Truth Foundation source-truth limitations.
- Phase 155 emits the Target 4 / Source Truth Foundation closure report from local evidence only.

## Non-Claims

This roadmap does not implement Productization Readiness, Release-Track
Readiness, Production Readiness, Client Preview, ClientHost changes, Runtime
gameplay, Server gameplay, Editor UI, Qt UI, asset import, asset cook, raw full
CTest, heavy stress, soak testing, bot swarms, or real transport stress.

Target 3 / Enterprise-Evolvable Foundation residual debt remains visible: ClientHost preview, Editor/Qt UI,
gameplay expansion, asset workflow source truth, raw full CTest, heavy stress,
and real transport proof.
