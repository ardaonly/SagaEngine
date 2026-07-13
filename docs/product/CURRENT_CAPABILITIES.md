# Current Capabilities

> **Owner:** Product documentation, validated by subsystem owners
> **Audience:** Users and contributors evaluating the current repository
> **Update trigger:** Accepted build, test, runtime, package, or artifact evidence changes a capability claim
> **Authority:** Current evidence summary; live source and accepted evidence take precedence

> Status: Current repository summary
> Scope: Bounded capabilities supported by current code and focused evidence

This page lists what the repository contains today. It is not a release note
for a finished engine and it is not a roadmap.

SagaEngine's selected future specialization is documented in
[SAGA_MMO_GENRE_FOCUS.md](SAGA_MMO_GENRE_FOCUS.md). That direction must not be
read as a current implementation claim.


## Evidence Traceability

The final merged version of this document must keep every positive current
claim traceable to at least one current source of evidence, such as:

- an owning build target;
- a focused automated test or suite;
- a documented runtime smoke path;
- a verified package or artifact record;
- a current architecture summary that names its limitations.

The evidence reference may live beside the claim or in a maintained evidence
index, but it must be discoverable from this document.

A class, file, report, or tool existing in the repository proves presence only.
It does not by itself prove that the behavior works, is integrated, is
supported, or is product-ready.

Exact commands, commit identities, platforms, configurations, and known
limitations belong in the accepted evidence record. When no current evidence
pointer exists, use presence-oriented wording such as `contains` or `exists`
rather than `supports`, `provides`, `works`, or `is complete`.

## Engine And Server Code

The repository contains C++ engine, runtime, server, diagnostics, editor, app,
and test code. Some focused targets build and run locally. A complete
end-to-end game creation workflow is not available yet.

The safest current product claim is that SagaEngine has bounded engine and
toolchain foundations with explicit source-truth and report/evidence
discipline. A capability listed here should not be read as product completion
unless another current product document says so directly.

Focused server-authority, command-validation, ownership, movement, simulation,
serialization, graphics, and diagnostics code exists. This does not establish
a complete multiplayer, replication, persistence, scale, or MMO product loop.

## Project Tooling

SagaProjectKit supports project-manifest validation, resolution, and
doctor-style reporting for supported inputs.

The `.sagaproj` schema is early and should be treated as a project/tooling
contract rather than a stable public SDK.

The `Saga` Product Launcher Foundation treats schema-0 `.sagaproj` as canonical
project input. A directory may resolve one immediate `.sagaproj`; legacy
`saga.project.json` remains a compatibility fallback only. The launcher keeps
at most twelve recent entries in platform application configuration, preserves
missing entries as disabled, and never auto-launches the Editor when a project
is selected.

Typed launcher state exposes explicit Editor handoff, authoritative
`sagaproject` validation, bounded StarterArena Runtime smoke/playable actions,
the existing first-playable check, known report status, and read-only
distribution identity. Executable paths and argument vectors are owned by the
allowlisted action executor, not the UI. Long actions run asynchronously, only
one executable action may run at once, and cancellation is cooperative and
bounded.
The CLI-only `--launcher-distribution-report <path>` override selects a known
schema-2 distribution evidence input for the read-only panel. It does not add
a package, archive, signing, or publishing action.

## Scripting And Authoring Tooling

The `SagaScript` toolchain contains selected C# analysis, compilation, source
discovery, artifact, and source-authoring workflows.

`SagaScript` is the toolchain name in this document. It is not a second
canonical gameplay language or an independent behavior source of truth.

This is useful infrastructure. It is not a complete visual scripting product,
a complete managed runtime integration, or arbitrary C# to blocks round trip.

## Packaging And Launch-Adjacent Tooling

SagaPackager and related tooling can inspect supported package and profile
inputs and produce local reports.

Selected staging, archive, checksum, profile, and smoke paths exist as bounded
local evidence. They do not establish a complete publishing, installation,
update, signing, or production distribution system.

## Rendering And Runtime Foundations

The repository contains Diligent-based rendering code, vendor-neutral public
graphics vocabulary, focused native resource paths, diagnostics, architecture
tests, and GPU-focused evidence for selected configurations.

These foundations do not establish production renderer readiness, complete
cross-vendor portability, a complete material/asset pipeline, device-loss
recovery, MMO-scale world streaming, or a finished runtime game loop.

## Diagnostics And Verification

The repository contains diagnostics primitives, focused tests, and internal
reporting tools. These help developers review bounded subsystem behavior.

They do not establish:

- full raw CTest health;
- complete heavy stress or load coverage;
- production networking or soak behavior;
- production observability;
- release readiness;
- product-level enforcement unless the relevant current policy says so.

Reports are evidence outputs. They are not source truth, automatic enforcement,
or readiness guarantees.

## Samples

`samples/MultiplayerSandbox` is currently a deterministic fixture for project,
script, package, validation, and headless workflows. It should not be described
as a finished or playable multiplayer sample.

Other sample-like projects must be classified by their actual role: product
sample, tutorial sample, integration fixture, packaging fixture, architecture
fixture, or obsolete experiment.
