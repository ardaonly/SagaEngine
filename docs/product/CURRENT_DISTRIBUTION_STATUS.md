# Current Distribution Status

SagaEngine is not currently a public product release.

The safe distribution model today is a source checkout used by developers who
understand the repo's current limits. Build and verification commands vary by
local environment and subsystem.

## What Exists

- The repository contains engine/toolchain source code and focused local tests.
- Some tools and native targets can be built and verified locally.
- Distribution staging exists for selected artifacts.
- A limited Linux technical-preview archive can be produced and smoke-checked.
- Detailed candidate status is documented in
  [Technical Preview Candidate Status](TECHNICAL_PREVIEW_CANDIDATE.md).

## What To Avoid

- Do not call this production-ready.
- Do not call it beta or release-candidate software.
- Do not present it as a finished editor, full game engine, public SDK, or
  complete packaging/publishing pipeline.
- Do not present the technical-preview archive as a verified final release.
- Do not claim production networking, cloud, security, editor workflow, visual
  scripting, gameplay, or scale coverage.

## Generated Outputs

Generated and build outputs should not be committed unless they are stable,
deterministic, required as fixtures, and documented as source material. Most
generated reports and build products should remain ignored.
