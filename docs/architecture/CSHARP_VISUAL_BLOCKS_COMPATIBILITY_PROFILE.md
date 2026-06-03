# C# Visual Blocks Compatibility Profile

## Status

Phase 13 status is `Implemented-Unverified`.

This document defines the first Saga-compatible C# profile used by
`sagascript compatibility-profile`. It is a compatibility and evidence
boundary only. It does not implement Visual Blocks, source patching, block
editor UI, runtime gameplay changes, server behavior, package output, or SDE
work.

## Canonical Source Rule

C# source remains canonical. Future Visual Blocks are source-preserving
projections over compatible C# source spans. The profile must not be used to
claim arbitrary C# conversion to blocks.

No block edit is implemented in this phase. Compatibility reports may expose
existing patch-oriented labels, but Phase 13 uses them only as profile metadata.

## Categories

| Category | Current report evidence | Meaning |
|---|---|---|
| Projectable | `ReadOnlyProjectable`, and existing `EditableByPatch` metadata for eligible literals | The construct can be represented as profile evidence with source spans. |
| Partially Projectable | A mix of `ReadOnlyProjectable` and `Opaque` constructs | Compatible source exists, but advanced regions must remain read-only. |
| Advanced Opaque | `Opaque` constructs with source spans and `editable: false` | Valid C# is preserved, but is not safely projected into editable blocks. |
| Unsupported | `Unsupported` constructs, `Invalid` constructs, or blocking diagnostics | The source is outside the current Saga-compatible authoring profile. |

## Authoring Levels

High-level C# describes gameplay or intention-oriented behavior with simple
parameters, pure helpers, and profile-visible method bodies. Current fixtures
use `SagaBehavior` metadata because that is the supported compatibility-profile
input surface.

Low-level C# describes explicit runtime, authority, diagnostics, or tool-facing
APIs that can still be represented as bounded profile metadata. Low-level
source is not treated as product gameplay proof by this phase.

Advanced C# remains valid C# when it compiles, but it is opaque to future block
editing. Examples include reflection-heavy code, LINQ/lambdas, complex
generics, arbitrary async/threading, unsafe code, unsupported IO, and control
flow outside the current profile.

## Fixture Contract

The Phase 13 fixtures live under
`Tools/SagaScript/tests/fixtures/csharp_blocks/`.

- `projectable/`: source that should pass with projectable profile constructs.
- `partially_projectable/`: source that should pass while preserving opaque
  source spans.
- `advanced_opaque/`: valid advanced source that should remain read-only.
- `unsupported/`: source that should fail or report unsupported/invalid
  constructs with diagnostics.

The fixture tests assert that `compatibility-profile` does not mutate source
bytes. The tests also assert that advanced regions remain opaque/read-only and
that unsupported evidence is explicit.

## Non-Claims

- No Visual Blocks projection UI exists in this phase.
- No source patching feature is introduced by this phase.
- No runtime gameplay behavior changes are introduced by this phase.
- No arbitrary C# to blocks conversion is claimed.
- No phase is `Verified`.
