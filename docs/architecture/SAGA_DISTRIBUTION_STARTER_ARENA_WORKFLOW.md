# Saga Distribution StarterArena Workflow

Phase 37 status is `Implemented-Unverified`.

`scripts/smoke-linux-saga-dist` runs the first limited StarterArena workflow
smoke from the unpacked Linux distribution tree:

```txt
/tmp/saga_dist_smoke/Saga/
```

The smoke uses only files and executables extracted from `Saga.tar.zst`. It does
not use repository tools, repository build directories, or repository samples.

## Passing Commands

The required Phase 37 distribution-only commands are:

```bash
/tmp/saga_dist_smoke/Saga/tools/sagaproject validate \
  --project /tmp/saga_dist_smoke/Saga/samples/StarterArena/StarterArena.sagaproj \
  --out /tmp/saga_dist_smoke/starter_arena_validate.json
```

```bash
/tmp/saga_dist_smoke/Saga/tools/sagascript analyze \
  --source /tmp/saga_dist_smoke/Saga/samples/StarterArena/Scripts \
  --out /tmp/saga_dist_smoke/starter_arena_sagascript_analyze \
  --json
```

```bash
/tmp/saga_dist_smoke/Saga/bin/Saga \
  --workflow-smoke \
  --project /tmp/saga_dist_smoke/Saga/samples/StarterArena/StarterArena.sagaproj \
  --profile technical_preview \
  --workflow-report-out /tmp/saga_dist_smoke/starter_arena_product_shell_workflow_report.json
```

These commands provide project validation, a narrow SagaScript analysis pass, and
a no-UI Product Shell workflow report.

## Recorded Blockers

The packaged Runtime StarterArena smoke is recorded as blocked. The current
packaged `SagaRuntime` command enters normal client startup, attempts UDP
transport setup, and does not write the requested runtime smoke report.

The packaged Editor inspect command is recorded as blocked. The current
packaged `SagaEditor` reports:

```txt
SagaEditor: unknown argument '--inspect-project'
```

The Product Shell workflow report is report-only. Its internal developer-tree
command references are not executed by the distribution smoke and are not
evidence that those referenced workflows pass from the package.

## Non-Claims

This phase does not claim:

- production readiness;
- enterprise readiness;
- verified final release status;
- full distribution verification;
- full gameplay readiness;
- full editor workflow;
- full Visual Blocks UI;
- Runtime StarterArena workflow success from the packaged distribution;
- Editor inspect workflow success from the packaged distribution;
- any phase `Verified` status.
