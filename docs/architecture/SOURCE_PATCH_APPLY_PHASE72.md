# Source Patch Apply Phase 72

Status: Hedef 2 Block B Phase 72 implementation evidence.

Phase 72 adds a bounded SagaScript source mutation path. The implemented
command is:

```sh
Tools/SagaScript/sagascript patch-apply --source <file-or-dir> --source-map <file> --request <file> --out <patch_apply_report.json> [--json]
```

## Implemented Scope

- SagaScript owns the write path.
- The editor does not write C# source.
- Only `ReplaceStringLiteral` is accepted.
- The request must include `operation`, `nodeId`, `baseSourceHash`,
  `expectedOldText`, and `replacement`.
- The target must resolve to one source-map node and one source file under the
  supplied `--source` input.
- The source hash must match before any write.
- The target must be a non-read-only string literal with editable projection
  compatibility and no opaque reason.
- The current UTF-8 byte span must decode to `expectedOldText`.

## Write And Recovery Model

`patch-apply` reads the original source bytes, replaces only the approved byte
span, creates a sibling `.saga-backup.<operationId>` file, writes a sibling
`.saga-tmp.<operationId>` temp file, and moves the temp file over the original
source in the same directory.

After replacement, SagaScript re-reads the source and verifies that the prefix,
replacement span, and suffix match the expected byte diff exactly. If the
post-apply verification fails, SagaScript restores the source from the backup
and reports `rollbackStatus = "Restored"` or `"Failed"`.

## Report Contract

`patch_apply_report.json` records:

- JSON envelope fields: `schemaVersion`, `tool`, `command`, `status`,
  `summary`, and `diagnostics`;
- patch identity: `operation`, `operationId`, `nodeId`, and `sourceFile`;
- source trust: `baseSourceHash`, `newSourceHash`, `sourceSpan`, `oldText`, and
  `newText`;
- write/recovery details: `backupPath`, `tempPath`, `rollbackStatus`, and
  `mutatesSource = true`;
- stale artifact disclosure: `source_map.json`, `projection_report.json`,
  `node_metadata.json`, and `runtime_bindings.json`;
- `artifactRegeneration = "NotPerformed"`.

## Non-Claims

Phase 72 does not add numeric, boolean, enum, argument, condition, insertion, or
removal patch operations. It does not regenerate source maps, projection
reports, node metadata, or runtime bindings after apply. It does not add
undo/redo, review UI, editor integration, runtime binding integration, publish
integration, or Hedef 2 Block C behavior.

## Evidence

Focused evidence:

```sh
nix-shell --run "dotnet build Tools/SagaScript/SagaScript.csproj --configuration Release --nologo --verbosity minimal -m:1"
nix-shell --run "python3 Tools/SagaScript/tests/test_sagascript_cli.py"
```

`test_sagascript_cli.py` covers successful exact-span replacement, outside-span
byte preservation, stale hash rejection, deterministic missing-file failure,
`expectedOldText` mismatch rejection, read-only/opaque/deferred/non-string
rejection, backup creation, rollback on forced post-check failure, preservation
of comments/whitespace/using order, stale artifact reporting without
regeneration, and the existing non-mutating preview behavior.
