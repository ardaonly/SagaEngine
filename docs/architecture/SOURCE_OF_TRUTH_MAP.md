# Source Of Truth Map

> Status: Current architecture policy
> Scope: Source truth, projections, generated artifacts, runtime truth, and
> evidence

Every important Saga data shape must be classified as one of:

- source of truth;
- projection;
- generated artifact;
- evidence.

The same data must not be treated as all of these at once.

## Current Classifications



## Required Behavior

- Generated artifacts must not silently replace missing source truth.
- Projections must not drift into independent source truth without a current
  architecture decision.
- Package manifests must not be used as authoring truth for missing scenes,
  entities, prefabs, scripts, or source asset records.
- Runtime and server systems must consume validated inputs and report invalid,
  stale, or missing artifacts explicitly.
- Diagnostics reports must identify evidence scope and missing evidence instead
  of turning unknown state into passed state.

## Architecture Rule

Source-controlled inputs are authored and reviewed directly. Projections help
users inspect or edit them. Generated artifacts carry build, package, runtime,
or analysis output. Evidence records what was checked.

Do not collapse those roles.
