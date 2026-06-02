# StarterArena Sample Definition

`StarterArena` is a future sample definition for project metadata validation.
In this batch it is only a `.sagaproj` specification sample.

This sample is not playable, not packaged, and not runtime-backed. It contains
no scenes, assets, scripts, launch profiles, package profiles, gameplay code,
runtime smoke evidence, or fake sample content.

The tracked directories exist only because the current project schema validates
the diagnostics and generated report paths:

- `Diagnostics`
- `Build/Reports`

Expected focused check:

```sh
Tools/SagaProjectKit/sagaproject validate --project samples/StarterArena/StarterArena.sagaproj --out /tmp/starter_arena_validate.json
```
