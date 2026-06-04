# Phase 28 Known Limitations

- This phase is not verified.
- Local role/permission smoke is a no-UI report-only command.
- Role metadata is local report metadata and is not durable project truth.
- Permission metadata is represented as intent only and is not enforced.
- The report does not authenticate an actor, enforce access control, or provide
  an enterprise policy engine.
- The report does not mutate `.sagaproj`, scenes, scripts, SDE files, package
  outputs, diagnostics folders, report folders, or workspace files except for
  the caller-provided report output path.
- No full multiplayer collaboration, cloud workspace, real-time team editing,
  CRDT/OT, collaboration server, full team workspace, product beta, package
  readiness, or distribution readiness is implemented.
- No public product claim should be derived from this file.
