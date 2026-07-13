# Saga Distribution StarterArena Workflow

> Status: Distribution smoke evidence note

This document records a limited StarterArena workflow smoke from an unpacked
Linux distribution tree. It is not current product onboarding material and it
does not claim packaged runtime/editor workflow completion.

`scripts/smoke-linux-saga-dist` runs the first limited StarterArena workflow
smoke from the unpacked Linux distribution tree:

```txt
/tmp/saga_dist_smoke/Saga/
```

The smoke uses only files and executables extracted from `Saga.tar.zst`. It does
not use repository tools, repository build directories, or repository samples.

## Passing Commands

The required this document distribution-only commands are:

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

## Recorded Limitations

The packaged Runtime StarterArena command is optional bounded evidence; its
current result is recorded directly in the smoke report without a hardcoded
historical diagnosis. `SagaEditor` is executable-presence checked but is not
launched by headless archive smoke.

The Product Shell workflow report is report-only. Its internal developer-tree
command references are not executed by the distribution smoke and are not
evidence that those referenced workflows pass from the package.

## Non-Claims

This smoke evidence does not claim:

- production readiness;
- enterprise readiness;
- verified final release status;
- full distribution verification;
- full gameplay readiness;
- full editor workflow;
- full Visual Blocks UI;
- Runtime StarterArena workflow success unless the current smoke report records it;
- Editor GUI or inspect workflow success;
- any historical verified status.
