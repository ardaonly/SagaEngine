# StarterArena Source Fixture

`StarterArena` currently provides project, scene, and C# source inputs for the
authoritative project and scripting tools. It is not a Runtime launch contract
or a finished playable sample.

The earlier sample-specific `SagaRuntime` modes and Product Launcher evidence
workflow were retired. Runtime consumes validated generated artifacts; it does
not parse these project sources or compile `GameRules.cs`.

Current source-level checks can be run without an editor:

```sh
Tools/SagaProjectKit/sagaproject validate \
  --project Samples/StarterArena/StarterArena.sagaproj \
  --out /tmp/starter-arena-project-validation.json

Tools/SagaScript/sagascript analyze \
  --source Samples/StarterArena/Scripts \
  --out /tmp/starter-arena-analysis \
  --json
```

Generated reports, manifests, assemblies, and caches belong outside this
directory. They are evidence or build output, never project source truth.
