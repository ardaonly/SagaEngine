# StarterArena Sample Definition

`StarterArena` is a future sample definition for project metadata validation.
In this batch it is only a `.sagaproj` specification sample.

This sample is not playable, not packaged, and not runtime-backed. It contains
no scenes, assets, scripts, launch profiles, package profiles, gameplay code,
runtime smoke evidence, or fake sample content.

Phase 10 attempted to find a current-supported runtime launch path for
StarterArena and found a blocker instead. The current `SagaRuntime` command
line does not accept a project or scene input, and `SagaLaunchLab` exposes only
server/headless launch commands. StarterArena must remain metadata-only until a
real runtime entrypoint can consume the project and scene source truth.

The tracked directories exist only because the current project schema validates
the diagnostics and generated report paths:

- `Diagnostics`
- `Build/Reports`

Expected focused check:

```sh
Tools/SagaProjectKit/sagaproject validate --project samples/StarterArena/StarterArena.sagaproj --out /tmp/starter_arena_validate.json
```

Blocked Phase 10 acceptance notes are tracked in `ACCEPTANCE.md`. Known
limitations are tracked in `KNOWN_LIMITATIONS.md`.
