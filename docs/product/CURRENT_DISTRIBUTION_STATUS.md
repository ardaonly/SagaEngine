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
- That candidate archive is constrained to exactly `Saga`, `SagaEditor`, and
  `SagaRuntime`, plus the published `sagaproject`, `sagapack`, and `sagascript`
  CLIs. Developer/lab/probe/test executables are not default package artifacts.
- The limited Linux path may include a staged layout, archive, checksum,
  preflight report, distribution smoke report, and candidate aggregation report.

These artifacts are local evidence for supported paths. They are not a public
release channel and they are not current onboarding requirements.

The archive includes the Saga license, third-party notices, and license texts,
but no SBOM. The Python path does not yet stage native dependency closure, so it
does not establish clean-machine portability.

## What To Avoid

- Do not call this production-ready.
- Do not call it beta or release-candidate software.
- Do not present it as a finished editor, full game engine, public SDK, or
  complete packaging/publishing pipeline.
- Do not present the technical-preview archive as a verified final release.
- Do not claim production networking, cloud, security, editor workflow, visual
  scripting, gameplay, or scale coverage.
- Do not treat candidate/preflight/smoke report names as release vocabulary.

## Generated Outputs

Generated and build outputs should not be committed unless they are stable,
deterministic, required as fixtures, and documented as source material. Most
generated reports and build products should remain ignored.

Historical detailed candidate notes, if needed for context, belong under
`docs/internal/product-history/`, not in the current product surface.
