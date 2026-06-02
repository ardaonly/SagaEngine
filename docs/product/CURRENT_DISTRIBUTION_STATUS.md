# Current Distribution Status

SagaEngine is not currently a public product release.

The safe distribution model today is a source checkout used by developers who
understand the repo's current limits. Build and verification commands can vary
by local environment and by subsystem.

## Safe Claims

- The repository contains engine/toolchain source code and focused local tests.
- Some tools and native targets can be built and verified locally.
- Distribution staging exists for selected artifacts.

## Unsafe Claims

- Do not call this ready for production use.
- Do not call it a beta or release-candidate build.
- Do not present it as a finished editor, full game engine, public SDK, or
  complete packaging/publishing pipeline.
- Do not claim production networking, cloud, security, or scale readiness.

## Generated Outputs

Generated and build outputs should not be committed unless they are stable,
deterministic, required as fixtures, and documented as source material. Most
generated reports and build products should remain ignored.
