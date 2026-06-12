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

| Data | Classification | Rule |
| --- | --- | --- |
| `.sagaproj` | Source of truth | Project-level manifest truth for supported project validation, resolution, profiles, and declared project roots. |
| Authored C# source | Source of truth | Canonical authored behavior source where current scripting/authoring docs define C# as canonical. |
| Visual blocks | Projection | A view/edit surface over accepted source or graph data unless a current architecture decision explicitly promotes a specific block format to source truth. |
| SDE source definitions | Source of truth for SDE-owned data | SDE source and schemas are authored inputs to deterministic validation/compile flows. |
| Generated code, source maps, projections, reports, and manifests | Generated artifacts | Outputs derived from declared inputs. They must remain reproducible or report stale/missing state. |
| Package manifests | Generated artifact and package evidence | Package/runtime consumption input, but not authoring source truth for scenes, entities, prefabs, scripts, or imported assets. |
| Runtime client state | Runtime execution state | Local visual/predictive state. It is not authoritative multiplayer truth. |
| Server state | Runtime authority truth | Authoritative multiplayer state during server execution. |
| Diagnostics reports | Evidence | Machine-readable evidence about tools, runtime, server, package, or validation state. They do not own the underlying state. |
| Test results | Evidence | Proof for the declared test scope only. |

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
