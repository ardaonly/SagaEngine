# Generated Artifact Freshness Contract

## Status

Phase 136 report-only contract for Hedef 4. Generated artifacts are checked as
freshness evidence only. SagaProjectKit does not regenerate them and does not
edit source files.

## Inputs

The freshness gate reads a source truth inventory report and each listed
generated artifact. Scene source truth provides the expected source hash through
`expectedSourceHash`. Generated artifacts may provide the observed input hash as
`sourceHash` or `inputHash`.

## Status Values

`Fresh` means the expected source hash and generated artifact hash match.

`Stale` means both hashes exist but do not match.

`MissingSource` means the source truth record does not provide an expected hash.

`MissingGenerated` means the generated artifact path is absent.

`UnknownFreshness` means the generated artifact exists but does not expose
`sourceHash` or `inputHash`.

`Invalid` means the artifact claims canonical source status or uses an invalid
project path.

## Report

`sagaproject generated-freshness-gate` emits
`generated_artifact_freshness_report.json` with:

- `generatedArtifacts`
- `sourceInputs`
- `freshnessStatus`
- `diagnostics`
- `localOnly=true`
- `networkExposure="None"`
- `mutatesSource=false`
- `enforcement="ReportOnly"`
