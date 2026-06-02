# Source Patch Review And Rollback Phase 73

Status: Hedef 2 Block B Phase 73 implementation evidence.

Phase 73 adds artifact-level diff, review, and strict rollback support around
the Phase 72 `ReplaceStringLiteral` apply path.

## Implemented Scope

- `patch-diff` previews one `ReplaceStringLiteral` request without mutating
  source.
- `patch-review` records `Approved` or `Rejected` review metadata for a passed
  diff report without applying a patch.
- `patch-rollback` is SagaScript-only and restores source only from a passed
  `patch_apply_report.json` and its backup file.
- Rollback validates current source hash against `newSourceHash`, backup hash
  against `baseSourceHash`, then restores through a same-directory temp file.
- Rollback verifies the restored source hash and exact backup bytes. If the
  post-check fails, it restores the pre-rollback source bytes.

## Report Contracts

`patch_diff_report.json` records the standard JSON envelope, operation identity,
source hash, proposed hash, source span, `oldText`, `newText`, byte diff,
unified text diff, `mutatesSource = false`, and
`artifactRegeneration = "NotPerformed"`.

`patch_review_report.json` records the standard JSON envelope, review decision,
reviewer, diff report path, operation identity, source hashes,
`mutatesSource = false`, `appliesPatch = false`, and
`artifactRegeneration = "NotPerformed"`.

`patch_rollback_report.json` records the standard JSON envelope, apply report
path, operation identity, source file, backup path, base hash, pre/post rollback
hashes, rollback status, `mutatesSource = true`,
`restoresExactPreviousBytes`, stale artifact disclosure, and
`artifactRegeneration = "NotPerformed"`.

## Non-Claims

Phase 73 does not add patch operations beyond `ReplaceStringLiteral`. It does
not add a broad undo/redo stack, redo mutation, editor UI, Qt UI, graph editing,
runtime binding integration, publish integration, artifact regeneration, Phase
74/75 implementation, Hedef 2 Block C, or Hedef 3 work.

## Evidence

Focused evidence:

```sh
nix-shell --run "dotnet build Tools/SagaScript/SagaScript.csproj --configuration Release --nologo --verbosity minimal -m:1"
nix-shell --run "python3 Tools/SagaScript/tests/test_sagascript_cli.py"
```

`test_sagascript_cli.py` covers non-mutating diff, diff rejection paths,
report-only review approval/rejection, review validation failures, strict
rollback restore, rollback source-hash and backup-hash rejection, missing or
failed apply report rejection, rollback post-check recovery to current bytes,
stale artifact reporting without regeneration, and Phase 72 regression
coverage.
