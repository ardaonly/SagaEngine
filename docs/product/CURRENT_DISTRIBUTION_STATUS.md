# Current Distribution Status

SagaEngine is not currently a public product release.

The safe distribution model today is a source checkout used by developers who
understand the repo's current limits. Build and verification commands can vary
by local environment and by subsystem.

## Safe Claims

- The repository contains engine/toolchain source code and focused local tests.
- Some tools and native targets can be built and verified locally.
- Distribution staging exists for selected artifacts.
- A Linux technical-preview archive can be produced and smoke-checked with
  limitations, including a candidate evidence report that keeps Runtime and
  Editor workflow blockers explicit.

## Unsafe Claims

- Do not call this ready for production use.
- Do not call it a beta or release-candidate build.
- Do not present it as a finished editor, full game engine, public SDK, or
  complete packaging/publishing pipeline.
- Do not claim production networking, cloud, security, or scale readiness.
- Do not treat the technical-preview candidate report as production readiness,
  enterprise readiness, verified final release status, full distribution
  verification, full editor workflow, full Visual Blocks UI, or full gameplay
  readiness.

## Generated Outputs

Generated and build outputs should not be committed unless they are stable,
deterministic, required as fixtures, and documented as source material. Most
generated reports and build products should remain ignored.
