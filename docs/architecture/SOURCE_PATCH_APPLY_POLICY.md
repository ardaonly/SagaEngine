# Source Patch Application Policy

Status: Source authoring write policy for SagaScript-owned patch application.

This document defines source patch ownership and write rules for future
SagaScript and SagaWeaver work.

## Ownership Decision

SagaScript owns future source patch application.

The editor may request, preview, display, and review patch operations through
versioned artifacts. The editor must not write C# source files directly and
must not bypass SagaScript for source changes.

## Preview Versus Application

Preview remains the default behavior for review. `patch_preview.json` artifacts
are review evidence only.

The explicit SagaScript-owned apply path currently supports only the bounded
`ReplaceStringLiteral` operation. It is not an editor action, not a general
source rewriter, and not a multi-file patch system.

## Required Write Rules

Future source-changing behavior must satisfy all of these rules:

- reject stale source hashes before writing;
- reject unsupported or opaque regions;
- mutate only the exact approved source span;
- write through an atomic temporary-file and rename sequence;
- create a backup before replacing source;
- roll back on deterministic failure where rollback is possible;
- preserve all bytes outside the approved span;
- emit deterministic failure reports;
- provide compile or check evidence after the write;
- record diagnostics with the source path, expected hash, actual hash, span, and
  operation id.

## Forbidden Write Behavior

Future source-changing behavior must not:

- rewrite whole files;
- normalize formatting;
- reorder usings, members, methods, or statements;
- delete comments outside the approved span;
- regenerate classes or methods;
- modify unsupported or opaque regions;
- apply edits through editor-side direct writes.

## Failure Rules

Failures must be explicit. A missing file, stale hash, unsupported operation,
opaque region, invalid span, write failure, failed rollback, or failed compile
check must produce a report with `status = "Failed"` and diagnostics. Silent
success is not allowed.

## Review And Rollback Boundary

Review, diff, and rollback behavior must remain model/report based unless a
later source-authoring contract proves a narrower write path. Additional
operations, undo/review UI, source diff panels, and publish/runtime integration
remain outside this policy. No editor path may mutate C# source directly.
