# Saga Generated Files Policy

Status: Policy checkpoint before Hedef 2 Block B and later tool/report phases.

This document defines source truth, generated artifacts, package inclusion, and
stale artifact behavior. It does not move files, rewrite generated output, or
change tool behavior.

## Source-Controlled Truth

The following are source truth when checked into the repository or project:

- `.sagaproj` project files;
- user scripts and sample source files;
- launch, package, view, and tool profiles;
- architecture, product, roadmap, testing, and developer docs;
- stable sample project source and manifests.

Source truth is authored and reviewed directly. Generated artifacts may support
it, but must not replace it as canonical input.

## Generated Artifacts

The following paths are generated artifact locations by policy:

- `Build/SagaScript`;
- `Build/Reports`;
- `Build/SmallTeamAlpha`.

Generated artifacts may include analysis reports, source maps, projection
reports, node metadata, runtime bindings, patch previews, launch reports,
package evidence, diagnostics summaries, and gate reports.

## Collaboration Metadata

`.saga/collaboration` is source truth only for durable collaboration metadata
such as workspace identity, durable locks, durable comments, or durable review
metadata when a later phase explicitly stores those records there.

Generated collaboration session, presence, dashboard, and report outputs remain
generated. They must not become canonical source simply because they describe a
collaboration state.

## Package Inclusion

Generated artifacts may be included in a package as evidence when all of these
are true:

- the artifact is marked or documented as generated;
- the producing command is known;
- the artifact is reproducible from source truth and declared inputs;
- stale or missing source evidence is reported explicitly.

Packaged generated artifacts are evidence snapshots. They are not canonical
source files for future authoring.

## Stale or Missing Behavior

Tools must not silently pass when required generated artifacts are stale,
missing, malformed, or produced from different source hashes.

Required behavior:

- report stale artifacts with explicit diagnostics;
- report missing required artifacts with explicit diagnostics;
- keep unknown freshness separate from fresh status;
- fail gates that require fresh artifacts;
- avoid replacing missing evidence with optimistic defaults.

## Phase Boundary

Phase 69-75 artifacts must follow this policy. Later editor, preview,
packaging, and collaboration reports must continue to distinguish source truth
from generated evidence.
